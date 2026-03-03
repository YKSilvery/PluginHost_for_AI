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

    // Give the editor a native window peer so all child components can
    // render correctly (required under headless X / xvfb).
    activeEditor->addToDesktop (juce::ComponentPeer::windowIsTemporary);
    activeEditor->setVisible (true);
    activeEditor->setBounds (0, 0, activeEditor->getWidth(), activeEditor->getHeight());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (200);

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

    // Remove from desktop before destroying to avoid dangling peer references
    if (activeEditor->isOnDesktop())
        activeEditor->removeFromDesktop();

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
// v1.2: Multi-bus I/O
// ============================================================================
juce::var PluginHost::getBusLayout() const
{
    using namespace JsonHelper;

    if (! pluginInstance)
        return makeError ("get_bus_layout", "No plugin loaded");

    auto data = makeObject();

    // Input buses
    auto inputArr = makeArray();
    int inputBusCount = pluginInstance->getBusCount (true);
    int totalInCh = 0;
    for (int i = 0; i < inputBusCount; ++i)
    {
        auto* bus = pluginInstance->getBus (true, i);
        auto busObj = makeObject();
        set (busObj, "bus_index",       i);
        set (busObj, "name",            bus->getName());
        set (busObj, "is_main",         bus->isMain());
        set (busObj, "is_enabled",      bus->isEnabled());
        set (busObj, "num_channels",    bus->getNumberOfChannels());
        set (busObj, "channel_offset",  totalInCh);
        set (busObj, "default_layout",  bus->getDefaultLayout().getDescription());
        set (busObj, "current_layout",  bus->getCurrentLayout().getDescription());
        set (busObj, "is_enabled_by_default", bus->isEnabledByDefault());
        totalInCh += bus->getNumberOfChannels();
        append (inputArr, busObj);
    }

    // Output buses
    auto outputArr = makeArray();
    int outputBusCount = pluginInstance->getBusCount (false);
    int totalOutCh = 0;
    for (int i = 0; i < outputBusCount; ++i)
    {
        auto* bus = pluginInstance->getBus (false, i);
        auto busObj = makeObject();
        set (busObj, "bus_index",       i);
        set (busObj, "name",            bus->getName());
        set (busObj, "is_main",         bus->isMain());
        set (busObj, "is_enabled",      bus->isEnabled());
        set (busObj, "num_channels",    bus->getNumberOfChannels());
        set (busObj, "channel_offset",  totalOutCh);
        set (busObj, "default_layout",  bus->getDefaultLayout().getDescription());
        set (busObj, "current_layout",  bus->getCurrentLayout().getDescription());
        set (busObj, "is_enabled_by_default", bus->isEnabledByDefault());
        totalOutCh += bus->getNumberOfChannels();
        append (outputArr, busObj);
    }

    set (data, "input_bus_count",       inputBusCount);
    set (data, "output_bus_count",      outputBusCount);
    set (data, "total_input_channels",  totalInCh);
    set (data, "total_output_channels", totalOutCh);
    set (data, "input_buses",           inputArr);
    set (data, "output_buses",          outputArr);

    return makeSuccess ("get_bus_layout", data);
}

// ============================================================================
juce::var PluginHost::configureBuses (const juce::var& config)
{
    using namespace JsonHelper;

    if (! pluginInstance)
        return makeError ("configure_buses", "No plugin loaded");

    if (! config.isArray())
        return makeError ("configure_buses", "'buses' must be an array");

    auto* arr = config.getArray();
    auto resultsArr = makeArray();

    for (int i = 0; i < arr->size(); ++i)
    {
        auto& entry = arr->getReference (i);
        bool isInput   = static_cast<bool> (entry.getProperty ("is_input", true));
        int busIndex   = static_cast<int> (entry.getProperty ("bus_index", 0));
        bool enabled   = static_cast<bool> (entry.getProperty ("enabled", true));

        auto* bus = pluginInstance->getBus (isInput, busIndex);
        auto resultObj = makeObject();
        set (resultObj, "is_input",  isInput);
        set (resultObj, "bus_index", busIndex);

        if (bus == nullptr)
        {
            set (resultObj, "success", false);
            set (resultObj, "error",   "Bus not found");
        }
        else
        {
            bool ok = bus->enable (enabled);
            set (resultObj, "success",      ok);
            set (resultObj, "is_enabled",   bus->isEnabled());
            set (resultObj, "num_channels", bus->getNumberOfChannels());
        }
        append (resultsArr, resultObj);
    }

    // Re-prepare after bus change
    pluginInstance->prepareToPlay (currentSampleRate, currentBlockSize);

    auto data = makeObject();
    set (data, "results",               resultsArr);
    set (data, "total_input_channels",  pluginInstance->getTotalNumInputChannels());
    set (data, "total_output_channels", pluginInstance->getTotalNumOutputChannels());

    return makeSuccess ("configure_buses", data);
}

