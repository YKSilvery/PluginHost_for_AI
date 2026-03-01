#include "PluginHost.h"
#include "JsonHelper.h"
#include <signal.h>
#include <chrono>

// ============================================================================
PluginHost::PluginHost()  = default;
PluginHost::~PluginHost()
{
    closeEditor();
    unloadPlugin();
}

// ============================================================================
void PluginHost::initialize()
{
#if JUCE_PLUGINHOST_VST3
    formatManager.addFormat (new juce::VST3PluginFormat());
#endif
}

// ============================================================================
bool PluginHost::isSynth() const
{
    if (! pluginInstance) return false;
    return pluginInstance->acceptsMidi();
}

// ============================================================================
juce::var PluginHost::loadPlugin (const juce::String& path, double sampleRate, int blockSize)
{
    using namespace JsonHelper;

    // Close any existing editor before unloading
    closeEditor();
    unloadPlugin();
    currentSampleRate = sampleRate;
    currentBlockSize  = blockSize;

    juce::File pluginFile (path);
    if (! pluginFile.exists())
        return makeError ("load_plugin", "Plugin file not found: " + path);

    // Scan the file for plugin descriptions
    scannedDescriptions.clear();
    for (auto* format : formatManager.getFormats())
        format->findAllTypesForFile (scannedDescriptions, path);

    if (scannedDescriptions.isEmpty())
        return makeError ("load_plugin",
                          "No valid plugin found in: " + path
                          + " (is the VST3 bundle intact?)");

    // Instantiate the first description found
    juce::String errorMessage;

    pluginInstance = formatManager.createPluginInstance (
        *scannedDescriptions.getFirst(), sampleRate, blockSize, errorMessage);

    if (pluginInstance == nullptr)
    {
        juce::String detail = "Failed to instantiate plugin: " + errorMessage;
        return makeError ("load_plugin", detail);
    }

    // Prepare to play
    pluginInstance->enableAllBuses();
    pluginInstance->prepareToPlay (sampleRate, blockSize);

    return getPluginInfo();
}

// ============================================================================
void PluginHost::unloadPlugin()
{
    // Must close editor first to avoid dangling references
    closeEditor();

    if (pluginInstance)
    {
        pluginInstance->releaseResources();
        pluginInstance.reset();
    }
    scannedDescriptions.clear();
}

// ============================================================================
int PluginHost::getNumInputChannels() const
{
    return pluginInstance ? pluginInstance->getTotalNumInputChannels() : 0;
}

int PluginHost::getNumOutputChannels() const
{
    return pluginInstance ? pluginInstance->getTotalNumOutputChannels() : 0;
}

// ============================================================================
juce::var PluginHost::getPluginInfo() const
{
    using namespace JsonHelper;

    if (! pluginInstance)
        return makeError ("get_plugin_info", "No plugin loaded");

    auto data = makeObject();
    set (data, "plugin_name",        pluginInstance->getName());
    set (data, "plugin_format",      pluginInstance->getPluginDescription().pluginFormatName);
    set (data, "manufacturer",       pluginInstance->getPluginDescription().manufacturerName);
    set (data, "version",            pluginInstance->getPluginDescription().version);
    set (data, "category",           pluginInstance->getPluginDescription().category);
    set (data, "unique_id",          pluginInstance->getPluginDescription().uniqueId);
    set (data, "num_input_channels", pluginInstance->getTotalNumInputChannels());
    set (data, "num_output_channels",pluginInstance->getTotalNumOutputChannels());
    set (data, "latency_samples",    pluginInstance->getLatencySamples());
    set (data, "accepts_midi",       pluginInstance->acceptsMidi());
    set (data, "produces_midi",      pluginInstance->producesMidi());
    set (data, "is_synth",           isSynth());
    set (data, "has_editor",         pluginInstance->hasEditor());
    set (data, "sample_rate",        currentSampleRate);
    set (data, "block_size",         currentBlockSize);
    set (data, "num_parameters",     pluginInstance->getParameters().size());
    set (data, "num_programs",       pluginInstance->getNumPrograms());
    set (data, "current_program",    pluginInstance->getCurrentProgram());

    return makeSuccess ("load_plugin", data);
}

