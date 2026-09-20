#!/bin/sh

# RetroArch's own menu, from the launcher's L2+R2 system menu: selection.sh runs this after autobleem-gui
# has exited with AB_SELECTION=4, and AutoBleem is started over when RetroArch is done.

sh /media/Autobleem/rc/launch_rb.sh

# return to AutoBleem
cd /media/Autobleem/
rm -f /tmp/.abload
./start.sh
