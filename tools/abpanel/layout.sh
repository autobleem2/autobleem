#!/bin/bash
# Lays out the standby monitor's workspace 2 as four quadrants (R24), called once from the sway config:
#   overview (abpanel overview: status + load) | the VM, live (virt-viewer)
#   teams    (abpanel teams)                   | reserved (abpanel empty - blank until its use is decided)
# Each window is started (swaymsg exec: sway places a window on the workspace focused when it was exec-ed)
# while its neighbour is focused and split, so sway puts it where it belongs; then the
# shell's workspace 1 gets the focus back.
set -u
abpanel=~/.local/bin/abpanel

wait_for() { # app_id
    for _ in $(seq 100); do
        swaymsg -t get_tree | grep -q "\"app_id\": \"$1\"" && return 0
        sleep 0.1
    done
    echo "layout: $1 did not appear" >&2
}

wait_for work
swaymsg -q 'workspace 2; layout splith'
swaymsg -q "exec foot --app-id abpanel-overview $abpanel overview"
wait_for abpanel-overview
swaymsg -q "exec virt-viewer -c qemu:///system --attach --reconnect --wait pcusb-test"
wait_for virt-viewer
swaymsg -q '[app_id="abpanel-overview"] focus; splitv'
swaymsg -q "exec foot --app-id abpanel-teams $abpanel teams"
wait_for abpanel-teams
swaymsg -q '[app_id="virt-viewer"] focus; splitv'
swaymsg -q "exec foot --app-id abpanel-empty $abpanel empty"
wait_for abpanel-empty
swaymsg -q 'workspace 1'
