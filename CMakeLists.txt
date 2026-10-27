cmake_minimum_required(VERSION 3.22)
project(QV89 VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "10.13" CACHE STRING "Minimum macOS version")
endif()

# JUCE: use a local checkout if present (JUCE_DIR or ./JUCE), otherwise download it.
if(DEFINED JUCE_DIR AND EXISTS "${JUCE_DIR}/CMakeLists.txt")
    add_subdirectory(${JUCE_DIR} JUCE)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW ON)
    FetchContent_MakeAvailable(JUCE)
endif()

set(QV89_FORMATS VST3 Standalone)
if(APPLE)
    list(APPEND QV89_FORMATS AU)
endif()

juce_add_plugin(QV89
    COMPANY_NAME "Roany Romano"
    COMPANY_WEBSITE ""
    BUNDLE_ID "com.roanyromano.qv89"
    PLUGIN_MANUFACTURER_CODE Rony
    PLUGIN_CODE Qv89
    FORMATS ${QV89_FORMATS}
    PRODUCT_NAME "QV89"
    DESCRIPTION "Quad multi-effects inspired by the 1989 Alesis QuadraVerb"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES "Fx" "Reverb" "Delay" "Modulation"
    AU_MAIN_TYPE kAudioUnitType_MusicEffect)

juce_generate_juce_header(QV89)

# Optional: embed an original QuadraVerb EPROM image to get the 90 factory programs as presets.
# Keep the ROM out of public repositories (it is Alesis copyrighted firmware).
set(QV89_ROM_FILE "${CMAKE_CURRENT_SOURCE_DIR}/rom/qv_v107.bin" CACHE FILEPATH "QuadraVerb 27C512 EPROM image (optional)")
if(EXISTS "${QV89_ROM_FILE}")
    message(STATUS "QV89: embedding QuadraVerb ROM ${QV89_ROM_FILE}")
    configure_file("${QV89_ROM_FILE}" "${CMAKE_CURRENT_BINARY_DIR}/rom/qv_rom.bin" COPYONLY)
    juce_add_binary_data(QV89RomData SOURCES "${CMAKE_CURRENT_BINARY_DIR}/rom/qv_rom.bin")
    target_link_libraries(QV89 PRIVATE QV89RomData)
    target_compile_definitions(QV89 PRIVATE QV89_HAS_ROM=1)
else()
    message(STATUS "QV89: no ROM image, building with QV89 original presets only")
    target_compile_definitions(QV89 PRIVATE QV89_HAS_ROM=0)
endif()

target_sources(QV89 PRIVATE
    Source/Rom.cpp
    Source/Params.cpp
    Source/Sysex.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/EmuEngine.cpp
    Source/emu/Cpu8051.cpp
    Source/emu/QvMachine.cpp)

target_compile_definitions(QV89 PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(QV89 PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags)

# Offline DSP test (no JUCE)
option(QV89_BUILD_TESTS "Build the offline DSP test" ON)
if(QV89_BUILD_TESTS)
    add_executable(qv89_dsp_test tests/dsp_test.cpp Source/Params.cpp Source/Sysex.cpp)
    add_executable(qv89_rom_test tests/rom_test.cpp Source/Rom.cpp Source/Params.cpp Source/Sysex.cpp)
    if(NOT MSVC)
        target_compile_options(qv89_dsp_test PRIVATE -O2)
    endif()
endif()

# Headless processor test (links the plugin's shared code); off by default
option(QV89_PROC_TEST "Build the headless processor test" OFF)
if(QV89_PROC_TEST)
    add_executable(qv89_proc_test tests/proc_test.cpp)
    target_link_libraries(qv89_proc_test PRIVATE QV89)
    target_include_directories(qv89_proc_test PRIVATE Source $<TARGET_PROPERTY:QV89,INCLUDE_DIRECTORIES>)
    target_compile_definitions(qv89_proc_test PRIVATE $<TARGET_PROPERTY:QV89,COMPILE_DEFINITIONS>)
endif()
