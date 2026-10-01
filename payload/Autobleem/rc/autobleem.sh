#!/bin/bash

# Update Database on the fly
# Extract system files to avoid crashing (boot.sh did already, for the boot picture: then nothing is unpacked again)
sh /media/Autobleem/rc/unpack_libs.sh

cd /media/Autobleem/bin/autobleem
./run.sh
# the launcher's status (139 SEGV, 137 KILL, 134 ABRT) for ab_persist_logs: this script's own would be sync's
ab_rc=$?
mkdir -p "${AB_RUNTIME_DIR:-/tmp/autobleem}"
echo $ab_rc > "${AB_RUNTIME_DIR:-/tmp/autobleem}/autobleem_exit"
sync
exit $ab_rc


