#!/bin/sh
# A FAKE RetroArch runner for tests and for the VM tester (PLATFORM-24). It speaks the launcher's contract
# (src/code/evoui/ra_job.h) without touching anything real:
#
#     fake_ra_runner.sh install|remove [--bios] --progress FILE
#
# On a dev host point the launcher at it:  AB_RA_RUNNER=tools/fake_ra_runner.sh  (the launcher appends the op,
# [--bios] and --progress FILE; the console's "abupdate <root> --retroarch" form is the same words after the runner).
#
# It walks every phase key with sleeps, rewrites FILE atomically (FILE.tmp, then rename) about every 0.2 s, appends a
# human log to FILE.log, ends with end|ok (exit 0) - and on SIGTERM finishes the current step, writes end|stopped and
# exits 3, keeping FILE.resume so the next install carries on from that phase (the real runner keeps its .part files).
#
# Knobs (environment):
#   AB_FAKE_RA_FAIL=<code>   end|error|<code> (exit 1) at the first download phase: no-network, no-space, download,
#                            checksum, apt-provided, other
#   AB_FAKE_RA_DIE=1         die without an end line (kill -9 itself) after the first phase
#   AB_FAKE_RA_STEPS=<n>     sleeps of 0.2 s per phase (default 10 - a full install is about 20 s)
#   AB_FAKE_RA_BINARY=<path> install creates this file as its last step, remove deletes it first (a dev host's
#                            Env::retroArchInstalled() sees it)

op=
bios=0
file=
while [ $# -gt 0 ]; do
    case "$1" in
    install | remove) op=$1 ;;
    --bios) bios=1 ;;
    --progress)
        shift
        file=$1
        ;;
    *) ;; # the console's <root> --retroarch words, and anything else a runner command carries
    esac
    shift
done
if [ -z "$op" ] || [ -z "$file" ]; then
    echo "usage: $0 install|remove [--bios] --progress FILE" >&2
    exit 2
fi

log=$file.log
steps=${AB_FAKE_RA_STEPS:-10}
stopped=0
trap 'stopped=1' TERM

say() { echo "$(date '+%H:%M:%S') $*" >>"$log"; }

# write <line>: the whole file is replaced in one rename
write() {
    printf '%s\n' "$1" >"$file.tmp" && mv -f "$file.tmp" "$file"
}

finish_stopped() {
    say "stopped"
    write "end|stopped"
    exit 3
}

if [ "$op" = install ]; then
    phases="check-space download-retroarch download-cores download-libs download-apps download-assets"
    [ "$bios" = 1 ] && phases="$phases download-bios"
    phases="$phases unpack merge-config install-binary"
else
    phases="remove-files"
fi
n=0
for p in $phases; do n=$((n + 1)); done

start=1
if [ "$op" = install ] && [ -f "$file.resume" ]; then
    start=$(cat "$file.resume" 2>/dev/null)
    case "$start" in '' | *[!0-9]*) start=1 ;; esac
    say "resuming at phase $start"
fi
[ "$op" = install ] || rm -f "$file.resume"
say "$op bios=$bios phases=$n"

i=0
for key in $phases; do
    i=$((i + 1))
    [ "$i" -lt "$start" ] && continue
    # the 8-field phase line: unit, the package's name, package i of n (no phase writes a total of 0)
    total=$steps
    unit=items
    item=$key
    case "$key" in
    download-*)
        total=$((steps * 1000000))
        unit=bytes
        item=$key.tar.gz
        ;;
    unpack)
        unit=files
        item=cheats.zip
        ;;
    esac
    if [ "$key" = remove-files ] && [ -n "$AB_FAKE_RA_BINARY" ]; then
        rm -f "$AB_FAKE_RA_BINARY" # the binary goes first
    fi
    s=0
    while [ "$s" -lt "$steps" ]; do
        done_=$((s * total / steps))
        write "phase $i/$n|$key|$done_|$total|$unit|$item|1|1"
        sleep 0.2
        s=$((s + 1))
        if [ "$stopped" = 1 ]; then # the current step finished: keep what is done and go
            echo "$i" >"$file.resume"
            finish_stopped
        fi
        if [ -n "$AB_FAKE_RA_FAIL" ] && [ "$key" != check-space ] && [ "$s" -ge 3 ]; then
            say "failing with $AB_FAKE_RA_FAIL"
            write "end|error|$AB_FAKE_RA_FAIL"
            exit 1
        fi
        if [ "$AB_FAKE_RA_DIE" = 1 ] && [ "$i" -ge 2 ]; then
            say "dying without an end line"
            kill -9 $$
        fi
    done
    write "phase $i/$n|$key|$total|$total|$unit|$item|1|1"
done

if [ "$op" = install ] && [ -n "$AB_FAKE_RA_BINARY" ]; then
    mkdir -p "$(dirname "$AB_FAKE_RA_BINARY")" && : >"$AB_FAKE_RA_BINARY"
fi
rm -f "$file.resume"
say "done"
write "end|ok"
exit 0
