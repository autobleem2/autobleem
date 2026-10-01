#!/bin/bash

# Kills Sony's own UI (the logo, the stock menu, the app that starts them).
#   killsony.sh now   at once - boot.sh's first line, so the stock logo does not hold the screen
#   killsony.sh       after a second: a moment for Sony's processes to finish forking, or killall misses a late
#                     starter and the stock UI comes up over ours (AutoBleem-NG's a5b57b21)
[ "$1" = now ] || sleep 1
killall -s KILL sonyapp showLogo ui_menu 2>/dev/null
exit 0
