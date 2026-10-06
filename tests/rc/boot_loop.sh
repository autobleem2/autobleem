#!/bin/sh
#
# rc/boot.sh's loop and its config readers, with the console's tools stubbed. Run as root in a THROWAWAY Linux
# root (a busybox container such as alpine - the console's shell is busybox ash): the script lays a fake stick at
# /media and a fake /etc/xdg/weston/weston.ini, so never on a real machine or the console:
#
#     docker run --rm -v <repo>/payload/Autobleem/rc:/rc:ro -v <repo>/tests/rc:/t:ro alpine sh /t/boot_loop.sh /rc
#
# Checks (prints "ok: ..." / "FAIL: ..." per check, exit 1 if any failed):
#   update     a boot.sh the stick gets in the middle of a round (an update) is the one the NEXT round runs - the
#              same shell process (exec, no second loop), the one-time setup not done twice
#   unchanged  with no update the loop is not restarted
#   config     apply_output_mode and launch_rb.sh read the launcher's real config.ini: /media/Autobleem/bin/autobleem/
#              config.ini, "Outputmode=" / "Theme=" with a capital, CRLF line ends, any case of the key

RC_SRC=${1:?usage: sh boot_loop.sh <payload rc folder>}
FAILS=0
ok() { echo "ok: $*"; }
bad() { echo "FAIL: $*"; FAILS=$((FAILS + 1)); }
check() { # check "what" "expected" "got"
    if [ "$2" = "$3" ]; then ok "$1 ($3)"; else bad "$1: expected '$2' got '$3'"; fi
}

[ "$(id -u)" = 0 ] || { echo "run as root"; exit 2; }
[ -d /media/Autobleem ] && { echo "refusing: /media/Autobleem exists - a real stick? (throwaway root only)"; exit 2; }

