# lzma-7z

The 7z archive reader out of the LZMA SDK 23.01 (https://www.7-zip.org/sdk.html, public domain - see
`lzma-sdk.txt`): the files `C/Util/7z/makefile.gcc` lists for `7zdec`, minus the codecs that libchdr's
vendored `lzma-24.05` already builds (`LzmaDec`, `Bra*`, `Delta`, `CpuArch`, `Alloc`) - these compile
against that copy's headers and link its target, so no symbol exists twice. Extract only
(`Z7_EXTRACT_ONLY`), single-threaded (`Z7_ST`). `ableem::SevenZipArchive` is the only user.
