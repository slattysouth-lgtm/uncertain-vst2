cmake_minimum_required(VERSION 3.22)

project(UNCERTAINPolish VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# AAX (Pro Tools) optionnel : -DBM_BUILD_AAX=ON -DAAX_SDK_PATH=...
option(BM_BUILD_AAX "Construire aussi la cible AAX (SDK AAX requis)" OFF)
set(BM_PLUGIN_FORMATS VST3 Standalone)
if(BM_BUILD_AAX)
    if(NOT DEFINED AAX_SDK_PATH)
        message(FATAL_ERROR "BM_BUILD_AAX=ON mais AAX_SDK_PATH non defini.")
    endif()
    juce_set_aax_sdk_path(${AAX_SDK_PATH})
    list(APPEND BM_PLUGIN_FORMATS AAX)
endif()

juce_add_plugin(UNCERTAINPolish
    COMPANY_NAME                "UNCERTAIN"
    PRODUCT_NAME                "UNCERTAIN Polish"
    PLUGIN_MANUFACTURER_CODE    Unct
    PLUGIN_CODE                 Uplh
    FORMATS                     ${BM_PLUGIN_FORMATS}
    VST3_CATEGORIES             Fx Mastering EQ Dynamics
    AAX_CATEGORY                AAX_ePlugInCategory_Dynamics
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    COPY_PLUGIN_AFTER_BUILD     TRUE)

target_sources(UNCERTAINPolish PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(UNCERTAINPolish PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(UNCERTAINPolish
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
