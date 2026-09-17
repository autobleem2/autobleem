set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)


set(CMAKE_C_COMPILER armv8-sony-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER armv8-sony-linux-gnueabihf-g++)

# search for programs in the build host directories
SET(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
SET(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
SET(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

SET(CMAKE_C_FLAGS "-mfloat-abi=hard -march=armv8-a+simd --static -Os -s")
SET(CMAKE_CXX_FLAGS " -mfloat-abi=hard -march=armv8-a+simd --static -Os -s")

# The unit tests run on the build host, never on the console - there is no reason to cross-compile them.
set(AB_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# ScanService (core/services/scan_service.*) added the app's first std::thread. ableem_engine already links
# Threads::Threads (lib_ableem/CMakeLists.txt), which is enough on the Windows/MinGW and Mac/Linux dev
# builds, but --static here means every symbol std::thread needs (pthread_create and friends) has to be
# pulled out of a static libpthread.a, and weak/stub pthread symbols in libc can silently win over the real
# ones from a plain -lpthread on the link line. Not verified on real hardware yet (no ARM toolchain on this
# host - see CLAUDE.md's "next thing to do"): if autobleem-gui links but the scan thread never starts, or
# link fails with undefined pthread_* references, try forcing every pthread symbol in with
# "-Wl,--whole-archive -lpthread -Wl,--no-whole-archive" added to CMAKE_EXE_LINKER_FLAGS here.
