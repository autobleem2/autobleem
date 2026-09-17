#!/usr/bin/env bash
#
# "RetroArch / EmulationStation" from the launcher's L2+R2 system menu.
#
# On the console this hands over to RetroBoot and the console reboots afterwards. Here it just opens
# RetroArch's own menu; when the user quits it, autobleem-session puts the launcher back on screen.
set -uo pipefail

RC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DATA_MOUNT="$(cd "$RC_DIR/../.." && pwd)"
RA_CONFIG="$DATA_MOUNT/retroarch/retroarch.cfg"

if ! command -v retroarch >/dev/null 2>&1; then
    echo "AUTOBLEEM: retroarch is not installed (sudo apt install retroarch)" >&2
    exit 1
fi

echo "AUTOBLEEM: starting RetroArch"
exec retroarch --config "$RA_CONFIG" --menu
