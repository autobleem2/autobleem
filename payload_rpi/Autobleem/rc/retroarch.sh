#!/usr/bin/env bash
#
# "RetroArch / EmulationStation" from the launcher's L2+R2 system menu.
#
# On the console this hands over to RetroBoot and the console reboots afterwards. Here it just opens
# RetroArch's own menu; when the user quits it, autobleem-session puts the launcher back on screen.
set -uo pipefail

# glibc 2.41 will not dlopen a core marked as needing an executable stack (libretro's buildbot cores are)
# without this; autobleem.service sets it for the whole session, this is for a shell that did not come
# through it
export GLIBC_TUNABLES="${GLIBC_TUNABLES:-glibc.rtld.execstack=2}"

RC_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DATA_MOUNT="$(cd "$RC_DIR/../.." && pwd)"
RA_CONFIG="$DATA_MOUNT/RetroArch/retroarch.cfg"   # RetroArch's standard tree, laid out by install.sh

if ! command -v retroarch >/dev/null 2>&1; then
    echo "AUTOBLEEM: retroarch is not installed (re-run install.sh, or sudo apt install retroarch)" >&2
    exit 1
fi

echo "AUTOBLEEM: starting RetroArch"
exec retroarch --config "$RA_CONFIG" --menu
