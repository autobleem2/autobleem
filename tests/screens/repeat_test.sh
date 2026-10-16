#!/bin/sh
# repeat_test.sh N EXE - runs EXE N times; fails (printing "failures K/N" and the stderr of the first) if any run
# exits other than 0; exits 77 (ctest: skipped) if every run skipped itself
n=$1
exe=$2
failed=0
skipped=0
first=
i=0
while [ "$i" -lt "$n" ]; do
    i=$((i + 1))
    out=$("$exe" 2>&1)
    rc=$?
    if [ "$rc" -eq 77 ]; then
        skipped=$((skipped + 1))
    elif [ "$rc" -ne 0 ]; then
        failed=$((failed + 1))
        [ -n "$first" ] || first="run $i exit $rc: $out"
    fi
done
echo "failures $failed/$n, skipped $skipped/$n"
if [ "$failed" -ne 0 ]; then
    echo "$first"
    exit 1
fi
[ "$skipped" -eq "$n" ] && exit 77
exit 0
