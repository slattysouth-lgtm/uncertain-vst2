cmake_minimum_required(VERSION 3.22)
project(UNCERTAINSpatial VERSION 1.0.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 7.0.12 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(UNCERTAINSpatial
    COMPANY_NAME "UNCERTAIN"
    PRODUCT_NAME "UNCERTAIN Spatial"
    PLUGIN_MANUFACTURER_CODE Unct
    PLUGIN_CODE Uspt
    FORMATS VST3 Standalone
    VST3_CATEGORIES Fx Mastering Spatial Dynamics
    IS_SYNTH FALSE NEEDS_MIDI_INPUT FALSE NEEDS_MIDI_OUTPUT FALSE IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(UNCERTAINSpatial PRIVATE Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(UNCERTAINSpatial PUBLIC
    JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_VST3_CAN_REPLACE_VST2=0 JUCE_DISPLAY_SPLASH_SCREEN=0)
target_link_libraries(UNCERTAINSpatial
    PRIVATE juce::juce_audio_utils juce::juce_dsp
    PUBLIC  juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)
