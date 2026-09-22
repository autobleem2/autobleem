# Shared by toolchains/rpi/RPitoolchain.cmake (32-bit) and toolchains/rpi64/RPi64toolchain.cmake (64-bit):
# where the cross compiler comes from, and the options every Pi build sets.
#
# Two kinds of host build for a Pi:
#   - the Windows PC, with a "SysGCC for Raspberry Pi" toolchain (a sysroot rsynced from a real Pi, no SDL
#     headers - the toolchain dir's cmake/FindSDL2.cmake borrows them from toolchains/rpi/sdl2-devkit)
#   - a Debian host (the Docker image, docker/Dockerfile's pi stage) with Debian's own crossbuild-essential-*
#     and the multiarch libsdl2*-dev:<arch> packages: real headers, .so links and sdl2-config.cmake under
#     /usr/lib/<triplet>, nothing borrowed. Raspberry Pi OS is Debian, so this is the closer sysroot.
# ab_rpi_toolchain() picks the first when its directory exists, the second otherwise.

# ab_rpi_toolchain(<triplet> <sysgcc root> <module dir>) - a macro, so the CMAKE_* it sets land in the
# toolchain file's own scope; <module dir> is the toolchain dir's cmake/ with the SysGCC FindSDL2.cmake.
macro(ab_rpi_toolchain _triplet _sysgcc_root _module_dir)
    if (EXISTS "${_sysgcc_root}/bin/${_triplet}-gcc.exe")
        set(CMAKE_C_COMPILER   "${_sysgcc_root}/bin/${_triplet}-gcc.exe")
        set(CMAKE_CXX_COMPILER "${_sysgcc_root}/bin/${_triplet}-g++.exe")
        set(CMAKE_SYSROOT "${_sysgcc_root}/${_triplet}/sysroot")
        set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")
        set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
        set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
        set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
        # and packages, or a find_package() in a third-party project we are cross-building finds the
        # *host's* library and puts its headers on the cross compile line - which is what happened
        # building OpenJazz, where MSYS2's libxmp landed in front of the sysroot. The console's
        # toolchain has always set this; these had not.
        set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
        # pkg-config, too. CMAKE_FIND_ROOT_PATH_MODE_PACKAGE does not govern it: pkg_check_modules() shells out
        # to whatever pkg-config is on PATH, which on an MSYS2 host answers about MSYS2's own libraries. That is
        # how a cross build of Chocolate Doom picked up the host SDL2 and, with it, -Dmain=SDL_main - so its
        # main() compiled as SDL_main and nothing linked. Pointing pkg-config at the sysroot (at a directory
        # that need not even exist) makes it answer "not found", which is the truth and is harmless.
        set(ENV{PKG_CONFIG_LIBDIR} "${CMAKE_SYSROOT}/usr/lib/pkgconfig:${CMAKE_SYSROOT}/usr/share/pkgconfig")
        set(ENV{PKG_CONFIG_SYSROOT_DIR} "${CMAKE_SYSROOT}")
        unset(ENV{PKG_CONFIG_PATH})
        list(APPEND CMAKE_MODULE_PATH "${_module_dir}")
        set(AB_RPI_TOOLCHAIN_KIND "sysgcc")
    else()
        # Debian multiarch: the compiler's own default paths are /usr/include/<triplet>, /usr/lib/<triplet>;
        # CMAKE_LIBRARY_ARCHITECTURE makes find_package look in /usr/lib/<triplet>/cmake too, and SDL2_DIR
        # pins the config package to that architecture's copy (the host's own would otherwise be a candidate)
        find_program(_ab_rpi_cross_gcc "${_triplet}-gcc")
        if (NOT _ab_rpi_cross_gcc)
            message(FATAL_ERROR "no Raspberry Pi cross compiler: neither ${_sysgcc_root} (SysGCC, Windows) nor "
                                "${_triplet}-gcc on PATH (Debian: crossbuild-essential + libsdl2*-dev:<arch>)")
        endif()
        set(CMAKE_C_COMPILER   "${_triplet}-gcc")
        set(CMAKE_CXX_COMPILER "${_triplet}-g++")
        set(CMAKE_LIBRARY_ARCHITECTURE "${_triplet}")
        set(SDL2_DIR "/usr/lib/${_triplet}/cmake/SDL2" CACHE PATH "" FORCE)
        set(AB_RPI_TOOLCHAIN_KIND "debian")
    endif()
endmacro()

# AB_RPI_DEBUG (make_rpi.sh --debug): symbols kept and little optimisation, for a backtrace under the Pi's
# gdb from a core dump; the shipped binary is small and stripped.
option(AB_RPI_DEBUG "Raspberry Pi build with debug symbols (not stripped, -O1 -g)" OFF)
if(AB_RPI_DEBUG)
    set(_ab_rpi_opt "-O1 -g")
else()
    set(_ab_rpi_opt "-Os -s")
endif()

# The unit tests run on the build host, never cross-compiled - same reasoning as toolchains/psc/PSCtoolchainV8.cmake.
set(AB_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# The Pi port: no internal games, paths under the exFAT data partition given on the command line.
set(AB_TARGET rpi CACHE STRING "" FORCE)
