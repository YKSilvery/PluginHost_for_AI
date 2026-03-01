/**
 * Headless VST3 Host — Main Entry Point
 *
 * A command-line / JSON-driven tool for automated audio plugin testing.
 * Designed to be invoked by AI agents for vibe-coding debug loops.
 *
 * Usage:
 *   ./HeadlessPluginHost --file commands.json          Read commands from file
 *   ./HeadlessPluginHost --stdin                       Read commands from stdin
 *   ./HeadlessPluginHost --plugin <path> [options]     Quick one-shot mode
 *   ./HeadlessPluginHost --help                        Show help
 */

#include <juce_core/juce_core.h>
#include <juce_events/juce_events.h>
#include "BatchProcessor.h"
#include "JsonHelper.h"
#include <iostream>
#include <cstdio>
#include <cstring>
#include <signal.h>

// =============================================================================
// Global crash handler for plugin-induced crashes
// =============================================================================
static volatile sig_atomic_t g_crashed = 0;
static int g_crashSignal = 0;

static void crashSignalHandler (int sig)
{
    g_crashed = 1;
    g_crashSignal = sig;

    // Try to output an error JSON to stdout before dying
    const char* sigName = "UNKNOWN";
    switch (sig)
    {
        case SIGSEGV: sigName = "SIGSEGV (Segmentation fault)"; break;
        case SIGABRT: sigName = "SIGABRT (Abort)"; break;
        case SIGFPE:  sigName = "SIGFPE (Floating-point exception)"; break;
        case SIGBUS:  sigName = "SIGBUS (Bus error)"; break;
        default: break;
    }

    // Write minimal error JSON directly (no heap allocation in signal handler)
    char buf[512];
    snprintf (buf, sizeof (buf),
              "{\"version\":\"1.0\",\"fatal_error\":true,"
              "\"signal\":%d,\"signal_name\":\"%s\","
              "\"message\":\"Plugin caused a fatal crash\"}\n",
              sig, sigName);
    // Use write() — async-signal-safe
    [[maybe_unused]] auto _ = write (STDOUT_FILENO, buf, strlen (buf));

    // Re-raise with default handler to get core dump
    signal (sig, SIG_DFL);
    raise (sig);
}

static void installCrashHandlers()
{
    signal (SIGSEGV, crashSignalHandler);
    signal (SIGABRT, crashSignalHandler);
    signal (SIGFPE,  crashSignalHandler);
    signal (SIGBUS,  crashSignalHandler);
}

// =============================================================================
// Help text
// =============================================================================
static void printHelp()
{
    std::cerr <<
        "Headless VST3 Host v1.0 — AI-driven audio plugin debugging tool\n"
        "\n"
        "Usage:\n"
        "  HeadlessPluginHost --file <commands.json>      Execute batch from file\n"
        "  HeadlessPluginHost --stdin                      Read JSON batch from stdin\n"
        "  HeadlessPluginHost --plugin <path.vst3> [opts]  Quick single-plugin test\n"
        "  HeadlessPluginHost --help                       Show this help\n"
        "\n"
        "Quick mode options:\n"
        "  --signal <type>      impulse|step|sine|sweep|noise (default: impulse)\n"
        "  --frequency <Hz>     Sine frequency (default: 440)\n"
        "  --duration <ms>      Signal duration (default: 1000)\n"
        "  --sample-rate <Hz>   Sample rate (default: 44100)\n"
        "  --block-size <n>     Block size (default: 512)\n"
        "  --screenshot <path>  Capture UI to PNG\n"
        "\n"
        "All output is JSON on stdout. Diagnostics go to stderr.\n";
}

