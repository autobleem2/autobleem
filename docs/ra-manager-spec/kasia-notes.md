# RetroArch manager dialogs: Kasia's notes for the developer (AUTOBLEEM-39 part 2)

Source: Kasia's design check of ra-1a..4c + crt-2a against `psc-session-2026-10-15.md` (22:50Z, 15.10.2026): all screens match owner
decisions 1-6; no screen counts files or walks the stick when the panel opens. Verbatim notes for the developer:

- a. 2c Remove "N of 7 400 files": the total must not come from a walk at Remove start (that is the same cost as defect 1). Take it
  from a file list written at install time (per package), or count while deleting and show "N files" plus the package bar by packages
  done. Either way: never on the UI thread.
- b. 2a download-assets in MB (defect 3): the 6 libretro zips need their sizes up front: a HEAD/Content-Length per zip before the
  step, or sizes in the catalog. 2b/2d totals: zip = central-directory entry count (cheap), tar.gz = compressed bytes read (MB).
- c. Numbers in the mockups are placeholders: 1a Update shows "about 640 MB", 1c "about 400 MB". The code shows real sizes from the
  catalog/manifest; the 1c text keeps "about 400 MB / 10-15 min" (the measured P4 values).

Her decision mapping, also for the brief:
- decision 2: no RA size: 1a shows version + free space via statvfs + bar; Remove has no size.
- decision 3: two bars on every 2* screen: the whole job "Step i of 9 - phase" + the current package in MB or files with its name.
- decision 4: Stop only on Install screens 2a/2b/2d; 2c Remove has no Stop.
- decision 5: 1c pre-install notice 10-15 min, ~400 MB, console on, coffee; one layout for every phase; one "why" line per phase;
  time left + speed on downloads; 3a/3b deleted/stays; 4a-c one OK each.
- decision 6: cheats always, no skip; 2b counts cheats in files.

Mockups: `ui/` (ra-1a..ra-4c, -pl, crt-2a, sheet.png). Generator: `make_ra_manager_ui.py` (autobleem-design branch
feature/platform24-ra-manager-ui, f14f2f8; copied here because laptop workers cannot clone that repo). Owner decisions:
`psc-session-2026-10-15.md`.
