#include "BatchProcessor.h"
#include "JsonHelper.h"

// ============================================================================
BatchProcessor::BatchProcessor()
{
    host.initialize();
}

BatchProcessor::~BatchProcessor() = default;

// ============================================================================
juce::var BatchProcessor::executeBatch (const juce::var& batchJson)
{
    using namespace JsonHelper;

    auto resultsArr = makeArray();

    // Accept either { "commands": [...] } or a bare array [...]
    const juce::var* commandsArray = nullptr;

    if (batchJson.isArray())
        commandsArray = &batchJson;
    else if (batchJson.hasProperty ("commands"))
        commandsArray = batchJson.getProperty ("commands", {}).getArray() != nullptr
                            ? &batchJson["commands"]
                            : nullptr;

    if (commandsArray == nullptr || ! commandsArray->isArray())
    {
        auto errResult = makeError ("batch", "Expected a JSON object with a 'commands' array or a bare array");
        append (resultsArr, errResult);
        auto root = makeObject();
        set (root, "version", "1.1");
        set (root, "results", resultsArr);
        return root;
    }

    auto* arr = commandsArray->getArray();
    for (int i = 0; i < arr->size(); ++i)
    {
        auto result = executeCommand (arr->getReference (i), i);
        set (result, "command_index", i);
        append (resultsArr, result);

        // If a command fails, continue with next (don't abort batch)
    }

    auto root = makeObject();
    set (root, "version", "1.1");
    set (root, "total_commands", arr->size());
    set (root, "results", resultsArr);
    return root;
}

// ============================================================================
juce::var BatchProcessor::executeCommand (const juce::var& command, int index)
{
    using namespace JsonHelper;

    if (! command.isObject())
        return makeError ("unknown", "Command #" + juce::String (index) + " is not a JSON object");

    juce::String action = command.getProperty ("action", "").toString().toLowerCase();

    if (action.isEmpty())
        return makeError ("unknown", "Command #" + juce::String (index) + " has no 'action' field");

    try
    {
        if (action == "load_plugin")          return handleLoadPlugin (command);
        if (action == "unload_plugin")        return handleUnloadPlugin (command);
        if (action == "get_plugin_info")      return handleGetPluginInfo (command);
        if (action == "list_parameters")      return handleListParameters (command);
        if (action == "set_parameter")        return handleSetParameter (command);
        if (action == "get_parameter")        return handleGetParameter (command);
        if (action == "generate_and_process") return handleGenerateAndProcess (command);
        if (action == "analyze_output")       return handleAnalyzeOutput (command);
        if (action == "capture_ui")           return handleCaptureUI (command);
        if (action == "get_parameter_layout") return handleGetParameterLayout (command);
        if (action == "open_editor")          return handleOpenEditor (command);
        if (action == "close_editor")         return handleCloseEditor (command);
        if (action == "test_plugin_lifecycle") return handleTestPluginLifecycle (command);
        if (action == "test_editor_lifecycle") return handleTestEditorLifecycle (command);
        if (action == "send_midi_and_process") return handleSendMidiAndProcess (command);
        if (action == "render_synth_note")    return handleRenderSynthNote (command);
        if (action == "analyze_stft")         return handleAnalyzeSTFT (command);
        if (action == "analyze_time_domain")  return handleAnalyzeTimeDomain (command);
        if (action == "analyze_loudness")     return handleAnalyzeLoudness (command);

        return makeError (action, "Unknown action: " + action);
    }
    catch (const std::exception& e)
    {
        return makeError (action, juce::String ("Exception: ") + e.what());
    }
    catch (...)
    {
        return makeError (action, "Unknown exception during command execution");
    }
}

// ============================================================================
juce::var BatchProcessor::handleLoadPlugin (const juce::var& cmd)
{
    juce::String path   = cmd.getProperty ("plugin_path", "").toString();
    double sampleRate   = static_cast<double> (cmd.getProperty ("sample_rate", 44100.0));
    int blockSize       = static_cast<int> (cmd.getProperty ("block_size", 512));

    if (path.isEmpty())
        return JsonHelper::makeError ("load_plugin", "Missing 'plugin_path'");

    lastSampleRate = sampleRate;
    return host.loadPlugin (path, sampleRate, blockSize);
}

