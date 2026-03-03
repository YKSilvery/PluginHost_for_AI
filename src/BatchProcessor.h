#pragma once
#include <juce_core/juce_core.h>
#include "PluginHost.h"
#include "SignalGenerator.h"
#include "AudioAnalyzer.h"
#include "OffscreenRenderer.h"

/**
 * Batch command processor.
 * Accepts a JSON array of commands and executes them sequentially,
 * maintaining state (loaded plugin, last output buffer) across commands.
 *
 * Supported actions:
 *   load_plugin, unload_plugin, get_plugin_info,
 *   list_parameters, set_parameter, get_parameter,
 *   generate_and_process, analyze_output,
 *   capture_ui, get_parameter_layout,
 *   open_editor, close_editor,
 *   test_plugin_lifecycle, test_editor_lifecycle,
 *   send_midi_and_process, render_synth_note
 */
class BatchProcessor
{
public:
    BatchProcessor();
    ~BatchProcessor();

    /** Execute a JSON object with a "commands" array. Returns JSON with "results" array. */
    juce::var executeBatch (const juce::var& batchJson);

    /** Execute a single JSON command. */
    juce::var executeCommand (const juce::var& command, int index);

    /** Direct access to the host. */
    PluginHost& getHost() { return host; }

private:
    juce::var handleLoadPlugin          (const juce::var& cmd);
    juce::var handleUnloadPlugin        (const juce::var& cmd);
    juce::var handleGetPluginInfo       (const juce::var& cmd);
    juce::var handleListParameters      (const juce::var& cmd);
    juce::var handleSetParameter        (const juce::var& cmd);
    juce::var handleGetParameter        (const juce::var& cmd);
    juce::var handleGenerateAndProcess  (const juce::var& cmd);
    juce::var handleAnalyzeOutput       (const juce::var& cmd);
    juce::var handleCaptureUI           (const juce::var& cmd);
    juce::var handleGetParameterLayout  (const juce::var& cmd);

    // New: UI lifecycle
    juce::var handleOpenEditor          (const juce::var& cmd);
    juce::var handleCloseEditor         (const juce::var& cmd);

    // New: Lifecycle stress testing
    juce::var handleTestPluginLifecycle (const juce::var& cmd);
    juce::var handleTestEditorLifecycle (const juce::var& cmd);

    // New: Synth / MIDI support
    juce::var handleSendMidiAndProcess  (const juce::var& cmd);
    juce::var handleRenderSynthNote     (const juce::var& cmd);

    // v1.1: Advanced analysis commands
    juce::var handleAnalyzeSTFT         (const juce::var& cmd);
    juce::var handleAnalyzeTimeDomain   (const juce::var& cmd);
    juce::var handleAnalyzeLoudness     (const juce::var& cmd);

    // v1.2: Multi-bus I/O
    juce::var handleGetBusLayout        (const juce::var& cmd);
    juce::var handleConfigureBuses      (const juce::var& cmd);
    juce::var handleProcessMultiBus     (const juce::var& cmd);
    juce::var handleProcessAudioWithMidi(const juce::var& cmd);

    // v1.2: UI Interaction
    juce::var handleSimulateClick       (const juce::var& cmd);
    juce::var handleSimulateDrag        (const juce::var& cmd);
    juce::var handleSimulateMouseWheel  (const juce::var& cmd);

    PluginHost                 host;
    juce::AudioBuffer<float>   lastOutputBuffer;
    double                     lastSampleRate = 44100.0;
};
