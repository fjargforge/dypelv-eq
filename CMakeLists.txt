cmake_minimum_required(VERSION 3.22)
project(DypelvEQ VERSION 0.1.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "")
# Windows: link the C++ runtime statically, so the plugin runs on a PC without the Visual C++ redistributable
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

# JUCE 8.0.4 is shipped inside the project (folder JUCE): nothing to download, git is not needed.
# If that folder is missing, it is fetched from GitHub instead.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

if(APPLE)
    set(DYPELV_FORMATS AU VST3 Standalone)
else()
    set(DYPELV_FORMATS VST3 Standalone)
endif()

juce_add_plugin(DypelvEQ
    COMPANY_NAME "Fjarg"
    PLUGIN_MANUFACTURER_CODE Fjrg
    PLUGIN_CODE Dypl
    FORMATS ${DYPELV_FORMATS}
    PRODUCT_NAME "Dypelv EQ"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon_1024.png"      # the standalone app icon
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon_256.png"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    VST3_CATEGORIES Fx EQ)

target_sources(DypelvEQ PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(DypelvEQ PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(DypelvEQ
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