// ============================================================================
juce::var PluginHost::listParameters() const
{
    using namespace JsonHelper;

    if (! pluginInstance)
        return makeError ("list_parameters", "No plugin loaded");

    auto paramsArr = makeArray();
    auto& params = pluginInstance->getParameters();

    for (int i = 0; i < params.size(); ++i)
    {
        auto* param = params[i];
        auto pObj = makeObject();
        set (pObj, "index", i);
        set (pObj, "name",  param->getName (128));
        set (pObj, "label", param->getLabel());
        set (pObj, "value_normalized", param->getValue());
        set (pObj, "value_text",       param->getCurrentValueAsText());
        set (pObj, "num_steps",        param->getNumSteps());
        set (pObj, "is_automatable",   param->isAutomatable());
        set (pObj, "is_discrete",      param->isDiscrete());
        set (pObj, "is_boolean",       param->isBoolean());

        // Try to get parameter ID for APVTS-based plugins
        if (auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
        {
            set (pObj, "parameter_id", withID->paramID);
        }

        // Try to get range for RangedAudioParameter
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*> (param))
        {
            auto range = ranged->getNormalisableRange();
            auto rangeObj = makeObject();
            set (rangeObj, "start",    range.start);
            set (rangeObj, "end",      range.end);
            set (rangeObj, "interval", range.interval);
            set (pObj, "range", rangeObj);
            set (pObj, "default_value", ranged->getDefaultValue());
        }

        append (paramsArr, pObj);
    }

    return makeSuccess ("list_parameters", paramsArr);
}

// ============================================================================
juce::AudioProcessorParameter* PluginHost::findParameter (const juce::String& idOrIndex) const
{
    if (! pluginInstance) return nullptr;
    auto& params = pluginInstance->getParameters();

    // Try numeric index first
    if (idOrIndex.containsOnly ("0123456789"))
    {
        int idx = idOrIndex.getIntValue();
        if (idx >= 0 && idx < params.size())
            return params[idx];
    }

    // Search by parameter ID
    for (auto* param : params)
    {
        if (auto* withID = dynamic_cast<juce::AudioProcessorParameterWithID*> (param))
        {
            if (withID->paramID == idOrIndex)
                return param;
        }
    }

    // Search by name (case-insensitive)
    for (auto* param : params)
    {
        if (param->getName (128).equalsIgnoreCase (idOrIndex))
            return param;
    }

    return nullptr;
}

// ============================================================================
juce::var PluginHost::setParameter (const juce::String& idOrIndex, float value)
{
    using namespace JsonHelper;

    if (! pluginInstance)
        return makeError ("set_parameter", "No plugin loaded");

    auto* param = findParameter (idOrIndex);
    if (param == nullptr)
        return makeError ("set_parameter", "Parameter not found: " + idOrIndex);

    // Clamp to [0, 1]
    float normalized = juce::jlimit (0.0f, 1.0f, value);

    param->beginChangeGesture();
    param->setValueNotifyingHost (normalized);
    param->endChangeGesture();

    auto data = makeObject();
    set (data, "parameter_id",    idOrIndex);
    set (data, "value_set",       normalized);
    set (data, "current_text",    param->getCurrentValueAsText());
    return makeSuccess ("set_parameter", data);
}

// ============================================================================
juce::var PluginHost::getParameter (const juce::String& idOrIndex) const
{
    using namespace JsonHelper;

    if (! pluginInstance)
        return makeError ("get_parameter", "No plugin loaded");

    auto* param = findParameter (idOrIndex);
    if (param == nullptr)
        return makeError ("get_parameter", "Parameter not found: " + idOrIndex);

    auto data = makeObject();
    set (data, "parameter_id",       idOrIndex);
    set (data, "name",               param->getName (128));
    set (data, "value_normalized",   param->getValue());
    set (data, "value_text",         param->getCurrentValueAsText());
    return makeSuccess ("get_parameter", data);
}

// ============================================================================
juce::AudioBuffer<float> PluginHost::processAudio (const juce::AudioBuffer<float>& input)
{
    if (! pluginInstance)
        return {};

    const int numInCh  = pluginInstance->getTotalNumInputChannels();
    const int numOutCh = pluginInstance->getTotalNumOutputChannels();
    const int maxCh    = std::max (numInCh, numOutCh);
    const int totalSamples = input.getNumSamples();
    const int blockSize    = currentBlockSize;

    // Prepare output buffer (full duration)
    juce::AudioBuffer<float> output (maxCh, totalSamples);
    output.clear();

    // Process in blockSize chunks
    int samplePos = 0;
    while (samplePos < totalSamples)
    {
        const int samplesThisBlock = std::min (blockSize, totalSamples - samplePos);

        // Create a working buffer for this block
        juce::AudioBuffer<float> blockBuffer (maxCh, samplesThisBlock);
        blockBuffer.clear();

        // Copy input channels into block
        for (int ch = 0; ch < std::min (input.getNumChannels(), maxCh); ++ch)
            blockBuffer.copyFrom (ch, 0, input, ch, samplePos, samplesThisBlock);

        // Process
        juce::MidiBuffer midiBuffer;
        pluginInstance->processBlock (blockBuffer, midiBuffer);

        // Copy output from block to output buffer
        for (int ch = 0; ch < maxCh; ++ch)
            output.copyFrom (ch, samplePos, blockBuffer, ch, 0, samplesThisBlock);

        samplePos += samplesThisBlock;
    }

    return output;
}

