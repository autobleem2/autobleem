#!/bin/bash

#PCSX launcher for AutoBleem

# Everything this script says goes to System/Logs/launch.log (a header per launch, the arguments as given);
# pcsx-ab's own output to System/Logs/pcsx.log, fresh for every launch, with its exit status at the end -
# so a game that comes straight back to the launcher leaves a trace instead of noise in AB_out/AB_err.
LOGS=/media/System/Logs
mkdir -p "$LOGS"
exec >> "$LOGS/launch.log" 2>&1
echo "=== launch.sh $(date '+%Y-%m-%d %H:%M:%S')"
echo "args: $@"

# copy configuration to it's place
if [[ $5 == *"/gaadata"* ]]; then
  echo "Internal game"
else
  cp "$5/pcsx.cfg" "$1/pcsx.cfg"
fi



# select pcsx to use

echo "Custom PCSX ONLY in EvolutionUI !!!"
cp -f /media/Autobleem/bin/emu/pcsx-ab /tmp/pcsx
[ -f /tmp/pcsx ] && chmod +x /tmp/pcsx


echo AUTOBLEEM: Starting PCSX
echo Cmd $@

rm -rf /tmp/runpcsx
mkdir -p /tmp/runpcsx

cd /tmp/runpcsx
ln -s "$1" /tmp/runpcsx/.pcsx

ln -s /media/System/Bios /tmp/runpcsx/bios
ln -s /media/Autobleem/bin/emu/plugins /tmp/runpcsx/plugins

if [ "$6" == "0" ]
then
  /tmp/pcsx -filter $8 -ratio $7 -lang $3 -region 4 -enter 1 -cdfile "$2" > "$LOGS/pcsx.log" 2>&1
else
  /tmp/pcsx -filter $8 -ratio $7 -lang $3 -region 4 -enter 1 -load $6 -cdfile "$2" > "$LOGS/pcsx.log" 2>&1
fi
rc=$?
echo "pcsx-ab exited with status $rc" | tee -a "$LOGS/pcsx.log"

echo FINISHED