// ============================================================================
juce::AudioBuffer<float> PluginHost::processMultiBus (
    const std::vector<juce::AudioBuffer<float>>& inputBuses,
    const juce::MidiBuffer& midiEvents,
    int totalSamples)
{
    if (! pluginInstance)
        return {};

    const int numInCh  = pluginInstance->getTotalNumInputChannels();
    const int numOutCh = pluginInstance->getTotalNumOutputChannels();
    const int maxCh    = std::max (numInCh, numOutCh);
    const int blockSize = currentBlockSize;

    // Determine total samples from the largest bus input, or use provided value
    if (totalSamples <= 0)
    {
        for (auto& buf : inputBuses)
            totalSamples = std::max (totalSamples, buf.getNumSamples());
    }
    if (totalSamples <= 0)
        return {};

    // Build a flat interleaved input buffer matching the plugin's total input channels
    juce::AudioBuffer<float> flatInput (maxCh, totalSamples);
    flatInput.clear();

    // Map each input bus to its channel offset in the flat buffer
    int channelOffset = 0;
    int inputBusCount = pluginInstance->getBusCount (true);
    for (int busIdx = 0; busIdx < inputBusCount; ++busIdx)
    {
        auto* bus = pluginInstance->getBus (true, busIdx);
        if (bus == nullptr || ! bus->isEnabled())
            continue;

        int busCh = bus->getNumberOfChannels();

        if (busIdx < static_cast<int> (inputBuses.size()))
        {
            auto& srcBuf = inputBuses[static_cast<size_t> (busIdx)];
            int chToCopy = std::min (busCh, srcBuf.getNumChannels());
            int samplesToUse = std::min (totalSamples, srcBuf.getNumSamples());

            for (int ch = 0; ch < chToCopy; ++ch)
            {
                if (channelOffset + ch < maxCh)
                    flatInput.copyFrom (channelOffset + ch, 0, srcBuf, ch, 0, samplesToUse);
            }
        }
        // else: bus gets silence (already cleared)

        channelOffset += busCh;
    }

    // Process in blocks
    juce::AudioBuffer<float> output (maxCh, totalSamples);
    output.clear();

    int samplePos = 0;
    while (samplePos < totalSamples)
    {
        const int samplesThisBlock = std::min (blockSize, totalSamples - samplePos);

        juce::AudioBuffer<float> blockBuffer (maxCh, samplesThisBlock);
        blockBuffer.clear();

        // Copy input channels into block
        for (int ch = 0; ch < maxCh; ++ch)
            blockBuffer.copyFrom (ch, 0, flatInput, ch, samplePos, samplesThisBlock);

        // Extract MIDI events for this block
        juce::MidiBuffer blockMidi;
        for (const auto metadata : midiEvents)
        {
            int ts = metadata.samplePosition;
            if (ts >= samplePos && ts < samplePos + samplesThisBlock)
                blockMidi.addEvent (metadata.getMessage(), ts - samplePos);
        }

        pluginInstance->processBlock (blockBuffer, blockMidi);

        for (int ch = 0; ch < maxCh; ++ch)
            output.copyFrom (ch, samplePos, blockBuffer, ch, 0, samplesThisBlock);

        samplePos += samplesThisBlock;
    }

    return output;
}

// ============================================================================
// v1.2: UI Interaction Simulation
// ============================================================================
juce::var PluginHost::simulateClick (int x, int y, int numClicks, bool isRight)
{
    using namespace JsonHelper;

    if (! activeEditor)
        return makeError ("simulate_click", "No editor open — call open_editor first");

    auto* peer = activeEditor->getPeer();
    if (peer == nullptr)
        return makeError ("simulate_click", "Editor has no peer (not on desktop)");

    auto pos = juce::Point<float> (static_cast<float> (x), static_cast<float> (y));
    auto mods = isRight ? juce::ModifierKeys::rightButtonModifier
                        : juce::ModifierKeys::leftButtonModifier;

    // Identify the component at the click point
    auto* target = activeEditor->getComponentAt (x, y);
    juce::String targetName = target ? target->getName() : "none";
    juce::String targetType = target ? target->getComponentID() : "";
    if (targetType.isEmpty() && target)
        targetType = typeid (*target).name();

    for (int click = 0; click < numClicks; ++click)
    {
        juce::int64 now = juce::Time::currentTimeMillis();

        // Mouse down
        peer->handleMouseEvent (juce::MouseInputSource::InputSourceType::mouse,
                                pos, mods, 0.0f, 0.0f, now);

        // Pump message loop briefly
        juce::MessageManager::getInstance()->runDispatchLoopUntil (10);

        // Mouse up
        peer->handleMouseEvent (juce::MouseInputSource::InputSourceType::mouse,
                                pos, juce::ModifierKeys(), 0.0f, 0.0f,
                                juce::Time::currentTimeMillis());

        juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
    }

    // Allow UI to settle
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

    auto data = makeObject();
    set (data, "x", x);
    set (data, "y", y);
    set (data, "num_clicks",    numClicks);
    set (data, "button",        isRight ? "right" : "left");
    set (data, "target_name",   targetName);
    set (data, "target_type",   targetType);

    return makeSuccess ("simulate_click", data);
}

