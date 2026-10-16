# PLATFORM-24 - PSC device session, 2026-10-15 01:17-01:55 UTC (owner watching)

Build: nightly v2.0.0-alpha1.1-355-g72d468c3 (launcher develop 72d468c3), installed with the launcher's own Update.
Store 1.1.0-20261013-9684f89 loads (24 catalog items, SDK ABI 11). RetroArch v1.22.2-7.

| Step | Result | Evidence |
|---|---|---|
| B1 | PASS | 355-g72d468c3, 15.6 GB free |
| P1 panel | PASS with defect 1 | the panel opened after ~90 s (screen frozen) |
| P2 Remove | PASS | removed exactly program, cores, info, assets, database, cheats, overlays, shaders, downloads, records, Retroarch themes, VERSION; kept config, retroarch.cfg, playlists (9), saves/states (22), thumbnails, ab-states, RetroArch/bios (343), roms (110); free space back to 17.5 GB |
| P3 no Wi-Fi | not run | after the UI rework |
| P4 Install | PASS (defects 2-4) | end ok, 01:21:45-01:31:55 (downloads ~2.5 min, unpack ~7 min, merge-config ~30 s); 166 cores; retroarch.cfg kept + 23 build keys, 6 save-state keys, AB2 theme 489 files; Ford Racing ran through RetroArch (pcsx_rearmed) |
| P5 Stop + resume | PASS | Stop during download-cores ended `stopped` in seconds twice, .part kept (12.9 MB, then ~140 MB), Install again continued with `abfetch --continue`, end ok 01:51:15 |
| P6 Wi-Fi pulled | not run | after the UI rework |
| P7 Remove + BIOS | not run | after the UI rework |
| P8 Polski | PASS | panel, progress, Done box in Polish, nothing cut (owner) |
| (extra) Stop during Remove | worked, but see decision 4 | half state: some folders renamed *.removing; the next Install cleaned them |

## Defects
1. The panel computes RetroArch's size on the UI thread (`RaJobService::sizeOnDisk()`, launcher `evoui_launcher_actions.cpp:1170`): walks every file of the Remove set, ~90 s frozen screen on the PSC stick.
2. `unpack` of a .zip reports `1|1` (no progress inside the archive): the bar stands still for minutes (cheats.zip = tens of thousands of files).
3. `download-assets` reports `0|0` (sizes of the 6 libretro zips unknown): pulsing bar without numbers.
4. The progress panel prints raw byte counts (e.g. `3604512 / 6295495`), not MB.

## Owner decisions (recorded in the keeper)
1. Done = PSC + Pi PASS; the PC stick is fixed at the end.
2. No RetroArch size in the panel: the version + the stick's free space.
3. Two bars: the whole job (step i of n) and the current package (MB or files, with its name).
4. Stop only during Install (keeps the downloaded parts); Remove always runs to the end; a half-done Remove is finished by the next Install/Remove.
5. UI reworked from Kasia's mockups (`ui/`, generator autobleem-design feature/platform24-ra-manager-ui f14f2f8): pre-install notice (10-15 min, keep the console on, coffee), one layout for every phase with movement always visible, sizes in MB, one line per phase on why it exists, time left + speed for downloads, Remove confirm with "will be deleted / stays on the stick", result boxes with one OK.
6. Cheats stay always (only the progress counts files).

## New things (for the owner's list)
- RetroArch's read-only data (assets, cheats, databases, overlays, shaders) as one squashfs image on the stick: install = one sequential write instead of tens of thousands of files.
- Unpack the previous package while the next one downloads; two unpack workers (measure on the stick first).
- `ssh_known_hosts.sh psc --reflashed`: keyscan returned no key although ssh works; keeper `device_check psc` returned n/a for everything while ssh worked.
