#!/usr/bin/env bash
# a moment for Sony's processes to finish forking, or killall misses a late starter and the stock UI
# comes up over ours (AutoBleem-NG's a5b57b21)
sleep 1
killall -s KILL sonyapp showLogo ui_menu
