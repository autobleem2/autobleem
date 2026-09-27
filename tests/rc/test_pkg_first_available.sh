#!/usr/bin/env bash
#
# K14: payload_linux/install.sh's pkg_first_available() under `set -euo pipefail`.
#
# The bug: pkg_first_available() probed with `apt-cache policy "$name" 2>/dev/null | grep -q '...'`.
# grep -q exits as soon as it has its first match, without reading the rest of its input; when apt-cache's
# output is longer than one pipe buffer, apt-cache is then killed by SIGPIPE (exit 141) while writing to a
# reader that has already gone away, `pipefail` turns that into the pipeline's exit status, and the `if`
# around it becomes false regardless of whether the candidate line was really there. Under `set -euo
# pipefail` (install.sh line 16) that would abort the script outright were the `if` not there; as it is,
# every call falls through to `echo "$1"` - the function always "succeeds" with its FIRST argument, so on
# Debian Trixie (`pkg_first_available libpng16-16t64 libpng16-16`, both spelled the other way round
# elsewhere) the wrong, or a nonexistent, package name is returned every time.
#
# Fix: read the grep's whole input (`grep '...' >/dev/null` instead of `grep -q ...`) so the pipe is
# drained and apt-cache exits 0 normally - no SIGPIPE, no pipefail trip.
#
# Run: tests/rc/test_pkg_first_available.sh [path-to-install.sh]
# Exit 0 = the real behaviour (only an existing package name is ever returned) is correct.
# Exit 1 = the bug is present (or something else in the extraction/harness broke - see stderr).
# Needs bash on PATH (MSYS2 UCRT64 on Windows); skips (exit 0) with a note when there is none.
#
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_SH="${1:-$SCRIPT_DIR/../../payload_linux/install.sh}"

if ! command -v bash >/dev/null 2>&1; then
    echo "SKIP: no bash on PATH"
    exit 0
fi

if [ ! -f "$INSTALL_SH" ]; then
    echo "FAIL: $INSTALL_SH not found"
    exit 1
fi

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

# --- extract pkg_first_available() verbatim from install.sh, so this test exercises the real function
# body rather than a hand copy that could quietly drift from it ---
FUNC_FILE="$WORK/pkg_first_available.sh"
awk '/^pkg_first_available\(\) \{/{p=1} p{print; if (/^\}/) exit}' "$INSTALL_SH" > "$FUNC_FILE"
if [ ! -s "$FUNC_FILE" ] || ! grep -q '^pkg_first_available() {' "$FUNC_FILE"; then
    echo "FAIL: could not extract pkg_first_available() from $INSTALL_SH"
    exit 1
fi

# --- a fake apt-cache: "policy <name>" prints a real Candidate line for libpng16-16 only, preceded/followed
# by enough filler (well over the 64 KB a pipe buffer typically holds) that a grep -q reader which stops at
# its first match leaves unread bytes in the pipe - the condition SIGPIPE needs to actually fire. An
# unknown package gets "(none)" the way real apt-cache does. ---
mkdir -p "$WORK/bin"
cat > "$WORK/bin/apt-cache" <<'APTCACHE'
#!/usr/bin/env bash
set -uo pipefail
if [ "$1" != "policy" ]; then
    exit 1
fi
name="$2"
# ~100 KB of filler lines, same shape as apt-cache's real chatter, so a short-circuiting reader really
# leaves data behind in the pipe.
filler() {
    for _ in $(seq 1 2000); do
        printf '  Version table:\n     1.2.3-4 500\n        500 http://deb.example/debian bookworm/main amd64 Packages\n'
    done
}
case "$name" in
    libpng16-16)
        printf '%s:\n' "$name"
        printf '  Installed: (none)\n'
        printf '  Candidate: 1.6.39-1\n'
        filler
        ;;
    *)
        printf '%s:\n' "$name"
        printf '  Installed: (none)\n'
        printf '  Candidate: (none)\n'
        filler
        ;;
esac
APTCACHE
chmod +x "$WORK/bin/apt-cache"

# --- run pkg_first_available under the same shell options install.sh uses, with our fake apt-cache first
# on PATH, and ask for the Trixie name before the Bookworm one - only the Bookworm one "exists". ---
RESULT="$(PATH="$WORK/bin:$PATH" bash -c '
    set -euo pipefail
    source "'"$FUNC_FILE"'"
    pkg_first_available libpng16-16t64 libpng16-16
' 2>"$WORK/stderr")"
STATUS=$?

echo "--- pkg_first_available libpng16-16t64 libpng16-16 ---"
echo "exit status: $STATUS"
echo "stdout: $RESULT"
echo "stderr:"
cat "$WORK/stderr"

if [ "$STATUS" -ne 0 ]; then
    echo "FAIL: pkg_first_available aborted (exit $STATUS) - SIGPIPE escaped past pipefail"
    exit 1
fi

if [ "$RESULT" != "libpng16-16" ]; then
    echo "FAIL: expected 'libpng16-16' (the package that actually exists), got '$RESULT'"
    exit 1
fi

echo "PASS"
exit 0
