cmake_minimum_required(VERSION 3.21)
project(ClaudeEngine LANGUAGES CXX)

# ---------------------------------------------------------------------------
# Build profile: "dev" (default) or "stable"
# ---------------------------------------------------------------------------
set(BUILD_PROFILE "dev" CACHE STRING "Build profile: dev or stable")
set_property(CACHE BUILD_PROFILE PROPERTY STRINGS "dev" "stable")

if(BUILD_PROFILE STREQUAL "stable")
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
    message(STATUS "[ClaudeEngine] Profile: stable (Release, O3, LTO)")
else()
    set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "" FORCE)
    message(STATUS "[ClaudeEngine] Profile: dev (Debug, O0, verbose logging)")
endif()

# ---------------------------------------------------------------------------
# Global C++ standard
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Binary always lands in build/ at the repository root.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build")

# ---------------------------------------------------------------------------
# LTO for stable profile
# ---------------------------------------------------------------------------
if(BUILD_PROFILE STREQUAL "stable")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        message(STATUS "[ClaudeEngine] LTO enabled")
    else()
        message(WARNING "[ClaudeEngine] LTO not supported: ${ipo_error}")
    endif()
endif()

# ---------------------------------------------------------------------------
# Fetch loguru into external/loguru-src/
# ---------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_BASE_DIR "${CMAKE_SOURCE_DIR}/external")

FetchContent_Declare(
    loguru
    GIT_REPOSITORY https://github.com/emilk/loguru.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)
set(LOGURU_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(LOGURU_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(LOGURU_INSTALL        OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(loguru)

add_subdirectory(src)