// ============================================================================
juce::var BatchProcessor::handleUnloadPlugin (const juce::var& /*cmd*/)
{
    host.unloadPlugin();
    lastOutputBuffer = {};
    return JsonHelper::makeSuccess ("unload_plugin");
}

// ============================================================================
juce::var BatchProcessor::handleGetPluginInfo (const juce::var& /*cmd*/)
{
    return host.getPluginInfo();
}

// ============================================================================
juce::var BatchProcessor::handleListParameters (const juce::var& /*cmd*/)
{
    return host.listParameters();
}

// ============================================================================
juce::var BatchProcessor::handleSetParameter (const juce::var& cmd)
{
    juce::String id = cmd.getProperty ("parameter_id", "").toString();
    if (id.isEmpty())
        id = cmd.getProperty ("index", "").toString();
    float value = static_cast<float> (cmd.getProperty ("value", 0.0));

    if (id.isEmpty())
        return JsonHelper::makeError ("set_parameter", "Missing 'parameter_id' or 'index'");

    return host.setParameter (id, value);
}

// ============================================================================
juce::var BatchProcessor::handleGetParameter (const juce::var& cmd)
{
    juce::String id = cmd.getProperty ("parameter_id", "").toString();
    if (id.isEmpty())
        id = cmd.getProperty ("index", "").toString();

    if (id.isEmpty())
        return JsonHelper::makeError ("get_parameter", "Missing 'parameter_id' or 'index'");

    return host.getParameter (id);
}

// ============================================================================
juce::var BatchProcessor::handleGenerateAndProcess (const juce::var& cmd)
{
    using namespace JsonHelper;

    if (! host.isPluginLoaded())
        return makeError ("generate_and_process", "No plugin loaded");

    double sampleRate = host.getSampleRate();
    int numChannels   = std::max (host.getNumInputChannels(), host.getNumOutputChannels());
    if (numChannels <= 0) numChannels = 2;

    // Generate input signal from command parameters
    auto inputBuffer = SignalGenerator::fromJson (cmd, sampleRate, numChannels);

    if (inputBuffer.getNumSamples() == 0)
        return makeError ("generate_and_process", "Generated signal has 0 samples — check parameters");

    // Process through plugin
    lastOutputBuffer = host.processAudio (inputBuffer);
    lastSampleRate   = sampleRate;

    // Quick inline analysis
    auto analysis = AudioAnalyzer::analyze (lastOutputBuffer, sampleRate);
    auto analysisJson = AudioAnalyzer::toJson (analysis, false);

    auto data = makeObject();
    set (data, "input_samples",  inputBuffer.getNumSamples());
    set (data, "output_samples", lastOutputBuffer.getNumSamples());
    set (data, "output_channels",lastOutputBuffer.getNumChannels());
    set (data, "signal_type",    cmd.getProperty ("signal_type", "unknown"));
    set (data, "analysis",       analysisJson);

    return makeSuccess ("generate_and_process", data);
}

// ============================================================================
juce::var BatchProcessor::handleAnalyzeOutput (const juce::var& cmd)
{
    using namespace JsonHelper;

    if (lastOutputBuffer.getNumSamples() == 0)
        return makeError ("analyze_output", "No output buffer available — run generate_and_process first");

    bool includeSpectrum = static_cast<bool> (cmd.getProperty ("include_spectrum", false));
    int fftOrder         = static_cast<int> (cmd.getProperty ("fft_order", 12));
    int maxPeaks         = static_cast<int> (cmd.getProperty ("max_peaks", 10));

    fftOrder = juce::jlimit (8, 16, fftOrder);  // 256 .. 65536

    auto analysis = AudioAnalyzer::analyze (lastOutputBuffer, lastSampleRate, fftOrder, maxPeaks);
    auto analysisJson = AudioAnalyzer::toJson (analysis, includeSpectrum);

    auto data = makeObject();
    set (data, "num_samples",  lastOutputBuffer.getNumSamples());
    set (data, "num_channels", lastOutputBuffer.getNumChannels());
    set (data, "sample_rate",  lastSampleRate);
    set (data, "analysis",     analysisJson);

    return makeSuccess ("analyze_output", data);
}

