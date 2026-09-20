# Cross-compile AutoBleem for the 32-bit PC USB stick (payload_linux/, Debian 12 Bookworm i386) with Debian's
# own multiarch cross compiler - the Docker image's pcusb stage (docker/Dockerfile): crossbuild-essential-i386
# is i686-linux-gnu-gcc, the :i386 dev packages put SDL2's headers, .so links and sdl2-config.cmake under
# /usr/lib/i386-linux-gnu. There is no Windows-hosted toolchain for this target (unlike the Pis' SysGCC) -
# it builds on the server: docker/run.sh ci/build.sh pcusb.
#
#   cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=toolchains/pcusb/PcUsbToolchain.cmake -B build_pcusb
#
# The CPU is a plain i686 (no SSE2) - Debian's own i386 baseline, what its libsdl2:i386 is built for - so a
# Pentium M / Athlon XP class machine boots the stick; the launcher would gain nothing measurable from SSE2.
# _FILE_OFFSET_BITS=64 keeps a >2 GB disc image readable through a 32-bit off_t.

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR i686)

set(_ab_pcusb_triplet i686-linux-gnu)
find_program(_ab_pcusb_gcc "${_ab_pcusb_triplet}-gcc")
if (NOT _ab_pcusb_gcc)
    message(FATAL_ERROR "no i386 cross compiler: ${_ab_pcusb_triplet}-gcc is not on PATH "
                        "(Debian: crossbuild-essential-i386 + libsdl2*-dev:i386 - the Docker image's pcusb stage)")
endif()
set(CMAKE_C_COMPILER   "${_ab_pcusb_triplet}-gcc")
set(CMAKE_CXX_COMPILER "${_ab_pcusb_triplet}-g++")
# the compiler's own default paths are /usr/include/<triplet>, /usr/lib/<triplet>; CMAKE_LIBRARY_ARCHITECTURE
# makes find_package look in /usr/lib/<triplet>/cmake too, and SDL2_DIR pins the config package to that
# architecture's copy (the host's own would otherwise be a candidate)
set(CMAKE_LIBRARY_ARCHITECTURE "i386-linux-gnu")
set(SDL2_DIR "/usr/lib/i386-linux-gnu/cmake/SDL2" CACHE PATH "" FORCE)

# AB_PCUSB_DEBUG: symbols kept and little optimisation, for a backtrace from a core dump; the shipped binary
# is small and stripped (and UPX-packed by ci/build.sh).
option(AB_PCUSB_DEBUG "PC stick build with debug symbols (not stripped, -O1 -g)" OFF)
if (AB_PCUSB_DEBUG)
    set(_ab_pcusb_opt "-O1 -g")
else()
    set(_ab_pcusb_opt "-Os -s")
endif()
set(_ab_pcusb_flags "-march=i686 -mtune=generic -D_FILE_OFFSET_BITS=64 ${_ab_pcusb_opt}")
# set outright, the way the Pi toolchain files do: the root CMakeLists' appliance branch leaves them alone
set(CMAKE_C_FLAGS   "${_ab_pcusb_flags}")
set(CMAKE_CXX_FLAGS "${_ab_pcusb_flags}")

# i386 code runs on the amd64 build host, so the unit tests are built and run here too - one more gate the
# ARM targets cannot have (their tests run in the native build only).
set(AB_BUILD_TESTS ON CACHE BOOL "" FORCE)

# The PC stick appliance: rpi's semantics on x86 (root CMakeLists.txt's AB_TARGET).
set(AB_TARGET pcusb CACHE STRING "" FORCE)
