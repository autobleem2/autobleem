#!/bin/bash

# the launcher's stdout/stderr go where this run's logs go - RAM (/tmp/autobleem/logs) unless the logs are
# kept on the stick (rc/ab_log.sh, docs/quiet-stick-plan.md)
. /media/Autobleem/rc/ab_log.sh
LD_LIBRARY_PATH=/tmp/lib ./autobleem-gui /media > "$AB_LOG_DIR/AB_out.txt" 2> "$AB_LOG_DIR/AB_err.txt"