// ============================================================================
juce::var BatchProcessor::handleCaptureUI (const juce::var& cmd)
{
    juce::String outputPath = cmd.getProperty ("output_path", "screenshot.png").toString();
    int width               = static_cast<int> (cmd.getProperty ("width", 800));
    int height              = static_cast<int> (cmd.getProperty ("height", 600));

    return OffscreenRenderer::captureSnapshot (host.getPluginInstance(), outputPath,
                                               width, height, host.getActiveEditor());
}

// ============================================================================
juce::var BatchProcessor::handleGetParameterLayout (const juce::var& cmd)
{
    int width  = static_cast<int> (cmd.getProperty ("width", 800));
    int height = static_cast<int> (cmd.getProperty ("height", 600));

    return OffscreenRenderer::getParameterLayout (host.getPluginInstance(), width, height,
                                                   host.getActiveEditor());
}

// ============================================================================
// New: UI Lifecycle
// ============================================================================
juce::var BatchProcessor::handleOpenEditor (const juce::var& /*cmd*/)
{
    return host.openEditor();
}

juce::var BatchProcessor::handleCloseEditor (const juce::var& /*cmd*/)
{
    return host.closeEditor();
}

// ============================================================================
// New: Lifecycle Stress Testing
// ============================================================================
juce::var BatchProcessor::handleTestPluginLifecycle (const juce::var& cmd)
{
    juce::String path = cmd.getProperty ("plugin_path", "").toString();
    double sampleRate = static_cast<double> (cmd.getProperty ("sample_rate", 44100.0));
    int blockSize     = static_cast<int> (cmd.getProperty ("block_size", 512));
    int iterations    = static_cast<int> (cmd.getProperty ("iterations", 5));

    if (path.isEmpty())
        return JsonHelper::makeError ("test_plugin_lifecycle", "Missing 'plugin_path'");

    iterations = juce::jlimit (1, 100, iterations);
    return host.testPluginLifecycle (path, sampleRate, blockSize, iterations);
}

juce::var BatchProcessor::handleTestEditorLifecycle (const juce::var& cmd)
{
    int iterations   = static_cast<int> (cmd.getProperty ("iterations", 5));
    int delayMs      = static_cast<int> (cmd.getProperty ("delay_between_ms", 50));

    iterations = juce::jlimit (1, 100, iterations);
    return host.testEditorLifecycle (iterations, delayMs);
}

