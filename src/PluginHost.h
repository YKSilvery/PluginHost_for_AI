#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

/**
 * Core VST3 plugin loader and processor.
 * Manages a single plugin instance lifecycle:
 *   load -> prepareToPlay -> processBlock(s) -> releaseResources -> unload
 *
 * Supports both effects (audio in -> audio out) and synths (MIDI in -> audio out).
 */
class PluginHost
{
public:
    PluginHost();
    ~PluginHost();

    /** Register supported plugin formats (VST3, etc.). */
    void initialize();

    /**
     * Scan + instantiate a plugin from a file path.
     * Returns JSON result with plugin info or error details.
     */
    juce::var loadPlugin (const juce::String& path,
                          double sampleRate = 44100.0,
                          int blockSize = 512);

    /** Release the current plugin. */
    void unloadPlugin();

    /** Query: is a plugin currently loaded? */
    bool isPluginLoaded() const { return pluginInstance != nullptr; }

    /** Query: is the loaded plugin a synth (accepts MIDI)? */
    bool isSynth() const;

    /** Get descriptive information about the loaded plugin. */
    juce::var getPluginInfo() const;

    /** List all parameters with metadata. */
    juce::var listParameters() const;

    /**
     * Set a parameter value.
     * @param idOrIndex  either the parameter ID string or a numeric index
     * @param value      normalized value 0..1
     */
    juce::var setParameter (const juce::String& idOrIndex, float value);

    /** Get a parameter's current value. */
    juce::var getParameter (const juce::String& idOrIndex) const;

    /**
     * Process an audio buffer through the loaded plugin (multi-block).
     * For effects: input audio is passed through.
     * For synths: input buffer can be empty/silent; MIDI is what drives output.
     */
    juce::AudioBuffer<float> processAudio (const juce::AudioBuffer<float>& input);

    /**
     * Process with MIDI events injected.
     * Used for synth plugins -- sends MIDI note-on/off during processing.
     * @param totalSamples  total number of samples to render
     * @param midiEvents    MIDI buffer with events timestamped in samples
     */
    juce::AudioBuffer<float> processWithMidi (int totalSamples,
                                              const juce::MidiBuffer& midiEvents);

    /**
     * Convenience: render a synth note.
     * Sends note-on at sample 0, note-off near the end, processes the full duration.
     * @param noteNumber   MIDI note (0-127)
     * @param velocity     MIDI velocity (0.0-1.0)
     * @param durationMs   total render duration in milliseconds
     * @param releaseMs    how long before end to send note-off (for release tail)
     */
    juce::AudioBuffer<float> renderSynthNote (int noteNumber, float velocity,
                                              double durationMs, double releaseMs = 100.0);

    // =========================================================================
    // UI Lifecycle
    // =========================================================================

    /**
     * Create the plugin editor (offscreen). Returns JSON status.
     * The editor is held internally and can be closed later.
     */
    juce::var openEditor();

    /**
     * Destroy the currently held plugin editor. Returns JSON status.
     */
    juce::var closeEditor();

    /** Query: is an editor currently open? */
    bool isEditorOpen() const { return activeEditor != nullptr; }

    // =========================================================================
    // Lifecycle Stress Testing
    // =========================================================================

    /**
     * Stress-test plugin load/unload cycle N times.
     * Returns JSON with timing data and whether any iteration hung or crashed.
     */
    juce::var testPluginLifecycle (const juce::String& path,
                                  double sampleRate, int blockSize,
                                  int iterations);

    /**
     * Stress-test editor open/close cycle N times.
     * Plugin must already be loaded. Tests for UI resource leaks and hangs.
     */
    juce::var testEditorLifecycle (int iterations, int delayBetweenMs = 50);

    // =========================================================================

    /** Direct access to the underlying AudioPluginInstance (may be nullptr). */
    juce::AudioPluginInstance* getPluginInstance() { return pluginInstance.get(); }

    /** Direct access to the active editor (may be nullptr). */
    juce::AudioProcessorEditor* getActiveEditor() { return activeEditor.get(); }

    double getSampleRate()  const { return currentSampleRate; }
    int    getBlockSize()   const { return currentBlockSize; }
    int    getNumInputChannels()  const;
    int    getNumOutputChannels() const;

private:
    /** Attempt to find the parameter matching idOrIndex. */
    juce::AudioProcessorParameter* findParameter (const juce::String& idOrIndex) const;

    juce::AudioPluginFormatManager                formatManager;
    std::unique_ptr<juce::AudioPluginInstance>     pluginInstance;
    std::unique_ptr<juce::AudioProcessorEditor>    activeEditor;
    juce::OwnedArray<juce::PluginDescription>      scannedDescriptions;

    double currentSampleRate = 44100.0;
    int    currentBlockSize  = 512;
};
