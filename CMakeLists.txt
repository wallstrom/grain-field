cmake_minimum_required(VERSION 3.22)

# Build universal binaries (Apple silicon + Intel) on macOS.
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "macOS architectures")
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")

project(GrainField VERSION 0.7.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(GF_BUILD_TESTS "Build the offline render test" OFF)

# JUCE: use a local copy if JUCE_DIR is given, otherwise download it.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.15
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)

endif()

juce_add_plugin(GrainField
    COMPANY_NAME "Grain Field Audio"
    BUNDLE_ID com.grainfieldaudio.grainfield
    PRODUCT_NAME "Grain Field"
    PLUGIN_MANUFACTURER_CODE Gfau
    PLUGIN_CODE Grfd
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Synth
    FORMATS AU VST3 Standalone)

set(GF_ENGINE_SOURCES
    Source/Params.cpp
    Source/SourceLibrary.cpp
    Source/Engine.cpp
    Source/Scenes.cpp
    Source/PluginProcessor.cpp)

set(GF_UI_SOURCES
    Source/PluginEditor.cpp
    Source/UI/Theme.cpp
    Source/UI/Widgets.cpp
    Source/UI/Views.cpp)

# Fonts embedded in the plugin: DM Mono and Instrument Serif (both SIL Open Font License).
juce_add_binary_data(GrainFieldFonts SOURCES
    Resources/Fonts/DMMono-Regular.ttf
    Resources/Fonts/DMMono-Medium.ttf
    Resources/Fonts/InstrumentSerif-Italic.ttf)
set_target_properties(GrainFieldFonts PROPERTIES POSITION_INDEPENDENT_CODE ON)

# The built-in sounds (Ogg Vorbis). Instruments from the Versilian Community Sample Library (CC0).
file(GLOB GF_SOUND_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Resources/Sounds/*.ogg")
juce_add_binary_data(GrainFieldSounds HEADER_NAME SoundData.h NAMESPACE SoundData SOURCES ${GF_SOUND_FILES})
set_target_properties(GrainFieldSounds PROPERTIES POSITION_INDEPENDENT_CODE ON)

target_sources(GrainField PRIVATE
    ${GF_ENGINE_SOURCES}
    ${GF_UI_SOURCES})

target_compile_definitions(GrainField PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1)

target_link_libraries(GrainField
    PRIVATE
        GrainFieldFonts
        GrainFieldSounds
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(GF_BUILD_TESTS)
    juce_add_console_app(GrainFieldRenderTest PRODUCT_NAME "GrainFieldRenderTest")
    target_sources(GrainFieldRenderTest PRIVATE Tests/RenderTest.cpp ${GF_ENGINE_SOURCES})
    target_compile_definitions(GrainFieldRenderTest PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_MP3AUDIOFORMAT=1 GF_HEADLESS=1 JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_Name="Grain Field" JucePlugin_IsSynth=1 JucePlugin_WantsMidiInput=1
        JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0)
    target_link_libraries(GrainFieldRenderTest PRIVATE GrainFieldSounds juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags juce::juce_recommended_warning_flags)
    # Renders each page of the interface to PNG files.
    juce_add_gui_app(GrainFieldSnapshot PRODUCT_NAME "GrainFieldSnapshot")
    target_sources(GrainFieldSnapshot PRIVATE Tests/Snapshot.cpp ${GF_ENGINE_SOURCES} ${GF_UI_SOURCES})
    target_compile_definitions(GrainFieldSnapshot PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_MP3AUDIOFORMAT=1 JUCE_MODAL_LOOPS_PERMITTED=1
        JucePlugin_Name="Grain Field" JucePlugin_IsSynth=1 JucePlugin_WantsMidiInput=1
        JucePlugin_ProducesMidiOutput=0 JucePlugin_IsMidiEffect=0 JucePlugin_VersionString="0.7.0")
    target_link_libraries(GrainFieldSnapshot PRIVATE GrainFieldFonts GrainFieldSounds juce::juce_audio_utils juce::juce_dsp
        juce::juce_recommended_config_flags)

endif()
