cmake_minimum_required(VERSION 3.24)

project(WhirlDT VERSION 0.4.4)

include(CTest)

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

FetchContent_Declare(
    signalsmith-stretch
    GIT_REPOSITORY https://github.com/Signalsmith-Audio/signalsmith-stretch.git
    GIT_TAG 57b93f4e9206a089a45387eaa39bdc9f310d3308
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE signalsmith-stretch)

# Older releases predate the dependency's own CMake interface target.  Keep a
# fallback so the project remains explicit if that upstream detail changes.
if(NOT TARGET signalsmith-stretch)
    add_library(signalsmith-stretch INTERFACE)
    target_include_directories(signalsmith-stretch SYSTEM INTERFACE ${signalsmith-stretch_SOURCE_DIR})
endif()

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
    Source/WhirlDTDSP.cpp
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
        signalsmith-stretch
        WhirlDTAssets
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

if(BUILD_TESTING)
    juce_add_console_app(WhirlDTDSPTests PRODUCT_NAME "WhirlDT DSP Tests")

    target_sources(WhirlDTDSPTests PRIVATE
        Tests/DSPTests.cpp
        Source/WhirlDTDSP.cpp
    )

    target_include_directories(WhirlDTDSPTests PRIVATE Source)

    target_link_libraries(WhirlDTDSPTests
        PRIVATE
            juce::juce_dsp
            signalsmith-stretch
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    add_test(NAME WhirlDTDSPTests COMMAND WhirlDTDSPTests)

    # Manual/offline validation utility for the large hardware profiling WAVs.
    # It is intentionally not registered with CTest because those recordings
    # live on an external volume and are not part of the repository.
    juce_add_console_app(WhirlDTProfileValidation PRODUCT_NAME "WhirlDT Profile Validation")

    target_sources(WhirlDTProfileValidation PRIVATE
        Tests/ProfileValidation.cpp
        Source/WhirlDTDSP.cpp
    )

    target_include_directories(WhirlDTProfileValidation PRIVATE Source)

    target_link_libraries(WhirlDTProfileValidation
        PRIVATE
            juce::juce_audio_formats
            juce::juce_dsp
            signalsmith-stretch
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )

    juce_add_console_app(WhirlDTGuitarRender PRODUCT_NAME "WhirlDT Guitar Render")

    target_sources(WhirlDTGuitarRender PRIVATE
        Tests/GuitarRender.cpp
        Source/WhirlDTDSP.cpp
    )

    target_include_directories(WhirlDTGuitarRender PRIVATE Source)

    target_link_libraries(WhirlDTGuitarRender
        PRIVATE
            juce::juce_audio_formats
            juce::juce_dsp
            signalsmith-stretch
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_warning_flags
    )
endif()
