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