// ============================================================================
juce::var PluginHost::simulateDrag (int startX, int startY, int endX, int endY,
                                    int steps, float durationMs)
{
    using namespace JsonHelper;

    if (! activeEditor)
        return makeError ("simulate_drag", "No editor open — call open_editor first");

    auto* peer = activeEditor->getPeer();
    if (peer == nullptr)
        return makeError ("simulate_drag", "Editor has no peer (not on desktop)");

    auto startPos = juce::Point<float> (static_cast<float> (startX),
                                        static_cast<float> (startY));
    auto endPos   = juce::Point<float> (static_cast<float> (endX),
                                        static_cast<float> (endY));
    auto mods     = juce::ModifierKeys::leftButtonModifier;

    steps = std::max (steps, 2);
    int delayPerStep = std::max (1, static_cast<int> (durationMs / steps));

    // Identify the component at the start point
    auto* target = activeEditor->getComponentAt (startX, startY);
    juce::String targetName = target ? target->getName() : "none";

    // Mouse down at start
    peer->handleMouseEvent (juce::MouseInputSource::InputSourceType::mouse,
                            startPos, mods, 0.0f, 0.0f,
                            juce::Time::currentTimeMillis());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (10);

    // Interpolate through intermediate positions
    for (int i = 1; i <= steps; ++i)
    {
        float t = static_cast<float> (i) / static_cast<float> (steps);
        auto pos = startPos + (endPos - startPos) * t;

        peer->handleMouseEvent (juce::MouseInputSource::InputSourceType::mouse,
                                pos, mods, 0.0f, 0.0f,
                                juce::Time::currentTimeMillis());
        juce::MessageManager::getInstance()->runDispatchLoopUntil (delayPerStep);
    }

    // Mouse up at end
    peer->handleMouseEvent (juce::MouseInputSource::InputSourceType::mouse,
                            endPos, juce::ModifierKeys(), 0.0f, 0.0f,
                            juce::Time::currentTimeMillis());
    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

    auto data = makeObject();
    set (data, "start_x", startX);
    set (data, "start_y", startY);
    set (data, "end_x",   endX);
    set (data, "end_y",   endY);
    set (data, "steps",   steps);
    set (data, "target_name", targetName);

    return makeSuccess ("simulate_drag", data);
}

// ============================================================================
juce::var PluginHost::simulateMouseWheel (int x, int y, float deltaX, float deltaY)
{
    using namespace JsonHelper;

    if (! activeEditor)
        return makeError ("simulate_mouse_wheel", "No editor open — call open_editor first");

    auto* peer = activeEditor->getPeer();
    if (peer == nullptr)
        return makeError ("simulate_mouse_wheel", "Editor has no peer (not on desktop)");

    auto pos = juce::Point<float> (static_cast<float> (x), static_cast<float> (y));

    juce::MouseWheelDetails wheel;
    wheel.deltaX = deltaX;
    wheel.deltaY = deltaY;
    wheel.isReversed = false;
    wheel.isSmooth   = true;
    wheel.isInertial = false;

    auto* target = activeEditor->getComponentAt (x, y);
    juce::String targetName = target ? target->getName() : "none";

    peer->handleMouseWheel (juce::MouseInputSource::InputSourceType::mouse,
                            pos, juce::Time::currentTimeMillis(), wheel);

    juce::MessageManager::getInstance()->runDispatchLoopUntil (50);

    auto data = makeObject();
    set (data, "x", x);
    set (data, "y", y);
    set (data, "delta_x", deltaX);
    set (data, "delta_y", deltaY);
    set (data, "target_name", targetName);

    return makeSuccess ("simulate_mouse_wheel", data);
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
