# Cross-compile AutoBleem for a 32-bit Raspberry Pi OS (Raspbian) userland, using the Windows-hosted
# "SysGCC for Raspberry Pi" toolchain (sysroot rsynced from a real Pi). This is an exploratory RPi port
# target, separate from the PSC's own toolchain (toolchains/psc/PSCtoolchainV8.cmake) - see CLAUDE.md.
#
# The sysroot has SDL2/SDL2_image/SDL2_mixer/SDL2_ttf runtime .so's (whatever was installed as apt packages
# on the source Pi) but no libsdl2-dev headers, unversioned .so symlinks or cmake config - see
# toolchains/rpi/cmake/FindSDL2.cmake, which papers over that with borrowed headers and direct .so paths.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(_ab_rpi_root "C:/sysGCC/raspberry")
set(_ab_rpi_sysroot "${_ab_rpi_root}/arm-linux-gnueabihf/sysroot")

set(CMAKE_C_COMPILER   "${_ab_rpi_root}/bin/arm-linux-gnueabihf-gcc.exe")
set(CMAKE_CXX_COMPILER "${_ab_rpi_root}/bin/arm-linux-gnueabihf-g++.exe")

set(CMAKE_SYSROOT "${_ab_rpi_sysroot}")
set(CMAKE_FIND_ROOT_PATH "${_ab_rpi_sysroot}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

# Raspberry Pi 2/3/4 running the 32-bit OS - a reasonable generic target for a first port attempt.
# (Pi Zero/1 are armv6 and would need a different -march; not a goal yet.)
# AB_RPI_DEBUG (make_rpi.sh --debug): symbols kept and little optimisation, for a backtrace under the Pi's
# gdb from a core dump; the shipped binary is small and stripped.
option(AB_RPI_DEBUG "Raspberry Pi build with debug symbols (not stripped, -O1 -g)" OFF)
if(AB_RPI_DEBUG)
    set(_ab_rpi_opt "-O1 -g")
else()
    set(_ab_rpi_opt "-Os -s")
endif()
set(CMAKE_C_FLAGS   "-mfloat-abi=hard -mfpu=neon-vfpv4 -march=armv7-a ${_ab_rpi_opt}")
set(CMAKE_CXX_FLAGS "-mfloat-abi=hard -mfpu=neon-vfpv4 -march=armv7-a ${_ab_rpi_opt}")

# Our own FindSDL2.cmake - see the file for why it exists on this toolchain specifically.
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# The unit tests run on the build host, never cross-compiled - same reasoning as toolchains/psc/PSCtoolchainV8.cmake.
set(AB_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# The Pi port: no internal games, paths under the exFAT data partition given on the command line.
set(AB_TARGET_RPI ON CACHE BOOL "" FORCE)
