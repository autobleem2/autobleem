#!/bin/bash
# clang-tidy (the repo's .clang-tidy) over every .cpp we own, with the compile commands of a configured
# build directory (make_win.sh's build_win by default - it configures with CMAKE_EXPORT_COMPILE_COMMANDS).
#
#   tools/lint.sh                 report; exit 1 when there is a finding
#   tools/lint.sh --fix           apply the checks' fix-its (review the diff, then tools/format.sh)
#   tools/lint.sh -p build_dir    another build directory
#   tools/lint.sh FILE.cpp ...    just these files
#
# Findings in headers are reported once per header (.clang-tidy's HeaderFilterRegex); third_party never is.
# --fix goes file by file rather than through run-clang-tidy -fix: on Windows a header included as
# "../x.h" and as "x.h" is two paths to clang-apply-replacements, which then applies the same edit twice
# ("override override"); one clang-tidy at a time re-parses the already fixed header instead.
# clang-tidy 20 or newer: MSYS2 `pacman -S mingw-w64-ucrt-x86_64-clang-tools-extra`, apt `clang-tidy`.
cd "$(dirname "$0")/.."

for dir in /c/msys64/ucrt64/bin /ucrt64/bin; do
    [ -x "$dir/clang-tidy" ] && PATH="$dir:$PATH"
done
if ! command -v clang-tidy >/dev/null 2>&1; then
    echo "clang-tidy not found (pacman -S mingw-w64-ucrt-x86_64-clang-tools-extra)"; exit 1
fi

build=build_win
fix=
files=()
while [ $# -gt 0 ]; do
    case "$1" in
        -p) build="$2"; shift 2 ;;
        --fix) fix=1; shift ;;
        *) files+=("$1"); shift ;;
    esac
done
if [ ! -f "$build/compile_commands.json" ]; then
    echo "$build/compile_commands.json not found - configure the build first (make_win.sh)"; exit 1
fi

# the sources we own (the vendored .c files are not C++ and never analysed)
sources() {
    if [ ${#files[@]} -gt 0 ]; then printf '%s\n' "${files[@]}"; return; fi
    find src/code lib_ableem/src lib_ableem/examples tests/core tests/support tests/doctest_main.cpp tools/theme_convert \
        -path '*/third_party' -prune -o -type f -name '*.cpp' -print
}
# one spelling per file: forward slashes, "dir/../" folded away
canonical() {
    sed -E 's#\\#/#g; :a; s#[^/]+/\.\./##; ta'
}

if [ -n "$fix" ]; then
    n=0
    while read -r f; do
        n=$((n + 1))
        echo "[$n] $f"
        clang-tidy -p "$build" --quiet --fix "$f" 2>&1 | grep -E ": (warning|error): " | canonical
    done < <(sources)
    echo "clang-tidy: fixes applied to $n files - review the diff, then tools/format.sh"
    exit 0
fi

jobs=$(nproc 2>/dev/null || echo 4)
out=$(mktemp)
sources | xargs run-clang-tidy -p "$build" -j "$jobs" -quiet > "$out" 2>&1
status=$?
# one line per finding, each once (a header's finding comes up in every translation unit that includes it)
findings=$(grep -E ": (warning|error): " "$out" | canonical | sort -u)
if [ -n "$findings" ]; then
    echo "$findings"
    echo
    echo "clang-tidy: $(echo "$findings" | wc -l) findings ($(echo "$findings" | grep -oE '\[[a-z]+-[a-z0-9-]+\]$' | sort | uniq -c | sort -rn | awk '{printf "%s%s %s", sep, $1, $2; sep=", "}'))"
    rm -f "$out"
    exit 1
fi
if [ $status -ne 0 ]; then
    tail -20 "$out"; rm -f "$out"; exit $status
fi
rm -f "$out"
echo "clang-tidy: ok ($(sources | wc -l) files)"
