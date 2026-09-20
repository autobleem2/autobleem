#!/usr/bin/env bash
# ci/progress.sh LOG - one line on where a ci/build.sh run is, from its log while it runs:
#
#   psc: configure + build (build_psc)  [ 67/99  68%]  running 4m12s  ~1m30s left
#   done in 489 s (native psc win)
#
# The stage is the last "==>" banner, the percentage ninja's last [n/N], the ETA a straight-line
# extrapolation of that phase's rate (ninja's numbers restart per configure, and the checks between phases
# print no counter, so "no counter yet" is what a stage says until its first compile line).
#
#   ssh psc-build ci/progress.sh ~/ci-fix.log        (or watch: ssh psc-build tail -f ~/ci-fix.log)
set -euo pipefail
log="${1:?usage: ci/progress.sh LOG}"
[ -f "$log" ] || { echo "no such log: $log"; exit 1; }

now=$(date +%s)
started=$(stat -c %W "$log" 2>/dev/null || echo 0)
[ "$started" -gt 0 ] || started=$(stat -c %Y "$log")
modified=$(stat -c %Y "$log")
fmt() { local s=$1; if [ "$s" -ge 3600 ]; then printf '%dh%02dm' $((s/3600)) $((s%3600/60)); elif [ "$s" -ge 60 ]; then printf '%dm%02ds' $((s/60)) $((s%60)); else printf '%ds' "$s"; fi; }

# finished?
if grep -q '^==> done in' "$log"; then
    echo "$(grep '^==> done in' "$log" | tail -1 | sed 's/^==> //; s/:$//') - $(grep -c '^==> .* done in' "$log") target(s) finished"
    exit 0
fi
if grep -qE 'ninja: build stopped|Errors while running CTest|FAIL|error:' "$log"; then
    echo "FAILED after $(fmt $((modified - started))): $(grep -E 'ninja: build stopped|Errors while running CTest|error:' "$log" | tail -1 | cut -c1-120)"
    exit 2
fi

stage="$(grep '^==> ' "$log" | tail -1 | sed 's/^==> //')"
[ -n "$stage" ] || stage="starting"
elapsed=$((now - started))

# ninja's last counter, and the rate over that phase
counter="$(tr '\r' '\n' < "$log" | grep -oE '^\[[0-9]+/[0-9]+\]' | tail -1 | tr -d '[]')"
if [ -n "$counter" ]; then
    n=${counter%/*}; total=${counter#*/}
    pct=$((n * 100 / total))
    # when this phase's first counter line was written: the log's mtime at that point is unknown, so the
    # phase's start is approximated by the banner's position - the elapsed time since the log grew past it
    eta=""
    if [ "$n" -gt 0 ] && [ "$n" -lt "$total" ]; then
        # rate from the whole run so far is the best we have without timestamps in the log
        phase_start_line=$(grep -n '^==> ' "$log" | tail -1 | cut -d: -f1)
        phase_lines=$(( $(wc -l < "$log") - phase_start_line ))
        [ "$phase_lines" -gt 0 ] || phase_lines=1
        # seconds per compiled target, taken as the run's elapsed time over the run's compiled lines
        compiled=$(tr '\r' '\n' < "$log" | grep -cE '^\[[0-9]+/[0-9]+\]' || true)
        [ "$compiled" -gt 0 ] || compiled=1
        left=$(( (total - n) * elapsed / compiled ))
        eta="  ~$(fmt "$left") left in this phase"
    fi
    printf '%s  [%s %3d%%]  running %s%s\n' "$stage" "$counter" "$pct" "$(fmt "$elapsed")" "$eta"
else
    printf '%s  (no counter yet)  running %s\n' "$stage" "$(fmt "$elapsed")"
fi
