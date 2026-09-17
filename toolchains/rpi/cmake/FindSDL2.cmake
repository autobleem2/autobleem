# Minimal SDL2 discovery for the Raspberry Pi cross toolchain (toolchains/rpi/RPitoolchain.cmake).
#
# The sysGCC Raspberry Pi toolchain's sysroot was rsynced from a real Pi's installed *packages*, which
# gives us the SDL2/SDL2_image/SDL2_mixer/SDL2_ttf runtime .so files but no libsdl2-dev headers, no
# unversioned .so symlinks and no sdl2-config.cmake. lib_ableem/CMakeLists.txt's non-MinGW branch does one
# find_package(SDL2 REQUIRED) and then links the bare names SDL2, SDL2_image, SDL2_mixer, SDL2_ttf, so this
# single module defines all four as IMPORTED targets: headers from toolchains/rpi/sdl2-devkit (borrowed from
# the MSYS2 SDL2 package - the public headers are pure C, arch-independent, and close enough in version to
# the target .so's SONAME to be ABI-safe), libraries pointed straight at the sysroot's versioned .so files
# (no unversioned symlink needed since IMPORTED_LOCATION takes a full path).

get_filename_component(_ab_rpi_devkit_include "${CMAKE_CURRENT_LIST_DIR}/../sdl2-devkit/include" ABSOLUTE)
set(_ab_rpi_sdl2_libdir "${CMAKE_SYSROOT}/usr/lib/arm-linux-gnueabihf")

function(_ab_rpi_add_sdl2_target name soname)
    if (NOT TARGET ${name})
        add_library(${name} UNKNOWN IMPORTED)
    endif()
    set_target_properties(${name} PROPERTIES
        IMPORTED_LOCATION "${_ab_rpi_sdl2_libdir}/${soname}"
        INTERFACE_INCLUDE_DIRECTORIES "${_ab_rpi_devkit_include}"
    )
endfunction()

_ab_rpi_add_sdl2_target(SDL2       libSDL2-2.0.so.0)
_ab_rpi_add_sdl2_target(SDL2_image libSDL2_image-2.0.so.0)
_ab_rpi_add_sdl2_target(SDL2_mixer libSDL2_mixer-2.0.so.0)
_ab_rpi_add_sdl2_target(SDL2_ttf   libSDL2_ttf-2.0.so.0)

set(SDL2_FOUND TRUE)
set(SDL2_INCLUDE_DIRS "${_ab_rpi_devkit_include}")
set(SDL2_LIBRARIES SDL2 SDL2_image SDL2_mixer SDL2_ttf)
