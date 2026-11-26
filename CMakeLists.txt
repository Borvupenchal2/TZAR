cmake_minimum_required(VERSION 3.22)

project(TZAR VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Windows: static runtime -> the .vst3 runs on any PC without the VC++ redistributable
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# ------------------------------------------------------------------ JUCE
# Use a local copy if you pass -DJUCE_DIR=..., otherwise download JUCE 8 automatically.
if(DEFINED JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.15
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)
endif()

# ------------------------------------------------------------------ Plugin
juce_add_plugin(TZAR
    PRODUCT_NAME                "TZAR"
    COMPANY_NAME                "TZAR Labs"
    BUNDLE_ID                   "com.tzarlabs.tzar"
    PLUGIN_MANUFACTURER_CODE    Tzlb
    PLUGIN_CODE                 Tzar
    FORMATS                     VST3 Standalone
    IS_SYNTH                    FALSE
    NEEDS_MIDI_INPUT            TRUE
    NEEDS_MIDI_OUTPUT           FALSE
    IS_MIDI_EFFECT              FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD     FALSE
    VST3_CATEGORIES             Fx Dynamics Generator
    DESCRIPTION                 "Thermonuclear sequence engine")

juce_add_binary_data(TZARFonts SOURCES
    Resources/fonts/Orbitron-Variable.ttf
    Resources/fonts/Rajdhani-SemiBold.ttf
    Resources/fonts/ShareTechMono-Regular.ttf)

target_sources(TZAR PRIVATE
    Source/dsp/TzarEngine.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/ui/TzarLookAndFeel.cpp
    Source/ui/HudComponents.cpp)

target_include_directories(TZAR PRIVATE Source)

target_compile_definitions(TZAR PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_VST3_EMULATE_MIDI_CC_WITH_PARAMETERS=0
    JUCE_USE_MP3AUDIOFORMAT=1)

# source files contain UTF-8 (French tooltips)
if(MSVC)
    target_compile_options(TZAR PRIVATE /utf-8)
endif()

target_link_libraries(TZAR
    PRIVATE
        TZARFonts
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ------------------------------------------------------------------ dev tool: offline UI snapshots
option(TZAR_BUILD_SNAPSHOT "Build the offline UI snapshot tool" OFF)
if(TZAR_BUILD_SNAPSHOT)
    juce_add_console_app(TZARSnapshot PRODUCT_NAME "TZARSnapshot")
    target_sources(TZARSnapshot PRIVATE tools/snapshot.cpp
        Source/dsp/TzarEngine.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp
        Source/ui/TzarLookAndFeel.cpp Source/ui/HudComponents.cpp)
    target_include_directories(TZARSnapshot PRIVATE Source)
    target_compile_definitions(TZARSnapshot PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JUCE_USE_MP3AUDIOFORMAT=1)
    target_link_libraries(TZARSnapshot PRIVATE TZARFonts juce::juce_audio_utils juce::juce_dsp
                          juce::juce_recommended_config_flags)
endif()

# ------------------------------------------------------------------ dev tool: VST3 host test
option(TZAR_BUILD_HOSTTEST "Build the VST3 hosting test" OFF)
if(TZAR_BUILD_HOSTTEST)
    juce_add_console_app(TZARHostTest PRODUCT_NAME "TZARHostTest")
    target_sources(TZARHostTest PRIVATE tools/hosttest.cpp)
    target_compile_definitions(TZARHostTest PRIVATE JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(TZARHostTest PRIVATE juce::juce_audio_utils juce::juce_recommended_config_flags)
endif()
