# GameEngines5
#
# Five targets: two static libraries and three programs. Each program has its own main(),
# which is why each one is its own target.
#
# Configure one build tree per platform, each holding both configurations:
#
#   cmake --preset x64
#   cmake --build --preset x64-Debug
#
# CMakePresets.json holds the generator, the toolset and the build directory, so an IDE picks
# the same ones the scripts do. The toolset is pinned on purpose: a machine with more than one
# set of Build Tools installed will otherwise take whichever is newest, and the saved demo
# output was captured with v143.

cmake_minimum_required(VERSION 3.25)

# C is here for SDL3, which is a C library. Our own code is all C++.
project(GameEngines5 LANGUAGES CXX C)

if(NOT MSVC)
    message(FATAL_ERROR
        "MemoryMonitor uses SRWLOCK and _aligned_malloc, which are Microsoft only. "
        "See \"It does not build outside Windows and MSVC\" in README.md.")
endif()

# Four configurations existed and four survive. CMake would otherwise add RelWithDebInfo
# and MinSizeRel, which nothing here has ever been built with.
if(CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)
endif()

# CMake hands MSVC /DWIN32 and /D_WINDOWS on every platform. The old build defined WIN32 on
# 32 bit only and never defined _WINDOWS, so they are taken out here and WIN32 is put back
# per platform in ge5_apply_common_settings.
string(REGEX REPLACE "[-/]D[ ]*WIN32" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REGEX REPLACE "[-/]D[ ]*_WINDOWS" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(STRIP "${CMAKE_CXX_FLAGS}" CMAKE_CXX_FLAGS)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CommonSettings)

# Programs and libraries land here, one folder per platform and configuration, exactly where
# MSBuild put them. GE5_PLATFORM comes from CommonSettings.cmake and holds x64 or Win32 under
# any generator. The $<CONFIG> is written out so CMake does not append a second config folder
# of its own.
set(GE5_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/bin/${GE5_PLATFORM}/$<CONFIG>")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${GE5_OUTPUT_DIR}")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${GE5_OUTPUT_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${GE5_OUTPUT_DIR}")
set(CMAKE_PDB_OUTPUT_DIRECTORY "${GE5_OUTPUT_DIR}")

# Start the IDE on something that runs. The engine is 64 bit only, so on Win32 it is the demo
# that the IDE opens on.
if(GE5_PLATFORM STREQUAL "x64")
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT GameEngines5)
else()
    set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT MemoryMonitorDemo)
endif()


# ============================== MemoryMonitor ==============================
#
# Two files and nothing else. Anything that links this gets the replaced operator new and
# operator delete, so long as it also calls into the library somewhere. A linker only takes
# the parts of a static library that something asks for.

add_library(MemoryMonitor STATIC
    src/utils/MemoryMonitor.cpp
    include/utils/MemoryMonitor.h
)
target_include_directories(MemoryMonitor PUBLIC include)
ge5_apply_common_settings(MemoryMonitor)


# ============================== MathLibrary ==============================
#
# The headers are split out into their own interface target. MemoryMonitorDemo needs to
# reach Vector.h but must not link the library, and this says so in one line.

add_library(MathLibraryHeaders INTERFACE)
target_include_directories(MathLibraryHeaders INTERFACE
    external/MathLibrary/include
    external/glm
)

# Globbed because these headers are browsing material for the IDE. They compile nothing:
# the three .cpp files below are the build, and they are listed by hand.
file(GLOB MATHLIBRARY_HEADERS CONFIGURE_DEPENDS "external/MathLibrary/include/*.h")

add_library(MathLibrary STATIC
    # DebugUtlities.cpp needs glm, vendored next door under external/glm
    external/MathLibrary/include/DebugUtlities.cpp
    external/MathLibrary/include/Fourier.cpp
    external/MathLibrary/include/Randomizer.cpp
    ${MATHLIBRARY_HEADERS}
)
target_link_libraries(MathLibrary PUBLIC MathLibraryHeaders)
ge5_apply_common_settings(MathLibrary)


# ============================== GameEngines5 ==============================
#
# The engine. Main.cpp calls MemoryMonitor::GetStats(), and it has to: that call is what
# makes the linker pull in the operator new and operator delete replacements. Without it the
# engine runs unwatched and says nothing about it.
#
# 64 bit only, and the Vulkan SDK is the reason. It ships Lib and no Lib32, so there is no 32
# bit vulkan-1.lib to link against. Everything inside this branch, SDL3 included, is skipped
# on Win32, which keeps that configure quick and free of network access. The two libraries,
# the demo and the tests still build for both platforms, and those are what
# scripts/capture-output.sh and scripts/run-tests.bat measure.

if(GE5_PLATFORM STREQUAL "x64")

    # SDL3 is built from source at a pinned tag rather than vendored as binaries, so a fresh
    # clone of this repository builds without a prepared machine. Shared, because that is how
    # SDL ships and what it recommends.
    include(FetchContent)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.18
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(SDL3)

    # No step here copies SDL3.dll next to the exe, and none is needed. SDL is brought in as a
    # subdirectory after CMAKE_RUNTIME_OUTPUT_DIRECTORY is set above, so its own targets inherit
    # it and SDL3.dll is built straight into bin/<platform>/<configuration>/. Scope those output
    # directories per target one day and the dll stops arriving, and the engine will not start.

    add_executable(GameEngines5
        Main.cpp
        src/core/Window.cpp
        include/core/Window.h
    )
    # So the renderer headers are reached as "core/Window.h"
    target_include_directories(GameEngines5 PRIVATE src)
    target_link_libraries(GameEngines5 PRIVATE MemoryMonitor MathLibrary SDL3::SDL3)
    ge5_apply_common_settings(GameEngines5)

else()

    message(STATUS
        "GameEngines5 and SDL3 are skipped on ${GE5_PLATFORM}. The Vulkan SDK has no 32 bit "
        "vulkan-1.lib. MemoryMonitor, MathLibrary, the demo and the tests still build.")

endif()


# ============================== MemoryMonitorDemo ==============================
#
# A walk through every path the monitor watches. It links MemoryMonitor and the math headers,
# and nothing else, on purpose: the point is a clean allocation picture.

add_executable(MemoryMonitorDemo demo/MemoryMonitorDemo.cpp)
target_link_libraries(MemoryMonitorDemo PRIVATE MemoryMonitor MathLibraryHeaders)
ge5_apply_common_settings(MemoryMonitorDemo)


# ============================== MemoryMonitorTests ==============================
#
# /sdl off, and only here.
#
# /sdl overwrites a pointer with a junk address as soon as it is deleted, so a stale pointer
# cannot be used again. The tests delete the same pointer twice on purpose, and under /sdl the
# second delete hands the monitor a wild address instead of a stale one. The monitor reads the
# header in front of the block to decide whether to refuse the free, and reading it at a wild
# address is an access violation. See "It does not survive a truly wild pointer" in the README.
#
# Everything else in this file keeps /sdl.

add_executable(MemoryMonitorTests tests/MemoryMonitorTests.cpp)
target_link_libraries(MemoryMonitorTests PRIVATE MemoryMonitor)
ge5_apply_common_settings(MemoryMonitorTests SDL_CHECK OFF)

# One program, one test. It counts its own checks and returns non zero if any of them fail.
# A crash comes back as a large negative number, and CTest calls that a failure too, which
# is the trap scripts/run-tests.bat used to have to sidestep by hand.
#
#   ctest --test-dir build/cmake-x64 -C Debug -V
enable_testing()
add_test(NAME MemoryMonitorTests COMMAND MemoryMonitorTests)
