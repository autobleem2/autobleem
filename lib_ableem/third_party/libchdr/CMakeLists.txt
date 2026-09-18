# Vendored from libchdr (https://github.com/rtissera/libchdr, BSD 3-Clause - see LICENSE.txt), commit
# 8bba7745d758627258b315997a860039244cedaf (2025-06-08), the same snapshot AutoBleem-NG bundles: reads
# MAME's CHDv1-v5 disc image format. Trimmed to a static build - the libchdr sources (FLAC decoding is
# the header-only dr_flac under include/dr_libs) plus the three codec dependencies they decompress
# against, each built from its own vendored copy under deps/: LZMA SDK 24.05, zlib 1.3.1 and
# zstd 1.5.6. zstd is what the previous libmamecd lacked - chdman has compressed with it by default for
# a while, so a fresh CHD would not open. Every dep tree is cut down to what its CMake build needs.
# This is vendored code, not ours to keep warning-free (see lib_ableem/CMakeLists.txt's sqlite/miniz
# comment for the same rule).
#
# Used by lib_ableem/src/engine/cd_image_reader.h (ChdImageReader) when ABLEEM_ENABLE_CHD is on.

set(WITH_LZMA_ASM OFF CACHE BOOL "" FORCE)   # plain C on every host, the same code path everywhere
add_subdirectory(deps/lzma-24.05 EXCLUDE_FROM_ALL)

option(ZLIB_BUILD_EXAMPLES "Enable Zlib Examples" OFF)
add_subdirectory(deps/zlib-1.3.1 EXCLUDE_FROM_ALL)
set_target_properties(zlibstatic PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ZSTD_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
set(ZSTD_LEGACY_SUPPORT OFF CACHE BOOL "" FORCE)
add_subdirectory(deps/zstd-1.5.6/build/cmake EXCLUDE_FROM_ALL)

set(CHDR_SOURCES
        src/libchdr_bitstream.c
        src/libchdr_cdrom.c
        src/libchdr_chd.c
        src/libchdr_flac.c
        src/libchdr_huffman.c
        )

add_library(chdr STATIC ${CHDR_SOURCES})
target_include_directories(chdr PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
# zlib's generated zconf.h lands in its binary dir; the lzma dep exports its own include dir
target_include_directories(chdr PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/zlib-1.3.1
        ${CMAKE_CURRENT_BINARY_DIR}/deps/zlib-1.3.1
        ${CMAKE_CURRENT_SOURCE_DIR}/deps/zstd-1.5.6/lib)
target_link_libraries(chdr PRIVATE lzma zlibstatic libzstd_static)

set_source_files_properties(${CHDR_SOURCES} PROPERTIES COMPILE_FLAGS "-w")