// ============================================================================
// New: Synth / MIDI Support
// ============================================================================
juce::var BatchProcessor::handleSendMidiAndProcess (const juce::var& cmd)
{
    using namespace JsonHelper;

    if (! host.isPluginLoaded())
        return makeError ("send_midi_and_process", "No plugin loaded");

    double durationMs = static_cast<double> (cmd.getProperty ("duration_ms", 1000.0));
    double sampleRate = host.getSampleRate();
    int totalSamples  = static_cast<int> (sampleRate * durationMs / 1000.0);

    if (totalSamples <= 0)
        return makeError ("send_midi_and_process", "Invalid duration");

    // Parse MIDI events from JSON
    juce::MidiBuffer midiBuffer;
    auto eventsVar = cmd.getProperty ("midi_events", juce::var());

    if (eventsVar.isArray())
    {
        auto* eventsArr = eventsVar.getArray();
        for (int i = 0; i < eventsArr->size(); ++i)
        {
            auto& ev = eventsArr->getReference (i);
            juce::String type  = ev.getProperty ("type", "note_on").toString().toLowerCase();
            int channel        = static_cast<int> (ev.getProperty ("channel", 1));
            int note           = static_cast<int> (ev.getProperty ("note", 60));
            float vel          = static_cast<float> (ev.getProperty ("velocity", 0.8));
            double timeMs      = static_cast<double> (ev.getProperty ("time_ms", 0.0));
            int samplePos      = static_cast<int> (sampleRate * timeMs / 1000.0);
            samplePos          = juce::jlimit (0, totalSamples - 1, samplePos);

            juce::uint8 velByte = static_cast<juce::uint8> (juce::jlimit (0.0f, 1.0f, vel) * 127.0f);

            if (type == "note_on")
                midiBuffer.addEvent (juce::MidiMessage::noteOn (channel, note, velByte), samplePos);
            else if (type == "note_off")
                midiBuffer.addEvent (juce::MidiMessage::noteOff (channel, note, (juce::uint8) 0), samplePos);
            else if (type == "cc" || type == "control_change")
            {
                int ccNum  = static_cast<int> (ev.getProperty ("cc_number", 1));
                int ccVal  = static_cast<int> (ev.getProperty ("cc_value", 64));
                midiBuffer.addEvent (juce::MidiMessage::controllerEvent (channel, ccNum, ccVal), samplePos);
            }
            else if (type == "pitch_bend")
            {
                int bendVal = static_cast<int> (ev.getProperty ("bend_value", 8192));
                midiBuffer.addEvent (juce::MidiMessage::pitchWheel (channel, bendVal), samplePos);
            }
        }
    }

    // Process
    lastOutputBuffer = host.processWithMidi (totalSamples, midiBuffer);
    lastSampleRate   = sampleRate;

    // Analyze output
    auto analysis = AudioAnalyzer::analyze (lastOutputBuffer, sampleRate);
    auto analysisJson = AudioAnalyzer::toJson (analysis, false);

    auto data = makeObject();
    set (data, "output_samples",  lastOutputBuffer.getNumSamples());
    set (data, "output_channels", lastOutputBuffer.getNumChannels());
    set (data, "midi_events_count", midiBuffer.getNumEvents());
    set (data, "analysis",        analysisJson);

    return makeSuccess ("send_midi_and_process", data);
}

// ============================================================================
juce::var BatchProcessor::handleRenderSynthNote (const juce::var& cmd)
{
    using namespace JsonHelper;

    if (! host.isPluginLoaded())
        return makeError ("render_synth_note", "No plugin loaded");

    int noteNumber   = static_cast<int> (cmd.getProperty ("note", 60));
    float velocity   = static_cast<float> (cmd.getProperty ("velocity", 0.8));
    double durationMs = static_cast<double> (cmd.getProperty ("duration_ms", 1000.0));
    double releaseMs = static_cast<double> (cmd.getProperty ("release_ms", 100.0));

    noteNumber = juce::jlimit (0, 127, noteNumber);
    velocity   = juce::jlimit (0.0f, 1.0f, velocity);

    lastOutputBuffer = host.renderSynthNote (noteNumber, velocity, durationMs, releaseMs);
    lastSampleRate   = host.getSampleRate();

    if (lastOutputBuffer.getNumSamples() == 0)
        return makeError ("render_synth_note", "Render produced empty buffer");

    // Analyze output
    auto analysis = AudioAnalyzer::analyze (lastOutputBuffer, lastSampleRate);
    auto analysisJson = AudioAnalyzer::toJson (analysis, false);

    // Calculate expected frequency from MIDI note
    double expectedFreq = 440.0 * std::pow (2.0, (noteNumber - 69) / 12.0);

    auto data = makeObject();
    set (data, "note_number",      noteNumber);
    set (data, "velocity",         velocity);
    set (data, "duration_ms",      durationMs);
    set (data, "release_ms",       releaseMs);
    set (data, "expected_freq_hz", expectedFreq);
    set (data, "output_samples",   lastOutputBuffer.getNumSamples());
    set (data, "output_channels",  lastOutputBuffer.getNumChannels());
    set (data, "analysis",         analysisJson);

    return makeSuccess ("render_synth_note", data);
}

