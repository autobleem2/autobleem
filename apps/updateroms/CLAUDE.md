# UpdateRoms - the PC-side ROM scanner

The last step of `docs/retroarch-scanner-plan.md` (2026-09-19): the PlayStation Classic usually has no
network, and its USB stick spends its life in a PC getting games copied on. This program, run from the
stick (`<stick>/UpdateRoms/UpdateRoms.exe`), does there what the launcher's own background scan does on a
Pi - `RetroArchScanner` over `roms/<system>/`, `CoreInfoTable` from the stick's `retroarch/info/`,
identification against `retroarch/database/rdb/`, `OnlineAssets` for the databases bundle and the box
art - **with the PC's network** and **with the target's paths in the playlists** (`usb_root` from the
target's `platform/<target>.ini`: `/media/roms/...` for the console, `/media/autobleem/RetroArch/roms/...`
for a Pi card). The console then boots and finds everything in place; its own scan has nothing left to do.

Two targets, like the console tools, but **no SDL and no AutoBleem rendering** (the owner's call, the same
night it was first written on `ab_classic`): a plain Win32 window, and a statically linked exe that needs
nothing of MSYS2 - the release bundle is **one 540 KB file** (Release, stripped, UPX) where the SDL version
had been 28 MB plus 50 MB of DLLs.

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
- **`updateroms`** (`UpdateRoms.exe`): `main.cpp` (arguments, `--quiet` on the console it was started
  from, the log to `System/Logs/updateroms.log`) and `win32_window.cpp` - one window: a stage line, a
  progress bar (marquee until the scanner reports a count), a list box with every line the job said, and
  a button that is Stop while it runs (the job returns at its next checkpoint) and Close when done; the
  job on a `std::thread` writing a mutex-guarded `State` a 100 ms timer moves into the controls; the
  system's message font; a manifest (`updateroms.rc`) for common controls v6 and DPI awareness. A
  `-mwindows` GUI-subsystem exe, so no console pops up when double-clicked; `--quiet` attaches to the
  parent console (`AttachConsole`) unless stdout is already redirected, which a script's pipe or file is.
  `-static -static-libgcc -static-libstdc++`: `ldd` shows only Windows' own DLLs. Off Windows there is
  the `--quiet` path only (the window is `#ifdef _WIN32`).

Built on the dev hosts only (root `CMakeLists.txt`: not for `arm`/`aarch64`). **`tools/
make_updateroms_bundle.sh`** makes the folder for a stick: a Release build in `build_updateroms/`
(incremental, `--clean` wipes), stripped and UPX-packed into `build_win/UpdateRoms/` next to `README.txt`.
`tools/make_usb.py` stages the Debug exe into `usb/UpdateRoms/` for the dev tree.

Tested on the fake tree with the real network: 146 databases fetched and unpacked, playlists written with
`/media/...` paths and `/media/retroarch/cores/...` core paths, covers fetched, the window and the packed
exe both run. **Not yet run against a real console stick** - the console has never run this build at all.
