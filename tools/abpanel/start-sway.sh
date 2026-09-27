#!/bin/bash
# Starts sway on the PC test machine (R24) - ~/.profile runs it on tty1 only.
# The standby monitor is on a DisplayLink dock (the evdi driver, no render node of its own). What works with
# sway 1.10 (tried 2026-09-27): the evdi card FIRST in WLR_DRM_DEVICES, so the primary renderer is the software
# one (WLR_RENDERER_ALLOW_SOFTWARE) and the Intel card is the secondary; no DRM format modifiers. The other
# ways fail: Intel first crashes sway as soon as it drives the evdi output, and pixman cannot add the second
# card. Only the first evdi card is used (the driver makes four; the empty ones would fail the DRM backend);
# /dev/dri/cardN numbers can change between boots, the by-path names do not. sway starts next to DisplayLink's
# proprietary daemon only with --unsupported-gpu. Without the dock it is a plain one-GPU sway.
set -u
dri=/dev/dri/by-path
intel=$(readlink -f "$dri/pci-0000:00:02.0-card" 2>/dev/null)
evdi=$(readlink -f "$dri/platform-evdi.0-card" 2>/dev/null)
flags=()
if [ -n "$intel" ]; then
    export WLR_DRM_DEVICES="${evdi:+$evdi:}$intel"
fi
if [ -n "$evdi" ]; then
    export WLR_RENDERER_ALLOW_SOFTWARE=1 WLR_DRM_NO_MODIFIERS=1
    flags=(--unsupported-gpu)
fi
mkdir -p ~/.cache
echo "start-sway: WLR_DRM_DEVICES=${WLR_DRM_DEVICES:-} ${flags[*]}" > ~/.cache/sway.log
exec sway "${flags[@]}" >> ~/.cache/sway.log 2>&1
