#!/bin/sh

# What happens after autobleem-gui exits. AutoBleem::run()'s loop only ever writes AB_SELECTION=4 (exit to
# RetroArch/EmulationStation, from the launcher's L2+R2 system menu) before the process actually exits -
# starting a game and returning from one both loop back into the launcher in-process and never reach this
# script. Anything else (a crash, a first boot where nothing was written) is a reboot, which brings
# AutoBleem back up. The stock SonyUI exit that used to be the fallback here - starter mounted over
# /usr/sony/bin/pcsx, USB games linked into /gaadata, ui_menu - went with the classic menu (2026-09).

SEL_RETROARCH=4

# autobleem_cfg.sh is what the GUI writes on its way out (LaunchService::writeSelectionScript). After a
# crash, or a first boot where it never ran, the file is not there - then there is no selection, and the
# unset variable would make the test below a shell error instead of the reboot it means.
AB_SELECTION=0
[ -f ./autobleem_cfg.sh ] && . ./autobleem_cfg.sh
echo Selection: $AB_SELECTION

# the emulator the launch script execs; a fresh copy on tmpfs every boot
echo Custom PCSX
cp -f /media/Autobleem/bin/emu/pcsx-ab /tmp/pcsx
[ -f /tmp/pcsx ] && chmod +x /tmp/pcsx

if [ "$AB_SELECTION" -eq "$SEL_RETROARCH" ]
then
    ./retroarch.sh
fi

sync
umount /media
sync
reboot
