#!/bin/sh
#
# The part of rc/pe_run.sh that tests/rc/test_pe_run.cpp cannot check unprivileged: the bind mounts. Run as root on
# a Linux box (the pcusb-test VM's guest; the console has the same mount/umount):
#
#     sudo sh tests/rc/pe_run_mounts.sh <folder holding rc/ (the payload's Autobleem/rc)> [scratch folder]
#
# A fake mod's launch.sh reads its world through the absolute path /media/project_eris-style mount point, calls the
# dialog tools and stops sdl_display with killall; then a TERM, and a run killed with SIGKILL followed by a new one
# (the stale binds). Everything happens under the scratch folder (default /tmp/pe_mounts_test); prints "ok: ..." per
# check and "FAIL: ..." for each failed one, exits 1 if any failed.

RC=${1:?usage: sh pe_run_mounts.sh <rc folder> [scratch folder]}
S=${2:-/tmp/pe_mounts_test}
FAILS=0
ok() { echo "ok: $*"; }
bad() { echo "FAIL: $*"; FAILS=$((FAILS + 1)); }
check() { # check "what" "expected" "got"
    if [ "$2" = "$3" ]; then ok "$1 ($3)"; else bad "$1: expected '$2' got '$3'"; fi
}

[ "$(id -u)" = 0 ] || { echo "run as root (sudo)"; exit 2; }
rm -rf "$S"
mkdir -p "$S/Apps/pe-demo" "$S/applib" "$S/power" "$S/vol" "$S/rt"
cp -r "$RC" "$S/Autobleem_rc" 2>/dev/null
mkdir -p "$S/Autobleem"
rm -rf "$S/Autobleem/rc"
mv "$S/Autobleem_rc" "$S/Autobleem/rc"
printf 'launcher_filename="demo"\nlauncher_title="Demo"\n' > "$S/Apps/pe-demo/launcher.cfg"
echo gl > "$S/applib/libGL.so.1"
echo glu > "$S/applib/libGLU.so.1"
printf 1 > "$S/power/disable"
echo "030000004c050000da0c000011010000,Pad,a:b2,platform:Linux" > "$S/Autobleem/rc/pe_gamecontrollerdb.txt"
echo "user data" > "$S/Apps/pe-demo/save.dat"
mkdir -p "$S/Autobleem/bin/abpad"
echo "ABPAD SHIM" > "$S/Autobleem/bin/abpad/libabpad.so"
echo "THE MODS OWN REMAP" > "$S/Apps/pe-demo/drastic_sdl_remap.so"
printf "
[demo]
remap=drastic_sdl_remap.so
" >> "$S/Autobleem/rc/pe_compat.ini"

