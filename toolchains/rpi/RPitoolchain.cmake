# Cross-compile AutoBleem for a 32-bit Raspberry Pi OS (Raspbian) userland. Two hosts can do it - see
# common.cmake: the Windows PC with the "SysGCC for Raspberry Pi" toolchain (AB_RPI_TOOLCHAIN, a sysroot
# rsynced from a real Pi; cmake/FindSDL2.cmake papers over its missing SDL headers with borrowed ones), or a
# Debian host with crossbuild-essential-armhf and the multiarch libsdl2*-dev:armhf packages (the Docker
# image). Separate from the PSC's own toolchain (toolchains/psc/PSCtoolchainV8.cmake) - see CLAUDE.md.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(AB_RPI_TOOLCHAIN "C:/sysGCC/raspberry" CACHE PATH "SysGCC for Raspberry Pi install directory (Windows)")
# re-read inside try_compile, where -D variables are invisible unless listed here
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES AB_RPI_TOOLCHAIN)

include("${CMAKE_CURRENT_LIST_DIR}/common.cmake")
ab_rpi_toolchain(arm-linux-gnueabihf "${AB_RPI_TOOLCHAIN}" "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Raspberry Pi 2/3/4 running the 32-bit OS - a reasonable generic target for a first port attempt.
# (Pi Zero/1 are armv6 and would need a different -march; not a goal yet.)
# _FILE_OFFSET_BITS=64: a 32-bit off_t makes stat() fail on a file over 2 GB (a Store download, a disc image)
set(CMAKE_C_FLAGS   "-mfloat-abi=hard -mfpu=neon-vfpv4 -march=armv7-a -D_FILE_OFFSET_BITS=64 ${_ab_rpi_opt}")
set(CMAKE_CXX_FLAGS "-mfloat-abi=hard -mfpu=neon-vfpv4 -march=armv7-a -D_FILE_OFFSET_BITS=64 ${_ab_rpi_opt}")
