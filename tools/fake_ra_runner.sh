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
#   AB_FAKE_RA_BINARY=<path> install creates this file as its last step, remove deletes it first. Give the same
#                            variable to the LAUNCHER: Env::retroArchInstalled() then asks only this file, whatever
#                            /usr/bin/retroarch the host has - no file = the "Not installed" panel (ra-1b / ra-1c)
#   AB_FAKE_NO_ROUTE=1       (launcher, not the runner) System::hasDefaultRoute() says no network: the panel's Install
#                            row reads "No network - connect in Network & Controllers" (T2); without it, with
#                            AB_FAKE_RA_BINARY pointing at a missing file, the panel shows Install enabled (T3)
#   AB_FAKE_RA_VERSION=<v>   with AB_FAKE_RA_BINARY: install writes <v> to VERSION next to it (the stamp the
#                            launcher's "Version %s is ready" box reads when the binary sits in <root>/RetroArch/bin);
#                            without a stamp the launcher falls back to AB_UPDATE_RETROARCH_VERSION. The free space of
#                            that box is a statvfs of the stick: it is never from the runner.
#   AB_FAKE_RA_SCENARIO=<s>  the mockups' states (default: unset = the plain walk above, one package per phase):
#       full     install: all 9 phases (10 with --bios), 1/1/9/3/3/6/9/1/1 packages ("Package 3 of 9" in the
#                cores phase), real-looking MB totals, cheats.zip 41000 files as package 5 and the cores tar.gz
#                290 MB as package 3 of the unpack phase; every package takes AB_FAKE_RA_STEPS ticks
#       cores    install, held in "Downloading cores", phase 3/9, package 3 of 9: a resumed download goes from
#                110 to 128 MB of 289 at AB_FAKE_RA_MBPS10/10 MB/s (default 24 = 2.4 MB/s, so the launcher's meter
#                shows 2.4 MB/s and about a minute left), then holds at "128 of 289 MB" for AB_FAKE_RA_HOLD seconds
#                creeping 1 KB a tick (the meter then dies away: take the speed/ETA shot on the way)
#       cheats   install, unpack phase 7/9, package 5 of 9, files up to 12400 of 41000, held
#       targz    install, unpack phase 7/9, package 3 of 9, 120 of 290 MB of the tar.gz read, held
#       remove7  remove: 1 phase, 7 packages (8 with --bios, BIOS files last), package 2 held at 3120 of 7400
#                files; any scenario name with op remove walks this
#   AB_FAKE_RA_HOLD=<s>      seconds a showcase scenario holds its mockup number (default 60)
#   AB_FAKE_RA_MBPS10=<n>    cores scenario: speed in tenths of MB/s (default 24)
#   (AB_FAKE_RA_FAIL also fails a Remove: end|error|<code>, the launcher maps any code but apt-provided to "Removal
#   failed"; AB_FAKE_RA_DIE also kills a Remove, 3 ticks into it.)
#   Speed and time left are not in the file: the launcher measures them from the bytes of the lines over time.

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

# the end of a good run (shared by the plain walk and the scenarios)
finish_ok() {
    if [ "$op" = install ] && [ -n "$AB_FAKE_RA_BINARY" ]; then
        mkdir -p "$(dirname "$AB_FAKE_RA_BINARY")" && : >"$AB_FAKE_RA_BINARY"
        # the stamp the launcher's 4a box reads ("Version %s is ready"): <root>/RetroArch/bin/VERSION next to the binary
        [ -n "$AB_FAKE_RA_VERSION" ] && printf '%s\n' "$AB_FAKE_RA_VERSION" >"$(dirname "$AB_FAKE_RA_BINARY")/VERSION"
    fi
    rm -f "$file.resume"
    say "done"
    write "end|ok"
    exit 0
}

remove_binary() {
    [ -n "$AB_FAKE_RA_BINARY" ] || return 0
    rm -f "$AB_FAKE_RA_BINARY" "$(dirname "$AB_FAKE_RA_BINARY")/VERSION"
}

