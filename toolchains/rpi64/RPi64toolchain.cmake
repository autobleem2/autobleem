# Cross-compile AutoBleem for a 64-bit Raspberry Pi OS (Trixie) userland, using the Windows-hosted "SysGCC
# for Raspberry Pi (64-bit)" toolchain (a sysroot rsynced from a real 64-bit Pi, same vendor and pattern as
# the 32-bit toolchains/rpi/RPitoolchain.cmake - see CLAUDE.md's "Raspberry Pi port" section for both).
#
# The sysroot has SDL2/SDL2_image/SDL2_mixer/SDL2_ttf runtime .so's but no libsdl2-dev headers, unversioned
# .so symlinks or cmake config - see toolchains/rpi64/cmake/FindSDL2.cmake, which reuses the 32-bit port's
# borrowed headers (arch-independent) and points straight at the sysroot's versioned .so's, same trick.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(_ab_rpi64_root "C:/sysGCC/raspberry64")
set(_ab_rpi64_sysroot "${_ab_rpi64_root}/aarch64-linux-gnu/sysroot")

set(CMAKE_C_COMPILER   "${_ab_rpi64_root}/bin/aarch64-linux-gnu-gcc.exe")
set(CMAKE_CXX_COMPILER "${_ab_rpi64_root}/bin/aarch64-linux-gnu-g++.exe")

set(CMAKE_SYSROOT "${_ab_rpi64_sysroot}")
set(CMAKE_FIND_ROOT_PATH "${_ab_rpi64_sysroot}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Raspberry Pi 3/4/5/400/Zero 2 running the 64-bit OS - all of them are armv8-a (the 64-bit image has no
# armv6/armv7 boards to support, unlike the 32-bit one). AB_RPI_DEBUG mirrors the 32-bit toolchain: symbols
# and no optimisation for gdb on the Pi from a core dump, otherwise a small stripped binary.
option(AB_RPI_DEBUG "Raspberry Pi build with debug symbols (not stripped, -O1 -g)" OFF)
if(AB_RPI_DEBUG)
    set(_ab_rpi64_opt "-O1 -g")
else()
    set(_ab_rpi64_opt "-Os -s")
endif()
set(CMAKE_C_FLAGS   "-march=armv8-a ${_ab_rpi64_opt}")
set(CMAKE_CXX_FLAGS "-march=armv8-a ${_ab_rpi64_opt}")

# Our own FindSDL2.cmake - see the file for why it exists on this toolchain specifically.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# The unit tests run on the build host, never cross-compiled - same reasoning as toolchains/rpi/RPitoolchain.cmake.
set(AB_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# Same Pi target as the 32-bit build (AB_PLATFORM_RPI does not branch on word size): no internal games, paths
# under the exFAT data partition given on the command line.
set(AB_TARGET_RPI ON CACHE BOOL "" FORCE)
