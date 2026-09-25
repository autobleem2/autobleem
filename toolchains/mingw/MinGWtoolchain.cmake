# Cross-compile AutoBleem for Windows from a Linux host with mingw-w64 - the Docker image (docker/Dockerfile's
# mingw stage) and the CI. The native Windows build on the PC is make_win.sh in MSYS2 and needs no toolchain
# file; this one exists so the same tree builds the same exe on a Linux CI runner.
#
# The compilers are Debian's x86_64-w64-mingw32-*-posix (the posix threads variant: the scan runs on a
# std::thread). SDL2 and SDL2_image/mixer/ttf are the official "devel-mingw" packages installed under
# AB_MINGW_SDL2 (/opt/mingw-sdl2 in the image: include/, lib/, lib/pkgconfig/, bin/*.dll); lib_ableem's
# MinGW branch finds them through pkg-config, which is pointed at that pkgconfig directory here.
#
#   cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=toolchains/mingw/MinGWtoolchain.cmake -B build_mingw

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR AMD64)

set(AB_MINGW_SDL2 "$ENV{AB_MINGW_SDL2}" CACHE PATH "The SDL2 mingw development packages' install prefix")
if (NOT AB_MINGW_SDL2)
    set(AB_MINGW_SDL2 "/opt/mingw-sdl2")
endif()
list(APPEND CMAKE_TRY_COMPILE_PLATFORM_VARIABLES AB_MINGW_SDL2)

set(_ab_mingw_triplet x86_64-w64-mingw32)
set(CMAKE_C_COMPILER   ${_ab_mingw_triplet}-gcc-posix)
set(CMAKE_CXX_COMPILER ${_ab_mingw_triplet}-g++-posix)
set(CMAKE_RC_COMPILER  ${_ab_mingw_triplet}-windres)

# headers and libraries from the mingw sysroot and the SDL prefix only, programs from the host
set(CMAKE_FIND_ROOT_PATH "/usr/${_ab_mingw_triplet}" "${AB_MINGW_SDL2}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# the GCC runtime inside the exe; winpthread stays a DLL (libstdc++'s thread support wants it shared) and
# ships next to the exe with the SDL DLLs (tools/make_win_package.sh). UpdateRoms links -static on its own.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++")
# and inside every extension (ab_add_extension builds a MODULE): the product ships no libstdc++-6.dll or
# libgcc_s_seh-1.dll, so a plugin that wanted them was refused - error 126 on a clean install, 127 where an
# older MSYS2-built install had left its own copies next to a newer winpthread (the Store, 2026-09-25)
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "-static-libgcc -static-libstdc++")

# the host's pkg-config, reading only the SDL packages' .pc files (their prefix is rewritten at install)
set(ENV{PKG_CONFIG_LIBDIR} "${AB_MINGW_SDL2}/lib/pkgconfig")
set(ENV{PKG_CONFIG_SYSROOT_DIR} "")

# the test executables are built (the compile is the check); ctest runs them only through wine when it is
# installed, otherwise the native Linux build's ctest run is where the suites execute
find_program(_ab_wine NAMES wine64 wine)
if (_ab_wine)
    set(CMAKE_CROSSCOMPILING_EMULATOR "${_ab_wine}")
endif()
