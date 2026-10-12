cmake_minimum_required(VERSION 3.22)
project(WaveformScope VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# JUCE is downloaded at configure time (pinned version). To use a local checkout instead:
#   cmake -B build -DFETCHCONTENT_SOURCE_DIR_JUCE=path/to/JUCE
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 9.0.3
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(WaveformScope
    COMPANY_NAME "Tomas"
    PLUGIN_MANUFACTURER_CODE Tmds
    PLUGIN_CODE Wvsc
    FORMATS VST3 Standalone
    PRODUCT_NAME "Waveform Scope"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    VST3_CATEGORIES Fx Analyzer
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(WaveformScope PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/WaveformView.cpp)

target_compile_definitions(WaveformScope PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(WaveformScope
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
