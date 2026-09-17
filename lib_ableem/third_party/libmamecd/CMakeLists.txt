# Vendored from libmamecd (https://github.com/DerekJohn0/libmamecd, BSD 3-Clause - see LICENSE.txt),
# an updated fork of libchdr: reads MAME's CHDv1-v5 disc image format. Trimmed to a static build - the
# libchdr sources plus the three codec dependencies they decompress against (FLAC, lzma, zlib), each
# built from its own vendored copy under deps/. This is vendored code, not ours to keep warning-free
# (see lib_ableem/CMakeLists.txt's sqlite/miniz comment for the same rule).
#
# Used by lib_ableem/src/engine/cd_image_reader.h (ChdImageReader) when ABLEEM_ENABLE_CHD is on.

if (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
    set(CPU_ARCH "x64")
elseif (CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86|i386|i686)$")
    set(CPU_ARCH "x86")
else()
    set(CPU_ARCH "other")
endif()

add_subdirectory(deps/flac-1.3.3 EXCLUDE_FROM_ALL)
add_subdirectory(deps/lzma-19.00 EXCLUDE_FROM_ALL)
add_subdirectory(deps/zlib-1.2.11 EXCLUDE_FROM_ALL)

add_library(mamecd STATIC
        src/libchdr_bitstream.c
        src/libchdr_cdecc.c
        src/libchdr_cdrom.c
        src/libchdr_chd.c
        src/libchdr_flac.c
        src/libchdr_huffman.c
        )

target_include_directories(mamecd PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(mamecd PUBLIC FLAC lzma zlib)

set_source_files_properties(
        src/libchdr_bitstream.c
        src/libchdr_cdecc.c
        src/libchdr_cdrom.c
        src/libchdr_chd.c
        src/libchdr_flac.c
        src/libchdr_huffman.c
        PROPERTIES COMPILE_FLAGS "-w"
)
