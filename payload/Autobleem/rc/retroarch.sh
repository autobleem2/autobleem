#!/bin/sh

# RetroArch's own menu, from the launcher's L2+R2 system menu: selection.sh runs this after autobleem-gui
# has exited with AB_SELECTION=4, and boot.sh's loop starts the launcher over when it returns.

sh /media/Autobleem/rc/launch_rb.sh

# return to AutoBleem
rm -f /tmp/.abload
