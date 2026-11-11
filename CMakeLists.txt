cmake_minimum_required(VERSION 3.22)

project(OpenSmacker VERSION 1.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(MSVC)
    add_compile_options(/utf-8)
endif()

option(OS_BUILD_PLUGIN "Build the plugin" ON)
option(OS_BUILD_TESTS "Build the unit tests" ON)

include(FetchContent)

add_library(opensmacker_core STATIC
    Source/Core/SmackSynth.cpp
    Source/Core/Effects.cpp
    Source/Core/SmackEngine.cpp)
target_include_directories(opensmacker_core PUBLIC Source/Core)
set_target_properties(opensmacker_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(OS_BUILD_PLUGIN)
    FetchContent_Declare(juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(juce)

    juce_add_binary_data(SmackerAssets SOURCES
        assets/opensmacker-faceplate.png
        assets/opensmacker-stage.png)
    set_target_properties(SmackerAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

    juce_add_plugin(OpenSmacker
        VERSION "${PROJECT_VERSION}"
        COMPANY_NAME "Alan Marcero"
        BUNDLE_ID "com.alanmarcero.opensmacker"
        PRODUCT_NAME "openSmacker"
        PLUGIN_MANUFACTURER_CODE Amrc
        PLUGIN_CODE Osmk
        FORMATS VST3 AU LV2 Standalone
        LV2URI "https://github.com/alanmarcero/openSmacker"
        VST3_CATEGORIES Instrument Drum Synth
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT FALSE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD FALSE)

    target_sources(OpenSmacker PRIVATE
        Source/Plugin/Parameters.cpp
        Source/Plugin/Presets.cpp
        Source/Plugin/EdmLookAndFeel.cpp
        Source/Plugin/Controls.cpp
        Source/Plugin/Panels.cpp
        Source/Plugin/PluginProcessor.cpp
        Source/Plugin/PluginEditor.cpp)

    target_include_directories(OpenSmacker PRIVATE
        Source/Core
        Source/Plugin)

    target_compile_definitions(OpenSmacker PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0)

    target_link_libraries(OpenSmacker
        PRIVATE
            opensmacker_core
            SmackerAssets
            juce::juce_audio_processors
            juce::juce_audio_utils
            juce::juce_gui_basics
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endif()

if(OS_BUILD_TESTS)
    FetchContent_Declare(Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.9.1
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(Catch2)

    add_executable(opensmacker_tests
        Tests/SmackSynthTests.cpp
        Tests/EffectsTests.cpp
        Tests/SmackEngineTests.cpp)

    target_include_directories(opensmacker_tests PRIVATE Tests)
    target_link_libraries(opensmacker_tests PRIVATE opensmacker_core Catch2::Catch2WithMain)

    enable_testing()
    include(Catch)
    catch_discover_tests(opensmacker_tests)

    if(OS_BUILD_PLUGIN)
        add_executable(opensmacker_plugin_tests Tests/PluginTests.cpp)
        target_include_directories(opensmacker_plugin_tests PRIVATE Source/Plugin Source/Core)
        target_compile_definitions(opensmacker_plugin_tests PRIVATE $<TARGET_PROPERTY:OpenSmacker,COMPILE_DEFINITIONS>)
        target_link_libraries(opensmacker_plugin_tests PRIVATE OpenSmacker SmackerAssets juce::juce_audio_utils Catch2::Catch2WithMain)
        catch_discover_tests(opensmacker_plugin_tests)
    endif()
endif()