T=/tmp/bootloop_t
STUBS=$T/bin
ROUNDS=$T/rounds
reset_world() {
    [ -s /tmp/.absplash.pid ] && kill "$(cat /tmp/.absplash.pid)" 2> /dev/null
    rm -rf /media /tmp/autobleem /tmp/weston.* /tmp/.absplash.pid /tmp/.abload /tmp/absplash /tmp/*.jpg "$T"
    mkdir -p /media/Autobleem/rc /media/Autobleem/bin/autobleem/splash /media/System /etc/xdg/weston "$STUBS"
    cp -r "$RC_SRC"/. /media/Autobleem/rc/
    # the console's tools, no-ops that leave a trace
    for tool in systemctl udevadm mount insmod killall sync; do
        printf '#!/bin/sh\necho "%s $*" >> %s/tools.log\n' "$tool" "$T" > "$STUBS/$tool"
        chmod +x "$STUBS/$tool"
    done
    # the scripts boot.sh calls that need the console
    for s in killsony unpack_libs backup checkstick ssh_keys pad_seat ab_watch; do
        printf '#!/bin/sh\nexit 0\n' > /media/Autobleem/rc/$s.sh
        chmod +x /media/Autobleem/rc/$s.sh
    done
    printf '#!/bin/sh\nexec sleep 30 # absplash_stub\n' > /media/Autobleem/bin/autobleem/absplash
    chmod +x /media/Autobleem/bin/autobleem/absplash
    echo jpg > /media/Autobleem/bin/autobleem/splash/autobleem.jpg
    echo jpg > /media/Autobleem/bin/autobleem/splash/updating.jpg
    echo jpg > /media/Autobleem/bin/autobleem/splash/poweroff.jpg
    echo rules > /media/Autobleem/rc/20-joystick.rules
    printf '[core]\nmode=1280x720\n' > /etc/xdg/weston/weston.ini
    : > "$ROUNDS"
    : > "$T/tools.log"
}
# the launcher: autobleem.sh says what the round was; $ROUND_HOOK (set per test) runs in round 1 (an update)
write_launcher_and_selection() {
    cat > /media/Autobleem/rc/autobleem.sh << 'EOF'
#!/bin/sh
echo "launcher mode=$(cat /tmp/weston.mode 2>/dev/null)" >> /tmp/bootloop_t/rounds
[ -n "$ROUND_HOOK" ] && [ "$(wc -l < /tmp/bootloop_t/rounds)" = 1 ] && sh -c "$ROUND_HOOK"
exit 0
EOF
    # selection.sh: round 3 ends the loop (a non-zero status = reboot, boot.sh's own end). $PPID is the shell
    # running the loop - the pid must not change over the rounds
    cat > /media/Autobleem/rc/selection.sh << 'EOF'
#!/bin/sh
echo "loop pid=$PPID" >> /tmp/bootloop_t/rounds
[ "$(grep -c '^launcher' /tmp/bootloop_t/rounds)" -ge 3 ] && exit 1
exit 0
EOF
    chmod +x /media/Autobleem/rc/autobleem.sh /media/Autobleem/rc/selection.sh
}
run_boot() { # the way Sony's usb_watch runs it: start.sh sources boot.sh from the rc folder
    ( cd /media/Autobleem/rc && PATH="$STUBS:$PATH" ROUND_HOOK="$ROUND_HOOK" timeout 120 sh -c '. ./boot.sh' > "$T/boot.out" 2>&1 )
}
bootlog() { cat /tmp/autobleem/logs/boot.log 2> /dev/null; }

# ---- update -------------------------------------------------------------------------------------------------
reset_world
write_launcher_and_selection
# the "update": round 1 replaces boot.sh on the stick with a newer one (here: the same file whose loop line
# says v2 in the log)
ROUND_HOOK="sed -i 's/loop: launcher round starts/loop: launcher round starts v2/' /media/Autobleem/rc/boot.sh"
run_boot
log=$(bootlog)
check "update: rounds run" 3 "$(grep -c '^launcher' $ROUNDS)"
check "update: round 1 is the old boot.sh" 1 "$(echo "$log" | grep -c 'loop: launcher round starts$')"
check "update: round 2+3 are the new boot.sh" 2 "$(echo "$log" | grep -c 'loop: launcher round starts v2')"
check "update: one loop shell all through" 1 "$(grep '^loop pid' $ROUNDS | sort -u | wc -l)"
check "update: the one-time setup ran once" 1 "$(echo "$log" | grep -c 'udev rules reloaded')"
check "update: one picture process started" 1 "$(echo "$log" | grep -c 'absplash started')"
check "update: the restart is logged once" 1 "$(echo "$log" | grep -c "on the stick changed")"
check "update: ends in a reboot" 1 "$(grep -c 'systemctl reboot' $T/tools.log)"

# ---- unchanged ----------------------------------------------------------------------------------------------
reset_world
write_launcher_and_selection
ROUND_HOOK=
run_boot
log=$(bootlog)
check "unchanged: rounds run" 3 "$(grep -c '^launcher' $ROUNDS)"
check "unchanged: no restart of the loop" 0 "$(echo "$log" | grep -c 'on the stick changed')"
check "unchanged: one-time setup once" 1 "$(echo "$log" | grep -c 'udev rules reloaded')"

# ---- config -------------------------------------------------------------------------------------------------
cfg=/media/Autobleem/bin/autobleem/config.ini
for line in "Outputmode=720x480" "outputmode=720x480" "OUTPUTMODE=720x480"; do
    reset_world
    write_launcher_and_selection
    printf '[Config]\r\nLanguage=English\r\n%s\r\nTheme=ab2.0.0\r\n' "$line" > $cfg
    ROUND_HOOK=
    run_boot
    check "config ($line): Weston asked for 720x480" 1 "$(cat /tmp/autobleem/logs/display.log 2> /dev/null | grep -c 'Weston restarted in 720x480')"
done
# a mode the launcher does not know falls back to 720, an empty value too
reset_world
write_launcher_and_selection
printf '[Config]\nOutputmode=auto\n' > $cfg
ROUND_HOOK=
run_boot
check "config (auto): stays 720, no restart" 0 "$(cat /tmp/autobleem/logs/display.log 2> /dev/null | grep -c 'Weston restarted')"
# the pending mode wins over the saved one
reset_world
write_launcher_and_selection
printf '[Config]\nOutputmode=720\n' > $cfg
mkdir -p /tmp/autobleem
echo 1080 > /tmp/autobleem/outputmode.pending
ROUND_HOOK=
run_boot
check "config: pending 1080 beats saved 720" 1 "$(cat /tmp/autobleem/logs/display.log 2> /dev/null | grep -c 'Weston restarted in 1080')"

# launch_rb.sh's theme reader: the function it sources from ab_log.sh
reset_world
printf '[Config]\r\nTheme=ab2.0.0\r\n' > $cfg
got=$(PATH="$STUBS:$PATH" sh -c '. /media/Autobleem/rc/ab_log.sh; ab_config_get theme')
check "config: ab_config_get theme (CRLF, capital key)" "ab2.0.0" "$got"
printf 'THEME=aergb\n' > $cfg
got=$(PATH="$STUBS:$PATH" sh -c '. /media/Autobleem/rc/ab_log.sh; ab_config_get theme')
check "config: ab_config_get theme (upper case key)" "aergb" "$got"
got=$(PATH="$STUBS:$PATH" sh -c '. /media/Autobleem/rc/ab_log.sh; ab_config_get nokey')
check "config: ab_config_get of a missing key is empty" "" "$got"
grep -q 'ab_config_get theme' /media/Autobleem/rc/launch_rb.sh && ok "launch_rb.sh uses ab_config_get" || bad "launch_rb.sh does not use ab_config_get"

[ -s /tmp/.absplash.pid ] && kill "$(cat /tmp/.absplash.pid)" 2> /dev/null
[ $FAILS -eq 0 ] && echo "all passed" || echo "$FAILS FAILED"
[ $FAILS -eq 0 ]
