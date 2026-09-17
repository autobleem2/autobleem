#!/bin/sh

# EvolutionUI is the only UI now (docs/refactor-plan.md, 2026-09): AutoBleem::run()'s loop only ever writes
# AB_SELECTION=4 (exit to RetroArch/EmulationStation, from the launcher's R2 system menu) before the process
# actually exits - starting a game and returning from one both loop back into the launcher in-process and
# never reach this script. The stock SonyUI exit (SEL_ORIGINAL/start_sony) and the old ui=classic path
# (SEL_SCAN) are gone with the classic menu; anything other than SEL_RETROARCH falls back to relaunching
# AutoBleem, same as they both used to.

SEL_RETROARCH=4

source ./autobleem_cfg.sh
echo Selection: $AB_SELECTION
echo MipMap: $AB_MIP


function select_pcsx
{
      echo Custom PCSX
      cp -f /media/Autobleem/bin/emu/pcsx-ab /tmp/pcsx
      [ -f /tmp/pcsx ] && chmod +x /tmp/pcsx
}

function start_retroarch
{
    ./retroarch.sh
}

function start_autobleem
{
     mount -o bind /media/Autobleem/bin/autobleem/starter /usr/sony/bin/pcsx
	 ./overmount.sh
     ./link.sh
     ./startsony.sh
}

select_pcsx
# AB_THEME in autobleem_cfg.sh is no longer used here: themes hold only what autobleem-gui draws (theme.json),
# not a copy of the console's data tree, so the stock SonyUI is not re-skinned any more.


if [ $AB_SELECTION -eq $SEL_RETROARCH ]
then
	 start_retroarch
else
    start_autobleem
fi

sync
umount /media
sync
reboot
