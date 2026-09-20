# The 7z container reader from the LZMA SDK (23.01, public domain, Igor Pavlov - lzma-sdk.txt): the
# archive parser, the folder decoder (LZMA, LZMA2, PPMd, BCJ/BCJ2 and the other branch filters, Delta)
# and the file/stream glue, as 7zDec (C/Util/7z) builds them - extract only. The codecs themselves
# (LzmaDec, Bra*, Delta, CpuArch, Alloc) are libchdr's vendored lzma-24.05, which this compiles against
# and links: one copy of every symbol. For ableem::SevenZipArchive (RetroArch's Windows builds come as .7z).
add_library(lzma7z STATIC
        src/7zAlloc.c
        src/7zArcIn.c
        src/7zBuf.c
        src/7zBuf2.c
        src/7zCrc.c
        src/7zCrcOpt.c
        src/7zDec.c
        src/7zFile.c
        src/7zStream.c
        src/Bcj2.c
        src/Lzma2Dec.c
        src/Ppmd7.c
        src/Ppmd7Dec.c
)
target_compile_definitions(lzma7z PRIVATE Z7_PPMD_SUPPORT Z7_EXTRACT_ONLY Z7_ST)
target_include_directories(lzma7z PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_link_libraries(lzma7z PUBLIC lzma)
set_target_properties(lzma7z PROPERTIES POSITION_INDEPENDENT_CODE ON)
# vendored: not ours to keep warning-free
target_compile_options(lzma7z PRIVATE -w)
