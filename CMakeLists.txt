cmake_minimum_required(VERSION 3.22)
project(audio-server VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Fetch JUCE
include(FetchContent)

FetchContent_Declare(
    JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(JUCE)

# cpp-httplib (header-only, vendored in deps/)
add_library(httplib INTERFACE)
target_include_directories(httplib INTERFACE ${CMAKE_SOURCE_DIR}/deps/cpp-httplib)

# Main executable
add_executable(audio-server
    src/Main.cpp
    src/Config.cpp
    src/AudioEngine.cpp
    src/ApiServer.cpp
    src/transport/TcpPcmBackend.cpp
)

target_include_directories(audio-server PRIVATE ${CMAKE_SOURCE_DIR}/src)

target_link_libraries(audio-server
    PRIVATE
        juce::juce_audio_devices
        juce::juce_audio_utils
        juce::juce_core
        httplib
)

# JUCE module definitions
target_compile_definitions(audio-server
    PRIVATE
        JUCE_STANDALONE_APPLICATION=1
        JUCE_USE_CURL=0
        JUCE_WEB_BROWSER=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
)

# Platform-specific settings
if(APPLE)
    target_link_libraries(audio-server PRIVATE
        "-framework CoreAudio"
        "-framework CoreMIDI"
        "-framework AudioToolbox"
        "-framework Accelerate"
        "-framework CoreFoundation"
        "-framework CoreServices"
        "-framework IOKit"
        "-framework AppKit"
    )
elseif(UNIX)
    find_package(ALSA REQUIRED)
    target_link_libraries(audio-server PRIVATE ${ALSA_LIBRARIES})
    target_include_directories(audio-server PRIVATE ${ALSA_INCLUDE_DIRS})
    find_package(Threads REQUIRED)
    target_link_libraries(audio-server PRIVATE Threads::Threads)
elseif(WIN32)
    target_link_libraries(audio-server PRIVATE winmm)
endif()

# Install target
install(TARGETS audio-server RUNTIME DESTINATION bin)
