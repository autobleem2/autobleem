# libchdr (vendored)

https://github.com/rtissera/libchdr at commit `8bba7745d758627258b315997a860039244cedaf` (2025-06-08),
BSD 3-Clause (LICENSE.txt). Reads MAME's CHDv1-v5 disc images. Replaced the older libmamecd fork on
2026-09-18 because chdman's default `zstd` codec was missing there.

`deps/` holds the codec libraries it decompresses with, each trimmed to what its own CMake build needs:
LZMA SDK 24.05 (its licence in its folder), zlib 1.3.1, zstd 1.5.6 (`lib/` + `build/cmake/`, no legacy
formats, programs, tests or contrib). FLAC is the header-only `include/dr_libs/dr_flac.h`.

See CMakeLists.txt here for how it is built; `lib_ableem/src/engine/cd_image_reader.h` is the only user.