// ============================================================================
// v1.1: Advanced Analysis Commands
// ============================================================================
juce::var BatchProcessor::handleAnalyzeSTFT (const juce::var& cmd)
{
    using namespace JsonHelper;

    if (lastOutputBuffer.getNumSamples() == 0)
        return makeError ("analyze_stft", "No output buffer available — run generate_and_process first");

    int fftOrder              = static_cast<int> (cmd.getProperty ("fft_order", 11));
    int hopSize               = static_cast<int> (cmd.getProperty ("hop_size", 512));
    int channel               = static_cast<int> (cmd.getProperty ("channel", 0));
    bool includeSpectrogram   = static_cast<bool> (cmd.getProperty ("include_spectrogram", false));
    int maxFrames             = static_cast<int> (cmd.getProperty ("max_frames", 200));

    fftOrder  = juce::jlimit (8, 14, fftOrder);   // 256 .. 16384
    channel   = juce::jlimit (0, lastOutputBuffer.getNumChannels() - 1, channel);
    maxFrames = juce::jlimit (10, 10000, maxFrames);

    auto result = AudioAnalyzer::performSTFT (lastOutputBuffer, lastSampleRate,
                                              fftOrder, hopSize, channel);
    auto resultJson = AudioAnalyzer::stftToJson (result, includeSpectrogram, maxFrames);

    auto data = makeObject();
    set (data, "num_samples",  lastOutputBuffer.getNumSamples());
    set (data, "num_channels", lastOutputBuffer.getNumChannels());
    set (data, "sample_rate",  lastSampleRate);
    set (data, "channel",      channel);
    set (data, "stft",         resultJson);

    return makeSuccess ("analyze_stft", data);
}

// ============================================================================
juce::var BatchProcessor::handleAnalyzeTimeDomain (const juce::var& cmd)
{
    using namespace JsonHelper;

    if (lastOutputBuffer.getNumSamples() == 0)
        return makeError ("analyze_time_domain",
                          "No output buffer available — run generate_and_process first");

    bool includeSamples       = static_cast<bool> (cmd.getProperty ("include_samples", true));
    int maxSamplesPerChannel  = static_cast<int> (cmd.getProperty ("max_samples", 10000));

    maxSamplesPerChannel = juce::jlimit (100, 1000000, maxSamplesPerChannel);

    auto result = AudioAnalyzer::analyzeTimeDomain (lastOutputBuffer, lastSampleRate,
                                                     maxSamplesPerChannel);
    auto resultJson = AudioAnalyzer::timeDomainToJson (result, includeSamples);

    return makeSuccess ("analyze_time_domain", resultJson);
}

// ============================================================================
juce::var BatchProcessor::handleAnalyzeLoudness (const juce::var& cmd)
{
    using namespace JsonHelper;

    if (lastOutputBuffer.getNumSamples() == 0)
        return makeError ("analyze_loudness",
                          "No output buffer available — run generate_and_process first");

    float windowMs     = static_cast<float> (cmd.getProperty ("window_ms", 10.0));
    float hopMs        = static_cast<float> (cmd.getProperty ("hop_ms", 5.0));
    int channel        = static_cast<int> (cmd.getProperty ("channel", 0));
    bool includeEnvelope = static_cast<bool> (cmd.getProperty ("include_envelope", true));
    int maxPoints      = static_cast<int> (cmd.getProperty ("max_points", 500));

    channel   = juce::jlimit (0, lastOutputBuffer.getNumChannels() - 1, channel);
    maxPoints = juce::jlimit (10, 50000, maxPoints);

    auto result = AudioAnalyzer::analyzeLoudness (lastOutputBuffer, lastSampleRate,
                                                   windowMs, hopMs, channel);
    auto resultJson = AudioAnalyzer::loudnessToJson (result, includeEnvelope, maxPoints);

    auto data = makeObject();
    set (data, "num_samples",  lastOutputBuffer.getNumSamples());
    set (data, "num_channels", lastOutputBuffer.getNumChannels());
    set (data, "sample_rate",  lastSampleRate);
    set (data, "channel",      channel);
    set (data, "loudness",     resultJson);

    return makeSuccess ("analyze_loudness", data);
}
