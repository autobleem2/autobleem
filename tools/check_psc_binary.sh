#!/usr/bin/env bash
# check_psc_binary.sh BINARY TOOLCHAIN_ROOT - would this binary load on the PlayStation Classic?
#
# The console's stock glibc is 2.24 and its libstdc++ is 6.0.22, whose newest symbol version is
# GLIBCXX_3.4.22. The Sony toolchain's own libstdc++ is newer, so a C++ library feature can link fine on
# the build server and then fail to load on the console with "version GLIBCXX_3.4.26 not found". And a
# RPATH/RUNPATH in the binary would name a directory on the build server. make_psc.sh runs this on the
# server, with the toolchain's readelf, before it fetches the binary. Exit 1 on any of the three.
set -euo pipefail
bin="$1"
toolchain="${2:-/opt/toolchain}"
readelf="$(ls "$toolchain"/bin/*-readelf "$toolchain"/bin/*-readelf.exe 2>/dev/null | head -1 || true)"
[ -n "$readelf" ] || readelf=readelf

# the highest version of a symbol family the binary needs, e.g. GLIBC_2.7 / GLIBCXX_3.4.22
highest() { "$readelf" -V "$bin" | grep -o "$1_[0-9][0-9.]*" | sort -t. -k1,1 -k2,2n -k3,3n -u | tail -1; }
# "2.24" >= "2.7"? - numeric, component by component
newer_than() {   # newer_than A B: true when version A is newer than B
    local a="$1" b="$2" i x y
    for i in 1 2 3; do
        x=$(echo "$a" | cut -d. -f$i); y=$(echo "$b" | cut -d. -f$i)
        x=${x:-0}; y=${y:-0}
        [ "$x" -gt "$y" ] && return 0
        [ "$x" -lt "$y" ] && return 1
    done
    return 1
}

glibc="$(highest GLIBC)"; gxx="$(highest GLIBCXX)"
echo "    $bin needs ${glibc:-no GLIBC version} and ${gxx:-no GLIBCXX version}"
status=0
if [ -n "$glibc" ] && newer_than "${glibc#GLIBC_}" "2.24"; then
    echo "    ERROR: $glibc is newer than the console's glibc 2.24"; status=1
fi
if [ -n "$gxx" ] && newer_than "${gxx#GLIBCXX_}" "3.4.22"; then
    echo "    ERROR: $gxx is newer than the console's libstdc++ 6.0.22 (GLIBCXX_3.4.22)"; status=1
fi
if "$readelf" -d "$bin" | grep -qE 'RPATH|RUNPATH'; then
    echo "    ERROR: the binary carries an RPATH/RUNPATH:"; "$readelf" -d "$bin" | grep -E 'RPATH|RUNPATH'; status=1
fi
[ "$status" -eq 0 ] && echo "    ok"
exit $status
