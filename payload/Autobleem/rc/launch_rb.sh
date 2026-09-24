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
# What this script says goes to System/Logs/launch.log, a header per launch; RetroArch's own output
# to RetroArch/bin/logs/retroarch.log (kept as retroarch_crash.log, with dmesg, when it died). A crash
# comes straight back to the launcher - RetroBoot relaunched five times (the blinking red LED for 45 s).

RA=/media/RetroArch
BIN=$RA/bin
BIOS=$RA/bios
ROMS=$RA/roms
LOGS=/media/System/Logs
ABSPLASH=/media/Autobleem/bin/autobleem/absplash
ABPICS=/media/Autobleem/bin/autobleem/splash

mkdir -p "$LOGS"
exec >> "$LOGS/launch.log" 2>&1
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
show_launch_splash()
{
	[ -x "$ABSPLASH" ] || return
	rm -f /tmp/.ra_up
	"$ABSPLASH" "$ABPICS/retroarch.jpg" --until-exists /tmp/.ra_up --timeout 30 &
	N=0
	while ! grep -q "Found display driver" "$BIN/logs/retroarch.log" 2>/dev/null; do
		usleep 250000
		N=$((N+1))
		[ $N -ge 120 ] && break
	done
	sleep 1
	touch /tmp/.ra_up
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
	rm -f "$BIN/logs/retroarch.log"
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
	"$BIN/retroarch" --config "$BIN/retroarch.cfg" -L "$CORE" "$1" > "$BIN/logs/retroarch.log" 2>&1
	LVL=$?
else
	show_launch_splash &
	"$BIN/retroarch" --config "$BIN/retroarch.cfg" > "$BIN/logs/retroarch.log" 2>&1
	LVL=$?
fi
echo "retroarch exited with status $LVL"
touch /tmp/.ra_up

if [ $LVL -ne 0 ]; then
	# keep the evidence and say so on the front LED, then back to the launcher
	led 1 0
	mv -f "$BIN/logs/retroarch.log" "$BIN/logs/retroarch_crash.log"
	printf "\n--End of retroarch.log--\n\nOutput from dmesg:\n\n" >> "$BIN/logs/retroarch_crash.log"
	dmesg >> "$BIN/logs/retroarch_crash.log"
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
