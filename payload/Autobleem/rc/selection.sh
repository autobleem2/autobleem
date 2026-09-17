#!/bin/sh

# This is kind of deprecated - the selector is not needed with EvoUI

SEL_ORIGINAL=3
SEL_RETROARCH=4
SEL_AUTOBLEEM=1
SEL_SCAN=2

source ./autobleem_cfg.sh
echo Selection: $AB_SELECTION
echo MipMap: $AB_MIP


function select_pcsx
{
      echo Custom PCSX
      cp -f /media/Autobleem/bin/emu/pcsx-ab /tmp/pcsx
      [ -f /tmp/pcsx ] && chmod +x /tmp/pcsx
}

function start_sony
{

   mount -o remount,rw /data
   mount -o bind /tmp/pcsx /usr/sony/bin/pcsx
   ./startsony.sh
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


if [ $AB_SELECTION -eq $SEL_ORIGINAL ]
then
    start_sony
fi

if [ $AB_SELECTION -eq $SEL_RETROARCH ]
then
	 start_retroarch
fi

if [ $AB_SELECTION -eq $SEL_AUTOBLEEM ]
then
    start_autobleem
fi

if [ $AB_SELECTION -eq $SEL_SCAN ]
then
    start_autobleem
fi

sync
umount /media
sync
reboot
