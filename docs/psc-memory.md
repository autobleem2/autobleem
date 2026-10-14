# PSC memory policy (AUTOBLEEM-31)

`payload/Autobleem/rc/ab_memory.sh`, sourced by `boot.sh`'s `boot_once` right after the log directory is known, does what
Project Eris does at boot:

1. `echo 1 > /proc/sys/vm/overcommit_memory` (the firmware has 0, heuristic). The N64 and Dreamcast emulators reserve big
   address ranges up front; with 1 GB RAM and no swap the heuristic refuses them (suspected cause of their failures).
2. One zram swap device per CPU (zram0..N-1; devices the kernel does not have yet come from
   `/sys/class/zram-control/hot_add`), `disksize` = MemTotal * 1024 / 2 / CPUs bytes (together half of RAM),
   `mkswap -L zramN`, `swapon -p 100`.

Only `/proc`, `/sys` and `/dev` are written: nothing on the eMMC, `/data` or the stick. `boot_once` runs once per boot
(the loop's restart skips it), and the function is idempotent anyway: a device already listed in `/proc/swaps` is left
alone, a zram device that already has a `disksize` keeps it. One line goes to `boot.log` (`overcommit_memory=1 (was 0),
zram swap on 4 of 4 devices, 125 MiB each (MemTotal ... kB)`). A kernel with neither `/sys/class/zram-control` nor
`/sys/block/zram0` gets the overcommit setting and no swap.

## Facts from the owner's PSC (13.10)

overcommit_memory=0, no swap, 1 GB RAM, 4 CPUs; the kernel has `/sys/class/zram-control` (hot_add) and `/sys/block/zram0`.

## Not verified (no device access when this was written)

- Whether the stock PSC rootfs has `mkswap` and `swapon`. The script uses the tool itself if it is on the PATH, else
  the busybox applet of the name (`busybox mkswap`), else skips the swap and says "0 of N devices" in the log. A
  busybox built without CONFIG_MKSWAP / CONFIG_SWAPON leaves overcommit set and no swap: check `cat /proc/swaps`.
- Standby / suspend is left alone. zram swap is plain RAM, so it should not hold the suspend back; if a wake ever
  shows swapped-out pages being slow, `swapoff` before the standby is the place to add (selection.sh).

## Check on the console

`free -m` (Swap total about 500 MB), `cat /proc/swaps` (four /dev/zramN, priority 100), `cat /proc/sys/vm/overcommit_memory`
(1), `grep memory System/Logs/boot.log` (with logs kept), then an N64 and a Dreamcast game.

Test: `tests/rc/test_ab_memory.cpp` (fake /proc, /sys, /dev; run twice = no second swap; no zram = skipped).
