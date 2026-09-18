#!/bin/bash
# clang-format over every source we own (not third_party, not the vendored SDL_FontCache / unecm), with the
# repo's .clang-format.
#
#   tools/format.sh          rewrite the files in place
#   tools/format.sh --check  report the files that are not formatted and exit 1 (what make_win.sh runs)
#
# clang-format 20 or newer: MSYS2 `pacman -S mingw-w64-ucrt-x86_64-clang-tools-extra`, apt `clang-format`.
set -e
cd "$(dirname "$0")/.."

CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"
if ! command -v "$CLANG_FORMAT" >/dev/null 2>&1; then
    if [ -x /c/msys64/ucrt64/bin/clang-format ]; then CLANG_FORMAT=/c/msys64/ucrt64/bin/clang-format
    elif [ -x /ucrt64/bin/clang-format ]; then CLANG_FORMAT=/ucrt64/bin/clang-format
    else echo "clang-format not found (pacman -S mingw-w64-ucrt-x86_64-clang-tools-extra)"; exit 1; fi
fi

# the sources we own: the app, the library, its example, the tests and the tools
sources() {
    find src/code lib_ableem/include lib_ableem/src lib_ableem/examples tests tools \
        \( -path '*/third_party' -o -path '*/__pycache__' \) -prune -o \
        -type f \( -name '*.cpp' -o -name '*.h' -o -name '*.c' \) \
        ! -name 'SDL_FontCache.*' ! -name 'unecm.c' -print
}

if [ "$1" = "--check" ]; then
    bad=$(sources | xargs "$CLANG_FORMAT" --dry-run --Werror 2>&1 | grep -E ": error:" | sed -E 's/:[0-9]+:[0-9]+: error:.*//' | sort -u)
    if [ -n "$bad" ]; then
        echo "$bad" >&2
        echo "clang-format: the files above are not formatted - run tools/format.sh" >&2
        exit 1
    fi
    echo "clang-format: ok ($(sources | wc -l) files)"
elif [ "$1" = "--list" ]; then
    sources
else
    sources | xargs "$CLANG_FORMAT" -i
    echo "clang-format: $(sources | wc -l) files formatted"
fi
