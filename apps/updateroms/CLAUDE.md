# UpdateRoms - the PC-side ROM scanner

The last step of `docs/retroarch-scanner-plan.md` (2026-09-19): the PlayStation Classic usually has no
network, and its USB stick spends its life in a PC getting games copied on. This program, run from the
stick (`<stick>/UpdateRoms/UpdateRoms.exe`), does there what the launcher's own background scan does on a
Pi - `RetroArchScanner` over `roms/<system>/`, `CoreInfoTable` from the stick's `retroarch/info/`,
identification against `retroarch/database/rdb/`, `OnlineAssets` for the databases bundle and the box
art - **with the PC's network** and **with the target's paths in the playlists** (`usb_root` from the
target's `platform/<target>.ini`: `/media/roms/...` for the console, `/media/autobleem/RetroArch/roms/...`
for a Pi card). The console then boots and finds everything in place; its own scan has nothing left to do.

Two targets, like the console tools:

- **`updateroms_core`** (`src/core/update_roms_job.*`, links `ab_core`, tested from
  `tests/apps/test_updateroms_core.cpp`): `UpdateRomsJob::rootFromProgramPath()` (the first parent folder
  with `Autobleem/bin/autobleem`), `detect(root, setup, error, target)` - which platform the stick is for
  (**not** by the RetroArch folder's name: a stick in a PC is exFAT, where the Pi's `RetroArch/` and
  RetroBoot's `retroarch/` are one folder; RetroBoot's own folder marks the console, the Pi installer's
  `retroarch.cfg` marks the Pi, `--target` overrides), the target ini applied over `EnvironmentSetup::
  fromRoot()` so the engine's paths are the stick's, the PC's `pc.ini` for `download_command` - and
  `run(setup, listener, shouldStop, say, runner)`: probe, databases bundle when there are none, the scan
  with every system's core path mapped onto the target's RetroArch dir, box art for what lacks one. In
  `RetroArchScanner::merge` a kept entry that names *this* machine's ROM folder (a launcher scan run on
  the PC) is made to name the target's, which is what makes a stick scanned here and there consistent.
- **`updateroms`** (`UpdateRoms.exe`): `main.cpp` (arguments, `--quiet` on stdout, the log to
  `System/Logs/updateroms.log`), `UpdateRomsApp : AppBase` (the stick's config.ini, theme and language;
  `loadAssets(false)` - the theme's look, never its music, on the owner's request), `GuiUpdateRoms` - the
  one screen: the job on a thread reporting through `ScanProgressListener` and a line sink under a mutex,
  the main loop drawing title / stage line / progress bar / the last log lines / `Close` (or `Stop`, which
  asks the job to return at its next checkpoint). Escape is `Stop`/`Close` here
  (`Input::setPowerKeyAsKey(true)`), never the power switch.

Built on the dev hosts only (root `CMakeLists.txt`: not for `arm`/`aarch64`); `make_win.sh` builds and
validates its `resources/lang/`. **`tools/make_updateroms_bundle.sh`** makes the folder for a stick:
`build_win/UpdateRoms/` with the exe, its `lang/` and `README.txt`, and the MSYS2 DLLs `ldd` names - SDL2
and its image/mixer/ttf libraries with every codec they were built with, ~50 MB, which is the known wart:
a `-static` build that leaves the codecs out (the tool needs PNG and TrueType and no sound at all) is the
follow-up. The exe is console-subsystem so `--quiet` can print; a console window comes up next to the
SDL one on Windows. `tools/make_usb.py` stages the exe into `usb/UpdateRoms/` for the dev tree (DLLs from
PATH there, as for the launcher).

Tested on the fake tree with the real network: 146 databases fetched and unpacked, playlists written with
`/media/...` paths and `/media/retroarch/cores/...` core paths, covers fetched. **Not yet run against a
real console stick** - the console has never run this build at all.
