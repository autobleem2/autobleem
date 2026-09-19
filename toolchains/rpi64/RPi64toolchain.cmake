# Cross-compile AutoBleem for a 64-bit Raspberry Pi OS (Trixie) userland. Two hosts can do it - see
# toolchains/rpi/common.cmake: the Windows PC with the "SysGCC for Raspberry Pi (64-bit)" toolchain
# (AB_RPI64_TOOLCHAIN; cmake/FindSDL2.cmake reuses the 32-bit port's borrowed headers), or a Debian host with
# crossbuild-essential-arm64 and the multiarch libsdl2*-dev:arm64 packages (the Docker image). See
# CLAUDE.md's "Raspberry Pi port" section for both architectures.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

set(AB_RPI64_TOOLCHAIN "E:/sysGCC/raspberry64" CACHE PATH "SysGCC for Raspberry Pi (64-bit) install directory (Windows)")
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES AB_RPI64_TOOLCHAIN)

include("${CMAKE_CURRENT_LIST_DIR}/../rpi/common.cmake")
ab_rpi_toolchain(aarch64-linux-gnu "${AB_RPI64_TOOLCHAIN}" "${CMAKE_CURRENT_LIST_DIR}/cmake")

# Raspberry Pi 3/4/5/400/Zero 2 running the 64-bit OS - all of them are armv8-a (the 64-bit image has no
# armv6/armv7 boards to support, unlike the 32-bit one).
set(CMAKE_C_FLAGS   "-march=armv8-a ${_ab_rpi_opt}")
set(CMAKE_CXX_FLAGS "-march=armv8-a ${_ab_rpi_opt}")
