# Minimal SDL2 discovery for the 64-bit Raspberry Pi cross toolchain (toolchains/rpi64/RPi64toolchain.cmake).
# Same reasoning as the 32-bit toolchains/rpi/cmake/FindSDL2.cmake (that file's comment has the full story):
# the sysGCC sysroot has the SDL2/image/mixer/ttf runtime .so's but no -dev headers, symlinks or cmake config.
# The public SDL2 headers are pure C and arch-independent, so this reuses the 32-bit port's borrowed copy
# instead of keeping a second one in sync; only the library directory (the sysroot's aarch64 multiarch path)
# differs from the 32-bit module.

get_filename_component(_ab_rpi64_devkit_include "${CMAKE_CURRENT_LIST_DIR}/../../rpi/sdl2-devkit/include" ABSOLUTE)
set(_ab_rpi64_sdl2_libdir "${CMAKE_SYSROOT}/usr/lib/aarch64-linux-gnu")

function(_ab_rpi64_add_sdl2_target name soname)
    if (NOT TARGET ${name})
        add_library(${name} UNKNOWN IMPORTED)
    endif()
    set_target_properties(${name} PROPERTIES
        IMPORTED_LOCATION "${_ab_rpi64_sdl2_libdir}/${soname}"
        INTERFACE_INCLUDE_DIRECTORIES "${_ab_rpi64_devkit_include}"
    )
endfunction()

_ab_rpi64_add_sdl2_target(SDL2       libSDL2-2.0.so.0)
_ab_rpi64_add_sdl2_target(SDL2_image libSDL2_image-2.0.so.0)
_ab_rpi64_add_sdl2_target(SDL2_mixer libSDL2_mixer-2.0.so.0)
_ab_rpi64_add_sdl2_target(SDL2_ttf   libSDL2_ttf-2.0.so.0)

set(SDL2_FOUND TRUE)
set(SDL2_INCLUDE_DIRS "${_ab_rpi64_devkit_include}")
set(SDL2_LIBRARIES SDL2 SDL2_image SDL2_mixer SDL2_ttf)
