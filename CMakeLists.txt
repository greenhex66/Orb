cmake_minimum_required(VERSION 3.22)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

project(HexQOrbit VERSION 1.0.0)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(HexQOrbit
    COMPANY_NAME "HEX-Q"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    PLUGIN_MANUFACTURER_CODE Hexq
    PLUGIN_CODE Orbt
    FORMATS VST3
    PRODUCT_NAME "HEX-Q ORBIT"
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Spatial)

target_sources(HexQOrbit PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
    Source/Diag.h
    Source/Parameters/OrbitParams.cpp
    Source/Parameters/OrbitParams.h
    Source/DSP/Biquad.h
    Source/DSP/OrbitEngine.h
    Source/HexQ/HexQTheme.h
    Source/HexQ/HexQLookAndFeel.cpp
    Source/HexQ/HexQLookAndFeel.h
    Source/HexQ/HexQHeader.h
    Source/HexQ/HexQFooter.h
    Source/Orbit/ModeButton.h
    Source/Orbit/MonoMeter.h
    Source/Orbit/PanPreview.h)

target_compile_definitions(HexQOrbit PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(HexQOrbit
    PRIVATE juce::juce_audio_utils
    PUBLIC juce::juce_recommended_config_flags)

# Offline DSP self-test (pure C++, no JUCE): run with `ctest -C Release`
enable_testing()
add_executable(HexQOrbitDspTest tests/orbit_dsp_test.cpp)
target_include_directories(HexQOrbitDspTest PRIVATE Source)
add_test(NAME orbit_dsp_self_test COMMAND HexQOrbitDspTest)
