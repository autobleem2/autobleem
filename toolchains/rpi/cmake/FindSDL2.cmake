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
        # both spellings: our own code says <SDL2/SDL.h>, while a third-party project built
        # against upstream's config package says "SDL.h" and expects the subdirectory itself
        INTERFACE_INCLUDE_DIRECTORIES "${_ab_rpi_devkit_include};${_ab_rpi_devkit_include}/SDL2"
    )
endfunction()

_ab_rpi_add_sdl2_target(SDL2       libSDL2-2.0.so.0)
_ab_rpi_add_sdl2_target(SDL2_image libSDL2_image-2.0.so.0)
_ab_rpi_add_sdl2_target(SDL2_mixer libSDL2_mixer-2.0.so.0)
_ab_rpi_add_sdl2_target(SDL2_ttf   libSDL2_ttf-2.0.so.0)

set(SDL2_FOUND TRUE)
set(SDL2_INCLUDE_DIRS "${_ab_rpi_devkit_include}")
set(SDL2_LIBRARIES SDL2 SDL2_image SDL2_mixer SDL2_ttf)

# The same libraries under the names a third-party project expects. Upstream SDL2's own config package
# exports SDL2::SDL2 and friends, so any game whose CMakeLists says target_link_libraries(game
# SDL2::SDL2) builds for this target unmodified - which is how the Apps are built (OpenJazz, Chocolate
# Doom). Our own targets link the bare names and are left alone.
_ab_rpi_add_sdl2_target(SDL2::SDL2       libSDL2-2.0.so.0)
_ab_rpi_add_sdl2_target(SDL2::SDL2_image libSDL2_image-2.0.so.0)
_ab_rpi_add_sdl2_target(SDL2::SDL2_mixer libSDL2_mixer-2.0.so.0)
_ab_rpi_add_sdl2_target(SDL2::SDL2_ttf   libSDL2_ttf-2.0.so.0)
if (NOT TARGET SDL2::SDL2main)
    add_library(SDL2::SDL2main INTERFACE IMPORTED) # SDL2main is a no-op outside Windows
endif()

# The version, read from the headers this toolchain actually compiles against rather than left unset.
# A third-party project may ask for a minimum - Chocolate Doom wants SDL2 2.0.14 - and with no version
# reported that check passes vacuously everywhere, including on the console, whose SDL2 is 2.0.12. It
# is better to fail at configure time with a version mismatch than at run time with a missing symbol.
foreach(_ab_sdl2_dir "${_ab_rpi_devkit_include}/SDL2" "${_ab_rpi_devkit_include}")
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
