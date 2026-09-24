#!/bin/sh

# AutoBleem's RetroArch launcher - what RetroBoot's launch_rfa_rom.sh / launch_rfa.sh used to do, for the
# stick's own layout (RetroArch/bin is RetroArch's tree, RetroArch/bios its system directory, RetroArch/roms
# the other systems' games) and our own RetroArch build (github.com/autobleem/retroarch-psc: the cores
# are unpacked by RetroArch itself, libstdc++ is inside it - no RetroBoot libraries on the path).
#
#   launch_rb.sh <file> <core>   a game: <core> is a core's path, or NEON / PEOPS for a PS1 game
#                                (LaunchService::launchRetroArch's argv)
#   launch_rb.sh                 RetroArch's own menu, nothing loaded (rc/retroarch.sh)
#
# What this script says goes to launch.log in the logs dir, RetroArch's own output to retroarch.log there -
# both fresh for every launch, in RAM unless the logs are kept (rc/ab_log.sh); a crash takes them to
# System/Logs/crash-<n> with the kernel's last lines. A crash comes straight back to the launcher -
# RetroBoot relaunched five times (the blinking red LED for 45 s).

RA=/media/RetroArch
BIN=$RA/bin
BIOS=$RA/bios
ROMS=$RA/roms
. /media/Autobleem/rc/ab_log.sh
LOGS=$AB_LOG_DIR
RALOG=$LOGS/retroarch.log
# what the launcher starts RetroArch with on top of retroarch.cfg - config_save_on_exit (Options ->
# "Persist RetroArch config") and a game's own settings - in RAM (LaunchService::prepareRaAppend)
RA_APPEND=$AB_RUNTIME_DIR/ra-append.cfg
ABSPLASH=/media/Autobleem/bin/autobleem/absplash
ABPICS=/media/Autobleem/bin/autobleem/splash

exec > "$LOGS/launch.log" 2>&1
echo "=== launch_rb.sh $(date '+%Y-%m-%d %H:%M:%S')"
echo "Image: $1"
echo "Core: $2"

# kill sony stuffs and set powermanagement parameters
killall -s KILL showLogo sonyapp ui_menu auto_dimmer pcsx dimmer 2>/dev/null
echo 2 > /data/power/disable

led()
{
	echo "$1" > /sys/class/leds/red/brightness
	echo "$2" > /sys/class/leds/green/brightness
}

# the splashes: absplash shows a picture in the same full-screen window the launcher and RetroArch use.
# The launch picture stays until RetroArch's log says its video is up (/tmp/.ra_up is touched then);
# the return picture until the launcher's window is back (it removes /tmp/.abload itself).
# "Up" is RetroArch 1.22's "[Video] Found display server" line: video_driver_init_internal() prints it
# right after the video driver's init returned - the window and the GL context exist - whatever the
# driver (under Wayland the server it names is "null"). RetroArch logs nothing without --verbose,
# hence the flag in retroarch_run; RetroBoot's "Found display driver" never appears in 1.22's output,
# so the splash used to sit out its whole timeout. The second after it covers the first frame; the
# 30 s are the fallback (a retroarch.cfg with frontend_log_level above 1 hides the line) and
# RetroArch's exit ends the wait at once.
show_launch_splash()
{
	[ -x "$ABSPLASH" ] || return
	rm -f /tmp/.ra_up
	"$ABSPLASH" "$ABPICS/retroarch.jpg" --until-exists /tmp/.ra_up --timeout 30 &
	N=0
	while ! grep -q "\[Video\] Found display server" "$RALOG" 2>/dev/null; do
		[ -e /tmp/.ra_up ] && return
		usleep 250000
		N=$((N+1))
		[ $N -ge 120 ] && break
	done
	sleep 1
	touch /tmp/.ra_up
}

# retroarch with its config, and the launcher's additions when there are any; --verbose for the
# splash's signal (show_launch_splash) - the log is in RAM
retroarch_run()
{
	if [ -f "$RA_APPEND" ]; then
		"$BIN/retroarch" --verbose --config "$BIN/retroarch.cfg" --appendconfig "$RA_APPEND" "$@"
	else
		"$BIN/retroarch" --verbose --config "$BIN/retroarch.cfg" "$@"
	fi
}

show_return_splash()
{
	[ -x "$ABSPLASH" ] || return
	touch /tmp/.abload
	"$ABSPLASH" "$ABPICS/autobleem.jpg" --until-gone /tmp/.abload --timeout 20 &
}

# what the tree needs before RetroArch starts
prepare()
{
	mkdir -p "$BIOS" "$ROMS" "$BIN/logs" "$BIN/playlists" "$BIN/saves" "$BIN/savestates" \
	         "$BIN/screenshots" "$BIN/config" /tmp/ra_cache
	[ -x "$BIN/retroarch" ] || chmod +x "$BIN/retroarch"
	# the PS1 BIOS from the console itself, for pcsx_rearmed / swanstation
	if [ -f /gaadata/system/bios/romw.bin ]; then
		[ -f "$BIOS/scph5500.bin" ] || cp /gaadata/system/bios/romJP.bin "$BIOS/scph5500.bin"
		[ -f "$BIOS/scph5501.bin" ] || cp /gaadata/system/bios/romw.bin "$BIOS/scph5501.bin"
		[ -f "$BIOS/scph5502.bin" ] || cp /gaadata/system/bios/romw.bin "$BIOS/scph5502.bin"
	fi
	rm -f "$RALOG"
	# every directory retroarch.cfg leaves at "default" (favorites, history, the filters) resolves to
	# $XDG_CONFIG_HOME/retroarch - pointed at RetroArch's own tree, as RetroBoot's XDG_CONFIG_HOME=/media
	# did with its retroarch/ folder
	mkdir -p /tmp/ra-xdg
	ln -sfn "$BIN" /tmp/ra-xdg/retroarch
	export XDG_CONFIG_HOME=/tmp/ra-xdg
	# libraries a core might want beyond the firmware's (the site's libs pack, unpacked by the installer);
	# our RetroArch itself needs none
	if [ -d /media/Autobleem/lib/retroarch ]; then
		export LD_LIBRARY_PATH=/media/Autobleem/lib/retroarch
	fi
}

led 0 1
prepare

if [ -n "$2" ]; then
	case "$2" in
		NEON)  CORE=$BIN/cores/pcsx_rearmed_libretro.so ;;
		PEOPS) CORE=$BIN/cores/swanstation_libretro.so ;;
		*)     CORE=$2 ;;
	esac
	echo "Using core $CORE"
	show_launch_splash &
	retroarch_run -L "$CORE" "$1" > "$RALOG" 2>&1
	LVL=$?
else
	show_launch_splash &
	retroarch_run > "$RALOG" 2>&1
	LVL=$?
fi
echo "retroarch exited with status $LVL"
touch /tmp/.ra_up

if [ $LVL -ne 0 ]; then
	# keep the evidence and say so on the front LED, then back to the launcher
	led 1 0
	ab_persist_logs "retroarch exited with status $LVL: ${1:-its own menu} ${2:+($2)}"
	for i in 1 2 3 4; do
		led 1 0
		usleep 125000
		led 0 0
		usleep 125000
	done
fi

show_return_splash

led 0 1
rm -rf /tmp/ra_cache
sync
usleep 250000
