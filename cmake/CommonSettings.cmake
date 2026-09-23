# Settings every target in this repository shares.
#
# This file does the job build/Common.props used to do. It is longer than the .props file
# was, because MSBuild switched several of these on by itself and CMake does not. Anything
# that used to be a silent default is written out here instead.

include_guard(GLOBAL)

# The folder name for the target architecture, x64 or Win32.
#
# A Visual Studio generator hands it over. Every other generator leaves
# CMAKE_VS_PLATFORM_NAME empty, including the NMake Makefiles that Rider reaches for when it
# is left to choose. Empty used to collapse bin/<platform>/<configuration> into
# bin/<configuration> and skip the 32 bit settings below without saying anything, so the
# pointer size answers when the generator will not.
if(CMAKE_VS_PLATFORM_NAME)
    set(GE5_PLATFORM "${CMAKE_VS_PLATFORM_NAME}")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(GE5_PLATFORM "Win32")
else()
    set(GE5_PLATFORM "x64")
endif()

#
# Applies the shared build settings to one target.
#
#   ge5_apply_common_settings(<target> [SDL_CHECK ON|OFF])
#
# SDL_CHECK defaults to ON. Only MemoryMonitorTests passes OFF, and the reason is written
# at that call in the top level CMakeLists.txt.
#
function(ge5_apply_common_settings target)
    cmake_parse_arguments(ARG "" "SDL_CHECK" "" ${ARGN})
    if(NOT DEFINED ARG_SDL_CHECK)
        set(ARG_SDL_CHECK ON)
    endif()

    target_compile_features(${target} PRIVATE cxx_std_20)

    # _DEBUG is what turns the monitor's checked paths on. NDEBUG comes from CMake's own
    # Release flags, so it is not repeated here.
    target_compile_definitions(${target} PRIVATE
        _CONSOLE
        UNICODE
        _UNICODE
        $<$<CONFIG:Debug>:_DEBUG>
    )

    # The old project defined WIN32 on the 32 bit builds only. Kept, so nothing shifts.
    if(GE5_PLATFORM STREQUAL "Win32")
        target_compile_definitions(${target} PRIVATE WIN32)
    endif()

    target_compile_options(${target} PRIVATE
        /W3
        /permissive-
        /FC
        /diagnostics:column
        # Just My Code, on in a Debug MSBuild project and off in CMake
        $<$<CONFIG:Debug>:/JMC>
        # /Oi and /Gy came with MaxSpeed and FunctionLevelLinking in Common.props
        $<$<CONFIG:Release>:/Oi>
        $<$<CONFIG:Release>:/Gy>
    )

    # 32 bit MSBuild builds kept the frame pointer. /O2 would drop it otherwise, so this
    # one is not cosmetic in Release.
    if(GE5_PLATFORM STREQUAL "Win32")
        target_compile_options(${target} PRIVATE /Oy-)
    endif()

    if(ARG_SDL_CHECK)
        target_compile_options(${target} PRIVATE /sdl)
    else()
        target_compile_options(${target} PRIVATE /sdl-)
    endif()

    # Debug used edit and continue info, Release used a plain program database. CMake gives
    # Release no debug info at all unless it is asked, and Common.props asked.
    set_property(TARGET ${target} PROPERTY
        MSVC_DEBUG_INFORMATION_FORMAT
        "$<$<CONFIG:Debug>:EditAndContinue>$<$<CONFIG:Release>:ProgramDatabase>")

    # WholeProgramOptimization in Common.props, which is /GL when compiling and /LTCG when
    # linking. It stays on: the saved demo output was captured with it, and whole program
    # optimization is allowed to remove an allocation the program never really needs.
    set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)

    target_link_options(${target} PRIVATE
        $<$<CONFIG:Release>:/DEBUG>
        $<$<CONFIG:Release>:/OPT:REF>
        $<$<CONFIG:Release>:/OPT:ICF>
        # /LTCG already forces this, but Common.props said it out loud and so does this
        $<$<CONFIG:Release>:/INCREMENTAL:NO>
    )
endfunction()