// ============================================================================
juce::AudioBuffer<float> PluginHost::processWithMidi (int totalSamples,
                                                      const juce::MidiBuffer& midiEvents)
{
    if (! pluginInstance)
        return {};

    const int numOutCh   = std::max (pluginInstance->getTotalNumOutputChannels(), 1);
    const int blockSize  = currentBlockSize;

    juce::AudioBuffer<float> output (numOutCh, totalSamples);
    output.clear();

    int samplePos = 0;
    while (samplePos < totalSamples)
    {
        const int samplesThisBlock = std::min (blockSize, totalSamples - samplePos);

        juce::AudioBuffer<float> blockBuffer (numOutCh, samplesThisBlock);
        blockBuffer.clear();

        // Extract MIDI events for this block's time window
        juce::MidiBuffer blockMidi;
        for (const auto metadata : midiEvents)
        {
            int ts = metadata.samplePosition;
            if (ts >= samplePos && ts < samplePos + samplesThisBlock)
                blockMidi.addEvent (metadata.getMessage(), ts - samplePos);
        }

        pluginInstance->processBlock (blockBuffer, blockMidi);

        for (int ch = 0; ch < numOutCh; ++ch)
            output.copyFrom (ch, samplePos, blockBuffer, ch, 0, samplesThisBlock);

        samplePos += samplesThisBlock;
    }

    return output;
}

// ============================================================================
juce::AudioBuffer<float> PluginHost::renderSynthNote (int noteNumber, float velocity,
                                                      double durationMs, double releaseMs)
{
    if (! pluginInstance)
        return {};

    const int totalSamples = static_cast<int> (currentSampleRate * durationMs / 1000.0);
    const int releaseSample = std::max (0,
        totalSamples - static_cast<int> (currentSampleRate * releaseMs / 1000.0));

    int midiChannel = 1;
    juce::uint8 vel = static_cast<juce::uint8> (juce::jlimit (0.0f, 1.0f, velocity) * 127.0f);

    juce::MidiBuffer midi;
    midi.addEvent (juce::MidiMessage::noteOn  (midiChannel, noteNumber, vel), 0);
    midi.addEvent (juce::MidiMessage::noteOff (midiChannel, noteNumber, (juce::uint8) 0), releaseSample);

    return processWithMidi (totalSamples, midi);
}

// ============================================================================
// UI Lifecycle
// ============================================================================
juce::var PluginHost::openEditor()
{
    using namespace JsonHelper;

    if (! pluginInstance)
        return makeError ("open_editor", "No plugin loaded");

    if (activeEditor != nullptr)
        return makeError ("open_editor", "Editor is already open — close it first");

    if (! pluginInstance->hasEditor())
        return makeError ("open_editor", "Plugin does not have a GUI editor");

    auto startTime = std::chrono::steady_clock::now();

    activeEditor.reset (pluginInstance->createEditor());

    if (activeEditor == nullptr)
        return makeError ("open_editor", "createEditor() returned nullptr");

    // Give it a size and let it initialize
    activeEditor->setSize (activeEditor->getWidth(), activeEditor->getHeight());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

    auto endTime = std::chrono::steady_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli> (endTime - startTime).count();

    auto data = makeObject();
    set (data, "editor_width",  activeEditor->getWidth());
    set (data, "editor_height", activeEditor->getHeight());
    set (data, "open_time_ms",  elapsedMs);
    return makeSuccess ("open_editor", data);
}

// ============================================================================
juce::var PluginHost::closeEditor()
{
    using namespace JsonHelper;

    if (activeEditor == nullptr)
    {
        // Not an error — idempotent close
        return makeSuccess ("close_editor");
    }

    auto startTime = std::chrono::steady_clock::now();

    activeEditor.reset();

    // Pump message loop to allow deferred deletions to complete
    juce::MessageManager::getInstance()->runDispatchLoopUntil (100);

    auto endTime = std::chrono::steady_clock::now();
    double elapsedMs = std::chrono::duration<double, std::milli> (endTime - startTime).count();

    auto data = makeObject();
    set (data, "close_time_ms", elapsedMs);
    return makeSuccess ("close_editor", data);
}

