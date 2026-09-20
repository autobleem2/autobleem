# Cross-compile AutoBleem for the PlayStation Classic with Sony's armv8-sony-linux-gnueabihf toolchain
# (crosstool-NG, GCC 8.2.0) - the real target. The same shape as pcsx-ab's toolchains/psc/PSCtoolchainV8.cmake,
# so the two projects build side by side on the same server.
#
# The toolchain root is AB_PSC_TOOLCHAIN (default /opt/toolchain, the crosstool-NG layout on the build server:
# <root>/bin/armv8-sony-linux-gnueabihf-gcc, <root>/armv8-sony-linux-gnueabihf/sysroot). The sysroot has the
# SDL2/SDL2_image/SDL2_mixer/SDL2_ttf dev files the console build needs (2.0.4, predating sdl2-config.cmake -
# hence cmake/FindSDL2.cmake next to this file). ./make_psc.sh runs this on the server over ssh; the toolchain
# is not installed on the Windows host.
#
#   cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=toolchains/psc/PSCtoolchainV8.cmake -B build_psc

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)

set(AB_PSC_TOOLCHAIN "/opt/toolchain" CACHE PATH "Sony PSC toolchain root")
# CMake re-reads this file inside its try_compile sandboxes, where -D variables are invisible unless listed
# here - without this the compiler probe silently used the default root
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES AB_PSC_TOOLCHAIN)
if (EXISTS "${AB_PSC_TOOLCHAIN}/armv8-sony-linux-gnueabihf/sysroot")
    set(_ab_psc_sysroot "${AB_PSC_TOOLCHAIN}/armv8-sony-linux-gnueabihf/sysroot")
else()
    set(_ab_psc_sysroot "${AB_PSC_TOOLCHAIN}/sysroot")
endif()

if (EXISTS "${AB_PSC_TOOLCHAIN}/bin")
    set(CMAKE_C_COMPILER   "${AB_PSC_TOOLCHAIN}/bin/armv8-sony-linux-gnueabihf-gcc")
    set(CMAKE_CXX_COMPILER "${AB_PSC_TOOLCHAIN}/bin/armv8-sony-linux-gnueabihf-g++")
    set(CMAKE_SYSROOT "${_ab_psc_sysroot}")
    set(CMAKE_FIND_ROOT_PATH "${_ab_psc_sysroot}")
else()
    # toolchain on PATH (the way the original make_arm.sh assumed it)
    set(CMAKE_C_COMPILER   armv8-sony-linux-gnueabihf-gcc)
    set(CMAKE_CXX_COMPILER armv8-sony-linux-gnueabihf-g++)
endif()

# search for programs in the build host directories, for libraries and headers in the target directories
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# The root CMakeLists.txt's "^arm" branch sets the console's CPU flags (-march=armv8-a+simd, hard float, -Os,
# -s) on CMAKE_C_FLAGS/CMAKE_CXX_FLAGS itself and would overwrite anything put there here. The original
# toolchain file also asked for --static, which that overwrite silently dropped - the console build has
# always been dynamic against the sysroot's SDL2 .so's, unpacked to /tmp/lib at boot from
# Autobleem/lib/libs.tar.gz - so this file no longer pretends otherwise.

# our FindSDL2: the sysroot's SDL2 2.0.4 predates sdl2-config.cmake
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

# The unit tests run on the build host, never on the console - there is no reason to cross-compile them.
set(AB_BUILD_TESTS OFF CACHE BOOL "" FORCE)
# the console (root CMakeLists.txt's AB_TARGET): the /media tree, the internal games, no online update
set(AB_TARGET psc CACHE STRING "" FORCE)
# FindSDL2.cmake links the sysroot's .so files by absolute path, which is exactly how a build-server path
# ends up as an RPATH/RUNPATH in the binary; the console loads SDL from /tmp/lib (rc/autobleem.sh) and
# /usr/lib, never from there. make_psc.sh checks the result with readelf.
set(CMAKE_SKIP_RPATH TRUE CACHE BOOL "" FORCE)
set(CMAKE_SKIP_BUILD_RPATH TRUE CACHE BOOL "" FORCE)