// =============================================================================
// Quick one-shot mode
// =============================================================================
static juce::var buildQuickCommands (const juce::StringArray& args)
{
    using namespace JsonHelper;

    juce::String pluginPath;
    juce::String signalType  = "impulse";
    double frequency         = 440.0;
    double durationMs        = 1000.0;
    double sampleRate        = 44100.0;
    int blockSize            = 512;
    juce::String screenshot;

    for (int i = 0; i < args.size(); ++i)
    {
        if (args[i] == "--plugin"      && i + 1 < args.size()) pluginPath  = args[++i];
        if (args[i] == "--signal"      && i + 1 < args.size()) signalType  = args[++i];
        if (args[i] == "--frequency"   && i + 1 < args.size()) frequency   = args[++i].getDoubleValue();
        if (args[i] == "--duration"    && i + 1 < args.size()) durationMs  = args[++i].getDoubleValue();
        if (args[i] == "--sample-rate" && i + 1 < args.size()) sampleRate  = args[++i].getDoubleValue();
        if (args[i] == "--block-size"  && i + 1 < args.size()) blockSize   = args[++i].getIntValue();
        if (args[i] == "--screenshot"  && i + 1 < args.size()) screenshot  = args[++i];
    }

    auto commands = makeArray();

    // 1. Load plugin
    {
        auto cmd = makeObject();
        set (cmd, "action",      "load_plugin");
        set (cmd, "plugin_path", pluginPath);
        set (cmd, "sample_rate", sampleRate);
        set (cmd, "block_size",  blockSize);
        append (commands, cmd);
    }

    // 2. List parameters
    {
        auto cmd = makeObject();
        set (cmd, "action", "list_parameters");
        append (commands, cmd);
    }

    // 3. Generate and process
    {
        auto cmd = makeObject();
        set (cmd, "action",      "generate_and_process");
        set (cmd, "signal_type", signalType);
        set (cmd, "duration_ms", durationMs);
        set (cmd, "frequency",   frequency);
        set (cmd, "start_freq",  20.0);
        set (cmd, "end_freq",    20000.0);
        set (cmd, "amplitude",   1.0);
        append (commands, cmd);
    }

    // 4. Detailed analysis
    {
        auto cmd = makeObject();
        set (cmd, "action",           "analyze_output");
        set (cmd, "include_spectrum", false);
        set (cmd, "fft_order",        12);
        set (cmd, "max_peaks",        10);
        append (commands, cmd);
    }

    // 5. Screenshot (if requested)
    if (screenshot.isNotEmpty())
    {
        auto cmd = makeObject();
        set (cmd, "action",      "capture_ui");
        set (cmd, "output_path", screenshot);
        set (cmd, "width",       800);
        set (cmd, "height",      600);
        append (commands, cmd);
    }

    auto batch = makeObject();
    set (batch, "commands", commands);
    return batch;
}

// =============================================================================
// Main
// =============================================================================
int main (int argc, char* argv[])
{
    // Install crash handlers FIRST
    installCrashHandlers();

    // Initialize JUCE (message manager, etc.)
    juce::ScopedJuceInitialiser_GUI juceInit;

    juce::StringArray args;
    for (int i = 1; i < argc; ++i)
        args.add (argv[i]);

    // --help
    if (args.contains ("--help") || args.contains ("-h") || args.isEmpty())
    {
        printHelp();
        return args.contains ("--help") || args.contains ("-h") ? 0 : 1;
    }

    juce::var batchJson;
    juce::String parseError;

    // --file mode
    if (args.contains ("--file"))
    {
        int idx = args.indexOf ("--file");
        if (idx + 1 >= args.size())
        {
            fprintf (stderr, "Error: --file requires a path argument\n");
            return 1;
        }

        juce::File jsonFile (args[idx + 1]);
        if (! jsonFile.existsAsFile())
        {
            fprintf (stderr, "Error: File not found: %s\n", args[idx + 1].toRawUTF8());
            return 1;
        }

        batchJson = JsonHelper::parseJson (jsonFile.loadFileAsString(), parseError);
    }
    // --stdin mode
    else if (args.contains ("--stdin"))
    {
        std::string input;
        std::string line;
        while (std::getline (std::cin, line))
            input += line + "\n";

        batchJson = JsonHelper::parseJson (juce::String (input), parseError);
    }
    // --plugin quick mode
    else if (args.contains ("--plugin"))
    {
        batchJson = buildQuickCommands (args);
    }
    else
    {
        // Try to interpret the first argument as a JSON file
        juce::File maybeFile (args[0]);
        if (maybeFile.existsAsFile() && maybeFile.getFileExtension().equalsIgnoreCase (".json"))
        {
            batchJson = JsonHelper::parseJson (maybeFile.loadFileAsString(), parseError);
        }
        else
        {
            fprintf (stderr, "Error: unrecognized arguments. Use --help for usage.\n");
            return 1;
        }
    }

    if (parseError.isNotEmpty())
    {
        fprintf (stderr, "JSON parse error: %s\n", parseError.toRawUTF8());
        return 1;
    }

    // Execute the batch
    fprintf (stderr, "[HeadlessHost] Starting batch execution...\n");
    fflush (stderr);

    BatchProcessor processor;
    auto result = processor.executeBatch (batchJson);

    // Output result JSON to stdout (clean, no mixing)
    juce::String jsonOut = JsonHelper::toJsonString (result, false) + "\n";
    auto utf8 = jsonOut.toRawUTF8();
    fwrite (utf8, 1, strlen (utf8), stdout);
    fflush (stdout);

    fprintf (stderr, "[HeadlessHost] Done.\n");
    fflush (stderr);
    return 0;
}