#******************
# Scenarios (AB_FAKE_RA_SCENARIO): the mockups' states with their package names, package i of n and units
#******************
# One tick is 0.2 s and one line. A package walks its numbers in AB_FAKE_RA_STEPS ticks; the "showcase" scenarios stop
# at the mockup's number and write the same line for AB_FAKE_RA_HOLD seconds (default 60) so it can be photographed.
fail_ticks=0
tick() {
    sleep 0.2
    if [ "$stopped" = 1 ]; then
        echo "$ph" >"$file.resume"
        finish_stopped
    fi
    fail_ticks=$((fail_ticks + 1))
    if [ -n "$AB_FAKE_RA_FAIL" ] && [ "$key" != check-space ] && [ "$fail_ticks" -ge 3 ]; then
        say "failing with $AB_FAKE_RA_FAIL"
        write "end|error|$AB_FAKE_RA_FAIL"
        exit 1
    fi
    if [ "$AB_FAKE_RA_DIE" = 1 ] && { [ "$ph" -ge 2 ] || [ "$pi" -ge 2 ]; }; then
        say "dying without an end line"
        kill -9 $$
    fi
}

# emit <done> <total>: the 8-field phase line of the current package
emit() { write "phase $ph/$np|$key|$1|$2|$unit|$item|$pi|$pn"; }

# walk <from> <to> <total> <ticks>: done goes from..to in ticks lines, the last one exactly `to`
walk() {
    w=0
    while [ "$w" -lt "$4" ]; do
        emit $(($1 + ($2 - $1) * w / $4)) "$3"
        tick
        w=$((w + 1))
    done
    emit "$2" "$3"
}

# hold <done> <total> <creep>: the same line for AB_FAKE_RA_HOLD seconds; creep > 0 adds that many bytes a tick, so
# a download's meter keeps a (small) reading instead of dying away to 0
hold() {
    h=0
    hn=$((${AB_FAKE_RA_HOLD:-60} * 5))
    hd=$1
    while [ "$h" -lt "$hn" ]; do
        emit "$hd" "$2"
        tick
        hd=$((hd + $3))
        h=$((h + 1))
    done
}

# pkg_set <key> <i> <bios>: unit, item and total of package i of the full install's phase
pkg_set() {
    case "$1" in
    check-space) unit=items item=check-space total=4 ;;
    download-retroarch) unit=bytes item=retroarch-psc-1.23.0.tar.gz total=42000000 ;;
    download-cores)
        unit=bytes item=cores-psc-20261003-$2.tar.gz total=$((60000000 + $2 * 7000000))
        [ "$2" = 3 ] && total=289000000
        ;;
    download-libs) unit=bytes item=libs-$2.tar.gz total=$((12000000 + $2 * 3000000)) ;;
    download-apps) unit=bytes item=apps-$2.zip total=$((8000000 + $2 * 2500000)) ;;
    download-assets) unit=bytes item=libretro-assets-$2.zip total=$((9000000 + $2 * 6500000)) ;;
    download-bios) unit=bytes item=bios-$2.zip total=$((2000000 + $2 * 1500000)) ;;
    unpack)
        unit=files item=unpack-$2.zip total=$((300 + $2 * 450))
        if [ "$2" = 5 ]; then
            item=cheats.zip total=41000
        elif [ "$2" = 3 ]; then
            unit=bytes item=cores-psc-20261003.tar.gz total=290000000
        fi
        ;;
    merge-config) unit=items item=merge-config total=3 ;;
    install-binary) unit=items item=install-binary total=2 ;;
    esac
}

# how many packages a phase of the full install has (cores 9, as in the mockup: "Package 3 of 9")
pkg_count() {
    case "$1" in
    download-cores | unpack) echo 9 ;;
    download-libs | download-apps) echo 3 ;;
    download-assets) echo 6 ;;
    download-bios) echo 2 ;;
    *) echo 1 ;;
    esac
}

REMOVE_NAMES="RetroArch_itself Emulator_cores Menu_assets Databases Cheats Overlays Shaders BIOS_files"

