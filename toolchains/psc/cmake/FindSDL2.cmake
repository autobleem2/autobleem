# SDL2 for the PlayStation Classic toolchain (toolchains/psc/PSCtoolchainV8.cmake).
#
# The console's sysroot has SDL2 2.0.4 with SDL2_image, SDL2_mixer and SDL2_ttf - headers in
# usr/include/SDL2, unversioned .so links in usr/lib - but no sdl2-config.cmake (SDL only started shipping
# one with 2.0.12), so the stock find_package(SDL2) finds nothing. lib_ableem/CMakeLists.txt's non-MinGW
# branch does one find_package(SDL2 REQUIRED) and then links the bare names SDL2, SDL2_image, SDL2_mixer,
# SDL2_ttf, so this module defines all four as IMPORTED targets over the sysroot's .so files. The headers
# need no include directory of their own: lib_ableem includes <SDL2/...>, and ${CMAKE_SYSROOT}/usr/include
# is on the compiler's default search path.
#
# (pcsx-ab's copy of this module also rewrites SDL_config.h to drop SDL_VIDEO_DRIVER_X11, which the sysroot
# declares without having any X11 headers. That only bites SDL_syswm.h, which lib_ableem never includes.)

set(_ab_psc_sdl2_libdir "${CMAKE_SYSROOT}/usr/lib")

if (NOT EXISTS "${CMAKE_SYSROOT}/usr/include/SDL2/SDL.h" OR NOT EXISTS "${_ab_psc_sdl2_libdir}/libSDL2.so")
    message(FATAL_ERROR "SDL2 not found in the PSC sysroot ${CMAKE_SYSROOT}")
endif()

function(_ab_psc_add_sdl2_target name libname)
    if (NOT EXISTS "${_ab_psc_sdl2_libdir}/${libname}")
        message(FATAL_ERROR "${libname} not found in the PSC sysroot ${_ab_psc_sdl2_libdir}")
    endif()
    if (NOT TARGET ${name})
        add_library(${name} UNKNOWN IMPORTED)
    endif()
    set_target_properties(${name} PROPERTIES IMPORTED_LOCATION "${_ab_psc_sdl2_libdir}/${libname}")
endfunction()

_ab_psc_add_sdl2_target(SDL2       libSDL2.so)
_ab_psc_add_sdl2_target(SDL2_image libSDL2_image.so)
_ab_psc_add_sdl2_target(SDL2_mixer libSDL2_mixer.so)
_ab_psc_add_sdl2_target(SDL2_ttf   libSDL2_ttf.so)

set(SDL2_FOUND TRUE)
set(SDL2_INCLUDE_DIRS "${CMAKE_SYSROOT}/usr/include")
set(SDL2_LIBRARIES SDL2 SDL2_image SDL2_mixer SDL2_ttf)

# The same libraries under the names a third-party project expects. Upstream SDL2's own config package
# exports SDL2::SDL2 and friends, so any game whose CMakeLists says target_link_libraries(game
# SDL2::SDL2) builds for this target unmodified - which is how the Apps are built (OpenJazz, Chocolate
# Doom). Our own targets link the bare names and are left alone.
_ab_psc_add_sdl2_target(SDL2::SDL2       libSDL2.so)
_ab_psc_add_sdl2_target(SDL2::SDL2_image libSDL2_image.so)
_ab_psc_add_sdl2_target(SDL2::SDL2_mixer libSDL2_mixer.so)
_ab_psc_add_sdl2_target(SDL2::SDL2_ttf   libSDL2_ttf.so)
if (NOT TARGET SDL2::SDL2main)
    add_library(SDL2::SDL2main INTERFACE IMPORTED) # SDL2main is a no-op outside Windows
endif()

# The version, read from the headers this toolchain actually compiles against rather than left unset.
# A third-party project may ask for a minimum - Chocolate Doom wants SDL2 2.0.14 - and with no version
# reported that check passes vacuously everywhere, including on the console, whose SDL2 is 2.0.14. It
# is better to fail at configure time with a version mismatch than at run time with a missing symbol.
foreach(_ab_sdl2_dir "${CMAKE_SYSROOT}/usr/include/SDL2" "${CMAKE_SYSROOT}/usr/include")
    if (EXISTS "${_ab_sdl2_dir}/SDL_version.h")
        file(STRINGS "${_ab_sdl2_dir}/SDL_version.h" _ab_sdl2_major REGEX "^#define SDL_MAJOR_VERSION ")
        file(STRINGS "${_ab_sdl2_dir}/SDL_version.h" _ab_sdl2_minor REGEX "^#define SDL_MINOR_VERSION ")
        file(STRINGS "${_ab_sdl2_dir}/SDL_version.h" _ab_sdl2_patch REGEX "^#define SDL_PATCHLEVEL ")
        string(REGEX MATCH "[0-9]+" _ab_sdl2_major "${_ab_sdl2_major}")
        string(REGEX MATCH "[0-9]+" _ab_sdl2_minor "${_ab_sdl2_minor}")
        string(REGEX MATCH "[0-9]+" _ab_sdl2_patch "${_ab_sdl2_patch}")
        set(SDL2_VERSION "${_ab_sdl2_major}.${_ab_sdl2_minor}.${_ab_sdl2_patch}")
        set(SDL2_VERSION_STRING "${SDL2_VERSION}")
        break()
    endif()
endforeach()
