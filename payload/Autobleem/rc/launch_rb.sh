#!/bin/bash

# Retroboot launcher for AutoBleem

# what this script and RetroBoot's launch_rfa_rom.sh say goes to System/Logs/launch.log, a header per
# launch (RetroArch's own log is RetroBoot's business, under retroarch/logs)
LOGS=/media/System/Logs
mkdir -p "$LOGS"
exec >> "$LOGS/launch.log" 2>&1
echo "=== launch_rb.sh $(date '+%Y-%m-%d %H:%M:%S')"

#kill sony stuffs and set powermanagement parameters
killall -s KILL showLogo sonyapp ui_menu auto_dimmer pcsx dimmer 
echo 2 > /data/power/disable

echo Image "$1"
echo Core "$2"

sh /media/retroarch/retroboot/bin/launch_rfa_rom.sh "$1" "$2"
echo "launch_rfa_rom.sh exited with status $?"
# /tmp/.abload (RetroBoot's return splash) is removed by the launcher itself, once its window is back
usleep 250000 