// ============================================================================
// Lifecycle Stress Testing
// ============================================================================
juce::var PluginHost::testPluginLifecycle (const juce::String& path,
                                           double sampleRate, int blockSize,
                                           int iterations)
{
    using namespace JsonHelper;

    auto resultsArr = makeArray();
    int successCount = 0;
    int failCount    = 0;
    double totalLoadMs   = 0.0;
    double totalUnloadMs = 0.0;

    for (int i = 0; i < iterations; ++i)
    {
        auto iterObj = makeObject();
        set (iterObj, "iteration", i);

        // Load
        auto loadStart = std::chrono::steady_clock::now();
        auto loadResult = loadPlugin (path, sampleRate, blockSize);
        auto loadEnd = std::chrono::steady_clock::now();
        double loadMs = std::chrono::duration<double, std::milli> (loadEnd - loadStart).count();

        bool loaded = isPluginLoaded();
        set (iterObj, "load_success", loaded);
        set (iterObj, "load_time_ms", loadMs);
        totalLoadMs += loadMs;

        if (loaded)
        {
            // Quick process test — 1 block of silence
            juce::AudioBuffer<float> silence (std::max (getNumInputChannels(), getNumOutputChannels()), blockSize);
            silence.clear();
            juce::MidiBuffer emptyMidi;
            pluginInstance->processBlock (silence, emptyMidi);
            set (iterObj, "process_ok", true);
        }

        // Unload
        auto unloadStart = std::chrono::steady_clock::now();
        unloadPlugin();
        auto unloadEnd = std::chrono::steady_clock::now();
        double unloadMs = std::chrono::duration<double, std::milli> (unloadEnd - unloadStart).count();

        set (iterObj, "unload_time_ms", unloadMs);
        totalUnloadMs += unloadMs;

        bool stillAlive = (pluginInstance == nullptr); // should be null after unload
        set (iterObj, "clean_unload", stillAlive);

        if (loaded && stillAlive)
            ++successCount;
        else
            ++failCount;

        append (resultsArr, iterObj);
    }

    auto data = makeObject();
    set (data, "iterations",       iterations);
    set (data, "success_count",    successCount);
    set (data, "fail_count",       failCount);
    set (data, "avg_load_ms",      totalLoadMs / std::max (iterations, 1));
    set (data, "avg_unload_ms",    totalUnloadMs / std::max (iterations, 1));
    set (data, "all_passed",       failCount == 0);
    set (data, "details",          resultsArr);

    return makeSuccess ("test_plugin_lifecycle", data);
}

// ============================================================================
juce::var PluginHost::testEditorLifecycle (int iterations, int delayBetweenMs)
{
    using namespace JsonHelper;

    if (! pluginInstance)
        return makeError ("test_editor_lifecycle", "No plugin loaded");

    if (! pluginInstance->hasEditor())
        return makeError ("test_editor_lifecycle", "Plugin does not have a GUI editor");

    auto resultsArr = makeArray();
    int successCount = 0;
    int failCount    = 0;
    double totalOpenMs  = 0.0;
    double totalCloseMs = 0.0;

    for (int i = 0; i < iterations; ++i)
    {
        auto iterObj = makeObject();
        set (iterObj, "iteration", i);

        // Open
        auto openStart = std::chrono::steady_clock::now();
        auto openResult = openEditor();
        auto openEnd = std::chrono::steady_clock::now();
        double openMs = std::chrono::duration<double, std::milli> (openEnd - openStart).count();

        bool opened = isEditorOpen();
        set (iterObj, "open_success", opened);
        set (iterObj, "open_time_ms", openMs);
        totalOpenMs += openMs;

        // Wait a bit to simulate real usage
        if (delayBetweenMs > 0)
            juce::MessageManager::getInstance()->runDispatchLoopUntil (delayBetweenMs);

        // Close
        auto closeStart = std::chrono::steady_clock::now();
        closeEditor();
        auto closeEnd = std::chrono::steady_clock::now();
        double closeMs = std::chrono::duration<double, std::milli> (closeEnd - closeStart).count();

        bool closed = ! isEditorOpen();
        set (iterObj, "close_success", closed);
        set (iterObj, "close_time_ms", closeMs);
        totalCloseMs += closeMs;

        if (opened && closed)
            ++successCount;
        else
            ++failCount;

        append (resultsArr, iterObj);
    }

    auto data = makeObject();
    set (data, "iterations",       iterations);
    set (data, "success_count",    successCount);
    set (data, "fail_count",       failCount);
    set (data, "avg_open_ms",      totalOpenMs / std::max (iterations, 1));
    set (data, "avg_close_ms",     totalCloseMs / std::max (iterations, 1));
    set (data, "all_passed",       failCount == 0);
    set (data, "details",          resultsArr);

    return makeSuccess ("test_editor_lifecycle", data);
}
