cmake_minimum_required(VERSION 3.22)

project(WhirlDT VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build architectures for macOS" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "Minimum macOS deployment target" FORCE)
endif()

include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE)

juce_add_binary_data(WhirlDTAssets
    SOURCES
        Assets/background.png
        Assets/fresh-knob.png
        Assets/moth_logo.png
        Assets/Fonts/Oxanium-Bold.ttf
        Assets/Fonts/BarlowCondensed-Light.ttf
        Assets/Fonts/BarlowCondensed-Medium.ttf
        Assets/Fonts/Rajdhani-Medium.ttf
)

juce_add_plugin(WhirlDT
    COMPANY_NAME "Moth Production"
    BUNDLE_ID "com.mothproduction.whirldt"
    PLUGIN_MANUFACTURER_CODE MthP
    PLUGIN_CODE Wdt1
    FORMATS VST3 AU Standalone
    PRODUCT_NAME "WhirlDT"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
)

target_sources(WhirlDT PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/WhirlDTLookAndFeel.cpp
    Source/WhirlDTControls.cpp
    Source/WhirlDTFonts.cpp
)

target_compile_definitions(WhirlDT
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(WhirlDT
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        WhirlDTAssets
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