cat > "$S/Apps/pe-demo/launch.sh" <<'MOD'
#!/bin/sh
# a minimal killall by process name (comm), for a guest without one
killall() {
    sig=; [ "$1" = -0 ] && { sig=0; shift; }
    found=1
    for p in /proc/[0-9]*; do
        if [ "$(cat $p/comm 2>/dev/null)" = "$1" ]; then kill ${sig:+-0} ${p#/proc/} 2>/dev/null; found=0; fi
    done
    return $found
}
source "$PE_VOLATILE/project_eris.cfg"
OUT="$APP_OUT"
echo "pep=$PROJECT_ERIS_PATH" >> $OUT
echo "remap_seen=$(cat ./drastic_sdl_remap.so)" >> $OUT
echo "mounted_tree=$(grep -c " $PE_MOUNT_POINT " /proc/mounts)" >> $OUT
# the absolute path the original mods use
ABS="$PROJECT_ERIS_PATH/etc/project_eris/SUP/launchers/demo"
echo "abs_launch=$(ls $ABS/launch.sh 2>&1)" >> $OUT
echo "abs_save=$(cat $ABS/save.dat 2>&1)" >> $OUT
echo "abs_tool=$(ls $PROJECT_ERIS_PATH/bin/sdl_text_display 2>&1)" >> $OUT
echo "abs_db=$(wc -c < $PROJECT_ERIS_PATH/etc/boot_menu/gamecontrollerdb.txt)" >> $OUT
echo "lib=$(ls $PROJECT_ERIS_PATH/lib | tr '\n' ' ')" >> $OUT
echo "ipc=$(echo hello > $ABS/written_through_bind; cat $PE_APP_REAL/written_through_bind)" >> $OUT
sdl_text_display "Please wait" 640 120 12
sleep 0.3
echo "sdl_display_running=$(killall -0 sdl_display 2>&1 >/dev/null && echo yes || echo no)" >> $OUT
killall sdl_display
sleep 0.5
echo "sdl_display_after=$(killall -0 sdl_display 2>&1 >/dev/null && echo yes || echo no)" >> $OUT
sdl_input_text_display "q" 640 120 12 f 0 0 0 bg XOST
echo "answer=$?" >> $OUT
if [ -n "$HANG" ]; then sleep 300 & echo $! > "$OUT.child"; wait; fi
exit 5
MOD

run() {
    env AB_ROOT="$S" AB_RUNTIME_DIR="$S/rt" AB_LOG_DIR="$S/rt/logs" PE_TREE="$S/pe" PE_VOLATILE="$S/vol" \
        PE_POWER_FLAG="$S/power/disable" PE_MOUNT_POINT="$S/media/project_eris" PE_APPLIB="$S/applib" \
        AB_APP_VIRTUAL_PAD=0 APP_OUT="$S/out.txt" PE_APP_REAL="$S/Apps/pe-demo" HOME="$S/home" "$@"
}
# in the background, with the program's own pid in $! (the subshell execs it)
runbg() {
    ( exec env AB_ROOT="$S" AB_RUNTIME_DIR="$S/rt" AB_LOG_DIR="$S/rt/logs" PE_TREE="$S/pe" PE_VOLATILE="$S/vol" \
        PE_POWER_FLAG="$S/power/disable" PE_MOUNT_POINT="$S/media/project_eris" PE_APPLIB="$S/applib" \
        AB_APP_VIRTUAL_PAD=0 APP_OUT="$S/out.txt" PE_APP_REAL="$S/Apps/pe-demo" HOME="$S/home" "$@" ) &
}
get() { sed -n "s/^$1=//p" "$S/out.txt" | head -n 1; }

echo "== a normal run"
run sh "$S/Autobleem/rc/pe_run.sh" "$S/Apps/pe-demo"
check "exit status of the mod" 5 $?
check "PROJECT_ERIS_PATH is the mount point" "$S/media/project_eris" "$(get pep)"
check "the tree is mounted for the mod" 1 "$(get mounted_tree)"
check "the absolute launch.sh is there (the app bound under launchers/)" "$S/media/project_eris/etc/project_eris/SUP/launchers/demo/launch.sh" "$(get abs_launch)"
check "the user's data seen through the bind" "user data" "$(get abs_save)"
check "a write through the bind lands in the app folder" hello "$(get ipc)"
check "killall sdl_display finds the stand-in" yes "$(get sdl_display_running)"
check "and stops it" no "$(get sdl_display_after)"
check "dialog answer, first allowed letter" 100 "$(get answer)"
check "abpad is bound over the mod's remap for the run" "ABPAD SHIM" "$(get remap_seen)"
check "and the mod's own file is back after" "THE MODS OWN REMAP" "$(cat "$S/Apps/pe-demo/drastic_sdl_remap.so")"
check "lib/ has gl4es (from rc/pe/lib) and the pad remap link" "libGL.so.1 libGLU.so.1 sdl_remap_arm.so " "$(get lib)"
check "nothing mounted after" 0 "$(grep -c "$S" /proc/mounts)"
check "the placeholder is an empty folder again" "" "$(ls -A "$S/media/project_eris")"
check "the app's data is untouched" "user data" "$(cat "$S/Apps/pe-demo/save.dat")"
check "the tree is gone" no "$([ -e "$S/pe" ] && echo yes || echo no)"
check "power flag back" 1 "$(cat "$S/power/disable")"

echo "== a TERM while the mod runs"
rm -f "$S/out.txt" "$S/out.txt.child"
runbg HANG=1 sh "$S/Autobleem/rc/pe_run.sh" "$S/Apps/pe-demo"
PID=$!
n=0
while [ ! -f "$S/out.txt.child" ] && [ $n -lt 100 ]; do sleep 0.1; n=$((n + 1)); done
sleep 0.3
check "mounted while it runs" 1 "$(get mounted_tree)"
kill -TERM $PID
wait $PID
check "exit 143" 143 $?
check "nothing mounted after the TERM" 0 "$(grep -c "$S" /proc/mounts)"
check "the child is gone" no "$(kill -0 "$(cat "$S/out.txt.child")" 2>/dev/null && echo yes || echo no)"
check "the app's data is untouched" "user data" "$(cat "$S/Apps/pe-demo/save.dat")"

echo "== a run killed with SIGKILL leaves its binds; the next run clears them and the app's data survives"
rm -f "$S/out.txt" "$S/out.txt.child"
runbg HANG=1 sh "$S/Autobleem/rc/pe_run.sh" "$S/Apps/pe-demo"
PID=$!
n=0
while [ ! -f "$S/out.txt.child" ] && [ $n -lt 100 ]; do sleep 0.1; n=$((n + 1)); done
sleep 0.3
kill -KILL $PID
wait $PID 2>/dev/null
kill "$(cat "$S/out.txt.child")" 2>/dev/null
LEFT=$(grep -c "$S" /proc/mounts)
check "binds are left behind (the tree, and the app under it)" yes "$([ "$LEFT" -ge 2 ] && echo yes || echo no)"
rm -f "$S/out.txt"
run sh "$S/Autobleem/rc/pe_run.sh" "$S/Apps/pe-demo"
check "the next run works" 5 $?
check "and is mounted for the mod" 1 "$(get mounted_tree)"
check "nothing mounted after it" 0 "$(grep -c "$S" /proc/mounts)"
check "the app's data is untouched" "user data" "$(cat "$S/Apps/pe-demo/save.dat")"

# never rm -rf through a bind that is still there
if grep -q "$S" /proc/mounts; then
    echo "mounts left under $S - not removing it"
    FAILS=$((FAILS + 1))
else
    rm -rf "$S"
fi
[ "$FAILS" = 0 ] && echo "all ok" || echo "$FAILS failed"
[ "$FAILS" = 0 ]
