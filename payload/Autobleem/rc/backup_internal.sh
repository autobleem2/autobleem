#!/bin/bash

# Copy original save data to USB - once: the launcher runs this at every start, and after the first time
# every folder already has its pcsx.cfg, so nothing is written and there is nothing to sync
COPIED=
for STATE in {1..20}
do
  if [ ! -f /media/Games/!SaveStates/$STATE/pcsx.cfg ]; then
    mkdir -p /media/Games/!SaveStates/$STATE
    cp -R /data/AppData/sony/pcsx/$STATE/.pcsx/* /media/Games/!SaveStates/$STATE
    [ ! -f /media/Games/!SaveStates/$STATE/pcsx.cfg ] && cp /gaadata/$STATE/pcsx.cfg /media/Games/!SaveStates/$STATE/pcsx.cfg
    COPIED=1
  fi
done

if [ ! -f /media/System/Databases/internal.db ]; then
  cp /gaadata/databases/regional.db /media/System/Databases/internal.db
  COPIED=1
fi

[ -n "$COPIED" ] && sync
exit 0
