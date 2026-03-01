#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>

/**
 * Offscreen UI renderer for plugin editors.
 * Captures plugin GUI to PNG and extracts component/parameter layout data
 * without displaying any window on screen.
 */
class OffscreenRenderer
{
public:
    /**
     * Render the plugin editor offscreen and save as PNG.
     * If existingEditor is non-null, uses that instead of creating a new one.
     * Returns JSON result with status and captured info.
     */
    static juce::var captureSnapshot (juce::AudioPluginInstance* plugin,
                                      const juce::String& outputPath,
                                      int width = 800, int height = 600,
                                      juce::AudioProcessorEditor* existingEditor = nullptr);

    /**
     * Extract the component tree and parameter mapping from the editor.
     * If existingEditor is non-null, uses that instead of creating a new one.
     * Returns JSON with each component's type, bounds, and any associated parameter.
     */
    static juce::var getParameterLayout (juce::AudioPluginInstance* plugin,
                                         int width = 800, int height = 600,
                                         juce::AudioProcessorEditor* existingEditor = nullptr);

private:
    /** Recursively walk the component tree and collect info. */
    static juce::var buildComponentTree (juce::Component* comp, int depth = 0);

    OffscreenRenderer() = delete;
};
