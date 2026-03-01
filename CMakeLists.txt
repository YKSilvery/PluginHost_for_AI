cmake_minimum_required(VERSION 3.22)
project(HeadlessPluginHost VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Add JUCE framework
add_subdirectory(JUCE)

# ==============================================================================
#  Headless VST3 Host Tool
# ==============================================================================
juce_add_console_app(HeadlessPluginHost
    PRODUCT_NAME "Headless Plugin Host"
    COMPANY_NAME "AIDebugTools"
    VERSION "1.0.0")

target_sources(HeadlessPluginHost PRIVATE
    src/Main.cpp
    src/PluginHost.cpp
    src/SignalGenerator.cpp
    src/AudioAnalyzer.cpp
    src/OffscreenRenderer.cpp
    src/BatchProcessor.cpp)

target_include_directories(HeadlessPluginHost PRIVATE src)

target_compile_definitions(HeadlessPluginHost PRIVATE
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_VST=0
    JUCE_PLUGINHOST_LADSPA=0
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STANDALONE_APPLICATION=1)

target_link_libraries(HeadlessPluginHost
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# ==============================================================================
#  Test Plugin (VST3) — simple gain/tone plugin for validation
# ==============================================================================
juce_add_plugin(TestGainPlugin
    PLUGIN_MANUFACTURER_CODE AiDb
    PLUGIN_CODE TGp1
    FORMATS VST3
    PRODUCT_NAME "Test Gain Plugin"
    COMPANY_NAME "AIDebugTools"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Fx")

target_sources(TestGainPlugin PRIVATE
    test_plugin/PluginProcessor.cpp
    test_plugin/PluginEditor.cpp)

target_include_directories(TestGainPlugin PRIVATE test_plugin)

target_compile_definitions(TestGainPlugin PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(TestGainPlugin
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)

# ==============================================================================
#  Test Synth Plugin (VST3) — simple polyphonic synth for validation
# ==============================================================================
juce_add_plugin(TestSynthPlugin
    PLUGIN_MANUFACTURER_CODE AiDb
    PLUGIN_CODE TSy1
    FORMATS VST3
    PRODUCT_NAME "Test Synth Plugin"
    COMPANY_NAME "AIDebugTools"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Instrument")

target_sources(TestSynthPlugin PRIVATE
    test_synth/SynthProcessor.cpp
    test_synth/SynthEditor.cpp)

target_include_directories(TestSynthPlugin PRIVATE test_synth)

target_compile_definitions(TestSynthPlugin PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(TestSynthPlugin
    PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