run_scenario() {
    key= unit= item= ph=1 np=1 pi=0 pn=0
    ticks=$steps
    case "$1" in
    full)
        keys="check-space download-retroarch download-cores download-libs download-apps download-assets"
        [ "$bios" = 1 ] && keys="$keys download-bios"
        keys="$keys unpack merge-config install-binary"
        np=0
        for key in $keys; do np=$((np + 1)); done
        say "scenario full bios=$bios phases=$np"
        start=1
        if [ -f "$file.resume" ]; then
            start=$(cat "$file.resume" 2>/dev/null)
            case "$start" in '' | *[!0-9]*) start=1 ;; esac
            say "resuming at phase $start"
        fi
        ph=0
        for key in $keys; do
            ph=$((ph + 1))
            [ "$ph" -lt "$start" ] && continue
            pn=$(pkg_count "$key")
            pi=0
            while [ "$pi" -lt "$pn" ]; do
                pi=$((pi + 1))
                pkg_set "$key" "$pi"
                walk 0 "$total" "$total" "$ticks"
            done
        done
        ;;
    cores)
        # a resumed download: the .part has 110 MB, then the speed is AB_FAKE_RA_MBPS10/10 MB/s (default 2.4) up to
        # 128 MB, where it holds (1 KB a tick: the line stays "128 of 289 MB"), then it goes on to the end
        ph=3 np=9 key=download-cores pi=3 pn=9 unit=bytes item=cores-psc-20261003.tar.gz
        say "scenario cores"
        per=$((${AB_FAKE_RA_MBPS10:-24} * 20000))
        walk 110000000 128000000 289000000 $((18000000 / per))
        hold 128000000 289000000 1000
        walk 128000000 289000000 289000000 "$ticks"
        ;;
    cheats)
        ph=7 np=9 key=unpack pi=5 pn=9 unit=files item=cheats.zip
        say "scenario cheats"
        walk 0 12400 41000 "$ticks"
        hold 12400 41000 0
        walk 12400 41000 41000 "$ticks"
        ;;
    targz)
        ph=7 np=9 key=unpack pi=3 pn=9 unit=bytes item=cores-psc-20261003.tar.gz
        say "scenario targz"
        walk 0 120000000 290000000 "$ticks"
        hold 120000000 290000000 0
        walk 120000000 290000000 290000000 "$ticks"
        ;;
    remove7)
        ph=1 np=1 key=remove-files unit=files
        pn=7
        names="RetroArch_itself Emulator_cores Menu_assets Databases Cheats Overlays Shaders"
        [ "$bios" = 1 ] && pn=8 && names="$REMOVE_NAMES"
        say "scenario remove7 packages=$pn"
        remove_binary
        pi=0
        for item in $names; do
            pi=$((pi + 1))
            if [ "$pi" = 2 ]; then
                walk 0 3120 7400 "$ticks"
                hold 3120 7400 0
                walk 3120 7400 7400 "$ticks"
            else
                walk 0 7400 7400 "$ticks"
            fi
        done
        ;;
    *)
        echo "unknown AB_FAKE_RA_SCENARIO '$1' (full, cores, cheats, targz, remove7)" >&2
        exit 2
        ;;
    esac
    finish_ok
}

if [ -n "$AB_FAKE_RA_SCENARIO" ]; then
    # a Remove always walks the Remove packages (any scenario name); the showcase scenarios cores/cheats/targz are
    # installs, remove7 is a remove
    if [ "$op" = remove ]; then
        rm -f "$file.resume"
        run_scenario remove7
    fi
    if [ "$AB_FAKE_RA_SCENARIO" = remove7 ]; then
        echo "scenario remove7 needs op remove" >&2
        exit 2
    fi
    run_scenario "$AB_FAKE_RA_SCENARIO"
fi

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
        remove_binary # the binary goes first
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
        if [ "$AB_FAKE_RA_DIE" = 1 ] && { [ "$i" -ge 2 ] || { [ "$op" = remove ] && [ "$s" -ge 3 ]; }; }; then
            say "dying without an end line"
            kill -9 $$
        fi
    done
    write "phase $i/$n|$key|$total|$total|$unit|$item|1|1"
done

finish_ok
