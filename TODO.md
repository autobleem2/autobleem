# TODO

What is known to be left, by area. The dated notes in CLAUDE.md say what was done and why; this is the
short list of what was not.

## PlayStation Classic

- **`payload/Docs/`** still holds the 0.9.0 manuals and release notes (WiFi setup, kernel installation,
  Bluetooth pads, the RetroBoot 1.1 readme, the gamepad list). They ship on every stick and describe an
  install that no longer exists. Rewrite for AutoBleem 2 (the installer, the layout, RetroArch's front
  buttons) or drop them.
- **RetroBoot's leftovers on a migrated stick**: `RetroArch/bin/retroboot/` (~600 MB, EmulationStation
  included) and `RetroArch/bin/apps/` (PPSSPP, drastic, hypseus_singe, crispydoom - launched only by
  RetroBoot's own playlist). Nothing of ours uses them; nothing of ours deletes them either. The installer
  could offer to remove them, or the apps could become AutoBleem Apps (`Apps/<name>/app.ini` + `run.sh`).
- **Our own cores build** (`github.com/autobleem/retroarch-psc`, `make cores`): the console runs RetroBoot
  1.2's cores as a pack; building the 81 of `cores/cores.txt` on the 2-core server was guessed at a day and
  never measured.
