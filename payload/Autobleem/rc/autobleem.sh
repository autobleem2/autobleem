#!/bin/bash

# Update Database on the fly
# Extract system files to avoid crashing
mkdir -p /tmp/lib
cp /media/Autobleem/lib/libs.tar.gz /tmp/lib
cd /tmp/lib
gunzip -c libs.tar.gz | tar xvf -   # busybox tar may lack -z (the new kernel payload's does)

cd /media/Autobleem/bin/autobleem
./run.sh
# the launcher's status (139 SEGV, 137 KILL, 134 ABRT) for ab_persist_logs: this script's own would be sync's
ab_rc=$?
mkdir -p "${AB_RUNTIME_DIR:-/tmp/autobleem}"
echo $ab_rc > "${AB_RUNTIME_DIR:-/tmp/autobleem}/autobleem_exit"
sync
exit $ab_rc


