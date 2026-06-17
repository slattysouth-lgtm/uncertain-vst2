cmake_minimum_required(VERSION 3.22)

project(BLOODMONEYMaster VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ------------------------------------------------------------------------------
# JUCE : téléchargé automatiquement (necessite une connexion internet au 1er build)
# Tag 7.0.12 = stable et eprouve. Pour passer a JUCE 8, remplace par 8.0.4 par ex.
# ------------------------------------------------------------------------------
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG        7.0.12
    GIT_SHALLOW    TRUE)
FetchContent_MakeAvailable(JUCE)

# ------------------------------------------------------------------------------
# AAX (Pro Tools) — OPTIONNEL. Desactive par defaut.
# Pour l'activer : -DBM_BUILD_AAX=ON  et fournis le SDK AAX d'Avid.
# Le binaire devra ensuite etre signe PACE pour charger dans un Pro Tools commercial.
# ------------------------------------------------------------------------------
option(BM_BUILD_AAX "Construire aussi la cible AAX (necessite le SDK AAX d'Avid)" OFF)
set(BM_PLUGIN_FORMATS VST3 Standalone)

if(BM_BUILD_AAX)
    if(NOT DEFINED AAX_SDK_PATH)
        message(FATAL_ERROR "BM_BUILD_AAX=ON mais AAX_SDK_PATH n'est pas defini. Passe -DAAX_SDK_PATH=C:/chemin/vers/aax-sdk")
    endif()
    juce_set_aax_sdk_path(${AAX_SDK_PATH})
    list(APPEND BM_PLUGIN_FORMATS AAX)
endif()

# ------------------------------------------------------------------------------
juce_add_plugin(BLOODMONEYMaster
    COMPANY_NAME                "BLOODMONEY"
    PRODUCT_NAME                "BLOODMONEY Master"
    PLUGIN_MANUFACTURER_CODE    Bldm
    PLUGIN_CODE                 Bmms
    FORMATS                     ${BM_PLUGIN_FORMATS}
    VST3_CATEGORIES             Fx Mastering Dynamics
    AAX_CATEGORY                AAX_ePlugInCategory_Dynamics
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            FALSE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    COPY_PLUGIN_AFTER_BUILD     TRUE)

target_sources(BLOODMONEYMaster PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(BLOODMONEYMaster PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(BLOODMONEYMaster
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
