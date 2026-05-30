cmake_minimum_required(VERSION 3.16)
project(BlackjackGame VERSION 1.0.0 LANGUAGES CXX)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Set rpath for macOS Homebrew SDL2
if(APPLE)
    list(APPEND CMAKE_EXE_LINKER_FLAGS "-Wl,-rpath,/opt/homebrew/lib")
endif()

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# Enable testing
enable_testing()

# Core configuration
option(BUILD_TESTS "Build unit tests" ON)
option(BUILD_ASSETS "Build asset generation tools" OFF)

# ============================================================================
# SDL2 Dependencies
# ============================================================================
find_package(PkgConfig QUIET)

if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 QUIET SDL2)
    pkg_check_modules(SDL2_IMAGE QUIET SDL2_image)
    pkg_check_modules(SDL2_TTF QUIET SDL2_ttf)
    pkg_check_modules(SDL2_MIXER QUIET SDL2_mixer)
else()
    # Fallback: try to use sdl2-config directly
    find_program(SDL2_CONFIG sdl2-config)
    if(SDL2_CONFIG)
        execute_process(COMMAND ${SDL2_CONFIG} --cflags OUTPUT_VARIABLE SDL2_CFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
        execute_process(COMMAND ${SDL2_CONFIG} --libs OUTPUT_VARIABLE SDL2_LDFLAGS OUTPUT_STRIP_TRAILING_WHITESPACE)
    endif()
    find_package(SDL2 CONFIG REQUIRED)
endif()

# Fetch nlohmann_json for JSON serialization
include(FetchContent)
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

# ============================================================================
# Catch2 Testing Framework
# ============================================================================
if(BUILD_TESTS)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.4
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(Catch2)
    
    # Compiler flags for testing
    add_compile_definitions(CATCH_CONFIG_FAST_COMPILE)
    add_compile_definitions(CATCH_CONFIG_NO_POSIX_SIGNALS)
    
    # Include Catch2's test discovery module
    include(${Catch2_SOURCE_DIR}/extras/CatchAddTests.cmake)
endif()

# ============================================================================
# Compiler Warnings
# ============================================================================
if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
endif()

# ============================================================================
# Source Directories
# ============================================================================
add_subdirectory(src/core)
add_subdirectory(src/game)
add_subdirectory(src/ai)
add_subdirectory(src/gui)
add_subdirectory(src/audio)
add_subdirectory(src/network)
add_subdirectory(src/persistence)
add_subdirectory(src/platform)

# Main executable
add_subdirectory(src)

# Tests
if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# ============================================================================
# Installation
# ============================================================================
install(DIRECTORY include/ DESTINATION include)
install(DIRECTORY assets/ DESTINATION share/blackjack/assets)