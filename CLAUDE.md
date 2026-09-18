# AutoBleem — developer context

AutoBleem is a game launcher / front-end for the **PlayStation Classic (PSC)**. It replaces the stock SonyUI,
scans a USB stick for PS1 games, keeps metadata + cover art in SQLite, and launches games in PCSX (bundled
`pcsx-ab`) or RetroArch/RetroBoot. Author: screemer (the repo owner). Snapshot version: v0.9.1 (`src/resources/config.ini`).

This file is the primary documentation for the codebase — the source itself is sparsely commented.

## Current work (2026-09)

Refactor for stability, then add features. Done on 2026-09-15 (one commit per step, see `git log`):

- Phase 0: Windows/MinGW dev build (`make_win.sh`), `AB_DEBUG_HOST` macro, keyboard-as-gamepad, `tools/win_drive.ps1`.
- Phase 1: no uncaught exceptions (playlist JSON, `stoi`, `popen`), `Util::runAndWait` for every fork/exec,
  SDL subsystem lifecycle (`TTF`/`Mix` init once, `SDL_Quit` via `atexit`), unit-buffered logs.
- Phase 2: `database.cpp` uses a local RAII `Stmt` wrapper (no leaked statements, NULL-safe columns, rollback).
- Phase 3: no raw owning `new`/`delete` left: stack objects for short-lived helpers and screens, `unique_ptr`
  for DBs, interceptors, `GuiLauncher` elements, pads, memory-card editors.
- Phase 4: `DirEntry::checkWritable` on every writer, `-Wall -Wextra` clean debug builds.
- lib_ableem: every SDL/SDL_image/SDL_mixer/SDL_ttf/SDL_FontCache call moved out of the app into a new static
  library, `lib_ableem/` (namespace `ableem`). The app links `ableem` and includes no SDL header anywhere;
  `grep -rl "SDL2/" src/code` returns nothing. See the "lib_ableem" section below.
- lib_ableem/engine: the SDL-free half of the app (filesystem, strings, ini/cfg files, the SQLite game
  database, cover dbs, disc image inspection, the scanner, RetroArch playlists, vendored sqlite/json/unecm)
  moved into a second target, `ableem_engine`, under `lib_ableem/include/ableem/engine/`. Every path the
  engine uses comes from `ableem::Environment` (set once in `main.cpp`); the engine has no idea what a Gui
  is (scan progress goes through `ScanProgressListener`). Verified byte-identical scan output before/after.

Done on 2026-09-16:

- The `Gui` god object is being split into a model (`App`) and a screen (`Gui`). `App` (`src/code/app.*`) was
  `main.cpp`'s loose free functions and globals; it owns the `Session` (`core/model/session.h`), the game
  library, the scanner, and now everything on the old `Gui` that was not graphics: `Config` (config.ini),
  `Theme` (the merged, resolved theme.json) and `AppAudio` (music + the five UI sounds).
  Screens reach them as `app.config()`, `app.theme()`, `app.audio()` through the `app` member of `GuiScreen`;
  the handful of non-screens (`Fonts`, the launcher's metadata panel) use `App::get()`.
  `Gui` is left with the window/renderer, `assets()`, `text()` and the background/logo/status drawing
  (phase C, 2026-09-16).

The next structural step is planned in `docs/refactor-plan.md`: split `src/code` into `ab_core` (model +
services, no SDL, unit tested with doctest), `ab_ui` and `ab_evoui`, moving the game queries, settings,
memcard/savestate and launch logic out of the screens that currently hold them. The list below is folded into
that plan's phases. Phase A is done apart from step 3 (the ARM build), which is deferred - no toolchain on
this host yet:

- **Step 1** - the `#define`-int selections are `enum class`es now: `GameSet` (+ `nextGameSet` for the Select
  wraparound) and `Ps1SelectState` in `session.h`, `LauncherScreenState` in `gui_launcher.h`, and the
  `LauncherMenuOption` in `gui_launcher.h`. `PsMenu::selOption` stays an `int` - it is a
  generic index into the icon row that `PsMenu` animates by `++`/`--` - and is compared through
  `selOptionIs()`. `EmuMode` and `MenuOption` were already enums.
- **Step 2** - `ab_core` exists (`src/code/core/`, links `ableem_engine` only, `starter` links just it).
  It held `main.h`, `environment.*`, `util.*`, `lang.*`, `DebugTimer.*`, `services/config.*` and
  `model/timing.h` at first; on 2026-09-16 the top level was cleared down to `main.h`. It is deliberately small: only files with no `Gui` and no `App::get()` could move without
  a content change.
- **Step 4** - the test harness (step 3, the ARM build, is deferred - no toolchain on this host). doctest
  2.4.11 vendored at `tests/third_party/doctest/doctest.h`, `tests/support/{env_fixture.h,temp_dir.*}`,
  ctest wiring behind `AB_BUILD_TESTS` (ON for hosts, forced OFF by the cross toolchain files), and the first
  suites: `tests/core/test_config.cpp` and `tests/core/test_env_fixture.cpp`. `make_win.sh` runs `ctest`
  after every build. `Theme` was to be tested here too, but it is not in `ab_core` yet.
- **Step 5** - `core/model/game_set.h` holds `GameSet`, `Ps1SelectState` and `GameSetSelection` (tested in
  `tests/core/test_game_set.cpp`). `Session::LauncherState` is gone: `Session::launcher` is a
  `GameSetSelection`, and `GuiLauncher`'s six `current*` mirror fields are one `selection` member that
  `loadAssets()` seeds and `GuiLauncher::rememberSelection()` writes back. Careful: the PS1 sub-set is
  deliberately **not** carried across while another set is showing - see the comment on
  `rememberSelection()`; it is a pre-existing quirk, flagged in the plan, not a fix waiting to happen.

Phase B has started. `GameQueryService` (`core/services/game_query.*`) owns every "which games does this
set show" question: `GuiLauncher::switchSet()` now calls `app.gameQuery().gamesFor(selection)` and does
carousel work only. RetroArch is reached through the `RetroArchGames` interface that `RetroArchService`
implements (and a test stub can). `PsGame` moved to `core/model/` to make that
possible - `setMemCard` is split into `PsGame::setMemCardInGameIni()` (Game.ini, in core) plus an explicit
`library().usbGames().updateMemcard()` at the two interceptor call sites, until step 8 reunites them.

`GameCatalogService` (`core/services/game_catalog.*`) is the write side: `recordGamePlayed()` (the history
is a 1..100 ranking, renumbered across USB *and* internal games on every launch, in one transaction),
`deleteUsbGame()` / `removeSaveStateFolder()` (a `!SaveStates` folder can be shared, so the caller confirms
before the second call) and `flushAllCovers()`. Favorite toggling is still in `GuiEditor` - it is one of six
identical `gameIni` blocks there and moves with the rest at step 9.

`MemcardService` (`core/services/memcard.*`) owns the `!MemCards` sets and which one a game plays with:
`activeCardName()`, `setCardForGame()` (Game.ini + regional.db together again), `swapInForLaunch()` /
`swapOutAfterLaunch()` - the halves both interceptors used to duplicate - and the create/list/rename/remove
the memory-card screens use. Nothing outside it constructs an `ableem::MemcardManager`.

`ResumePointService` (`core/services/resume_point.*`) owns the save-state slots under a game's
`!SaveStates` folder - `slotIsActive`/`pictureForSlot`/`lastPicture`/`storePictureForSlot`/`removeSlot`/
`exitedCleanly`, plus `prepareForLaunch`/`saveAfterLaunch` that the PCSX interceptor used to hold. Its
header documents the file layout. Two naming quirks callers depend on: slot 0's picture has no number in
its name, and `lastPicture()` uses slot 0's picture name whichever slot it finds.

`GameSettingsService` (`core/services/game_settings.*`) is what the game editor edits: `open(game)` gives a
`GameSettings` (the game's Game.ini as an `IniFile` - filled in from the record for an internal game, which
has no file - plus the nine `pcsx.cfg` values), and one setter per editor option writes it back with the
encoding PCSX expects (0/1 flags decimal, levels hex, every `!SaveStates` copy via `ConfigFileEditor`).
The favorite/play-using-RA toggles live here (Game.ini for USB, internal.db for internal). `GuiEditor` is
now only the screen: callers set `gameData` and `show()`.

`LaunchService` (`core/services/launch.*`) is a game launch start to finish - what `App::launchGame` and
the three `EmuInterceptor`s (PCSX, RetroArch, Apps) did between them: `writeSelectionScript()`, then
`launch(game, mode, resumePoint)` picks the path from the game and the mode, swaps the memory cards in,
prepares the resume point, builds the argv for `rc/launch.sh` / `rc/launch_rb.sh` (or an App's own
`startup`), runs it through a `ProcessRunner`, and swaps the cards back out. `ProcessRunner`
(`core/services/process_runner.h`) is the one interface introduced purely for testability: `ForkProcessRunner`
is `System::runAndWait`, the dev host installs a `SplashProcessRunner` from `App` (which is where the old
`#ifdef AB_DEBUG_HOST` in each interceptor went), and the tests pass a recording fake. The launcher script
and RetroArch paths come from `Env` now (`getPathToRCDir()`, `getPathToRetroarchDir()`), not literals.
`session.h` moved to `core/model/` with it.

**`PsGame` is now a data record** - `ableem::GameRecord` plus the launcher-only fields and
`fromRecords()`, and nothing else. No filesystem, no `App`, no `Gui`.

**`ab_ui` and `ab_evoui` exist** (2026-09-16, after phase C step 14 broke the `menuSelection()` ->
`GuiLauncher` cycle). `App` is the model at the top of `ab_ui` - every screen's `app` member - and takes its
`ProcessRunner` from whoever constructs it; `AutoBleem : App` (`src/code/autobleem.*`, in the executable) adds
`run()`, the runner choice (fork on the console, splash on a dev host) and owns the outer loop.
`ClassicMenuScreen` is in the executable too, being the one screen that shows both the classic sub-screens and
the launcher. The linker now enforces: core knows no SDL, ab_ui knows no launcher, ab_evoui knows no `main`.

Nothing core-shaped is left outside `core/` (2026-09-17): `Theme` and `Clock` (was `UtilTime`) are core services
taking `Config&`; the `Scanner` singleton is gone - `AutoBleem` makes a `GameScanner` with a `SplashScanProgress`
listener (`gui/scan_progress.*`); `AppAudio` is `gui/app_audio.*` with `Config&`/`Theme&`; and `CardEdit` split into
`ableem::MemcardImage` (the engine, tested) plus a texture wrapper in `evoui/card_edit.*`. `src/code/engine/` no
longer exists.

**Phase C has started.** `TextRenderer` (`gui/text_renderer.*`, step 12) is the text half of the old `Gui`:
the `|@X|` token layout, `renderText*`/`renderSelectionBox`/`renderLabelBox`, the opscreen/text rects and
the `getR/G/B` colour parsing. Screens reach it as `gui->text()`. `ThemeAssets` (`gui/theme_assets.*`,
step 13) is the other half: the theme's font and font sets, the background/logo/jewel textures and the
button-marker textures, with `load()` reading them for whatever theme config.ini names; screens reach it as
`gui->assets()`. `Gui` keeps `loadAssets()` (assets + the theme's music) and the background/logo/status
drawing that combines assets and text. `ClassicMenuScreen` (`gui/screens/gui_classic_menu.*`, step 14) is the old
`Gui::menuSelection()` as a screen: `App::run()` shows it, it sets `session().menuOption` and closes, or shows
a sub-screen and restarts itself where the old code recursed. `gui.cpp` is 182 lines.

The three bugs the phase B extractions pinned were each fixed in their own commit on 2026-09-16: the
memcard fallback guard, `ConfigFileEditor`'s prefix matching (a key now has to be followed by whitespace or
`=`), and RetroArch launches not recording last_played (they do, for library games only).

**Phase B is complete.** `RetroArchService` (`core/services/retroarch.*`, step 11) is the old `RAIntegrator`
singleton as an `App`-owned service: reads `retroarch/info/*.info` and `retroarch/playlists/*.lpl` on first
use, resolves each entry's core (its own if installed, else `resources/coreOverride.cfg`, else the first
`.info` listing the playlist's database), keeps Favorites/History after the platforms and refreshes them
after a RetroArch run (`reloadFavoritesAndHistory()`). `escapeName()` is the boxart file name rule.

Still to do, in order:

1. Continue `docs/refactor-plan.md` - phase B, the service extractions (`GameQueryService` first). Each one
   ships with its tests in the same commit, and pulls its file into `ab_core` as it goes.
2. Centralize the hard-coded paths still in the app (`/media/System/Logs/ver.txt`) in `Env` - the engine side
   is done, so are the theme loaders (`Theme` asks `Env` for both branches now), the launch scripts and
   RetroArch paths (`LaunchService`, step 10) and `backup_internal.sh` (`Env::getPathToRCDir()`, done with
   the Pi port). `config.ini`'s `Cfg=` key is still an absolute console path - the Pi installer rewrites it
   per install, since where it points depends on where the data partition was mounted.
3. ~~Split `GuiLauncher`~~ - done (phase D, 2026-09-16). **The refactor plan is complete.**
4. ~~Set up the Sony ARM toolchain~~ - done 2026-09-17: `make_psc.sh` builds on the remote server (see Build).
   `autobleem-gui` and `starter` cross-compile and link cleanly with the Sony GCC 8.2 toolchain. **Still to
   do: run it on a console** - nothing has run on real hardware yet; the Windows/MinGW build is the only one
   that has been executed. The Pi toolchain (below) is a different target and does not substitute for it.
5. Features. Done on 2026-09-17: **themes are `theme.json`** (`docs/theme-format.md`). `ableem::ThemeSpec` is
   the typed theme (engine, JSON in/out, partial-over-default merge, per-file fallback), `ThemeConverter`
   (`core/services/theme_converter.*`) turns an old `theme.ini` + PSC-data-tree folder into the new layout in
   place - `Theme::load()` does it on first contact, `tools/theme_convert` ahead of time - and `payload/themes`
   ships converted (aergb 334 -> 29 files). The stock SonyUI is no longer re-skinned (`rc/selection.sh`), and
   `src/resources/sony/` is just the two SST fonts. Screens read `app.theme().classic()/launcher()/sounds()`.
   A theme can also be dropped in as `<name>.zip`: `ThemeInstaller` (`core/services/theme_installer.*`)
   unpacks it to `<name>/` at `Theme::load()` / the Options theme list, over `ableem::ZipArchive` (vendored
   miniz, read-only, `lib_ableem/third_party/miniz/`).
6. Straight into EvolutionUI, with the scan in the background. Done on 2026-09-17 (plan at
   `C:\Users\Artur Jakubowicz\.claude\plans\lets-plan-some-feature-synchronous-feather.md` if that path is
   still around; otherwise this entry and the source map are the record). Four steps:
   - **Step 1** - `GamesFingerprint` (`lib_ableem/engine/games_fingerprint.*`): a snapshot of the games
     directory keyed by path + file size, deliberately **no mtime** - the PSC has no battery-backed clock,
     so a stored modification time cannot be trusted to stay put across a reboot (see
     `DirEntry::fileSize()`'s comment). `ScanProgressListener` gained `done`/`total` on `onScanProgress` and
     two new per-game callbacks, `onGameVerified`/`onGameFailedVerify`. `GameScanner::writeRegionalDatabase`
     split into `writeSubDirRows`/`writeAutobleemList`, both keyed by a caller-supplied id-by-path map - the
     class no longer assigns game ids itself. `GameDatabase` gained `loadGamePaths`/`findGameIdByPath`/
     `maxGameId`/`updateGame`/`replaceDiscs`/`clearSubDirTables` for that. **Watch the trailing separator**:
     the `PATH` column always carries one (`insertGame`'s `fullPath + sep`, a no-op if already there), a
     `UsbGame::fullPath` never does - every lookup by path needs it added back (or stripped, for the
     id-by-path maps `writeSubDirRows`/`writeAutobleemList` probe with bare `fullPath`). Missing this the
     first time round made every rescan treat known games as new (duplicate rows); `tests/core/
     test_scan_service.cpp`'s real-scan integration test is what caught it - the DB-level unit tests didn't,
     because their fixture never went through `+ sep` at all.
   - **Step 2** - `ScanService` (`core/services/scan_service.*`, `App::scans()`): one `std::thread` at the
     OS's lowest scheduling priority (`System::lowerCurrentThreadPriority()` - `SCHED_IDLE` on Linux,
     `THREAD_PRIORITY_IDLE` on Windows, so a scan never takes CPU from a running emulator) does every bit of
     filesystem work with its own `CoverDatabase` connection and queues `WorkerEvent`s; `poll()`, called
     from the main thread, applies every regional.db write and returns a `ScanUpdate`. `requestScan()`/
     `scanning()`/`setWatching()`; `checkForChanges()` is the watcher's debounce (two identical fingerprints
     in a row, `ScanWatchInterval` = 10s apart, `core/model/timing.h`), checked every `threadMain()` cycle
     when nothing was requested directly.
   - **Step 3** - `ClassicMenuScreen`/`gui/scan_progress.*` are deleted; `AutoBleem::run()` goes splash ->
     `GuiLauncher` directly and loops there (`Session::MenuOption` keeps only `IDLE`/`RETRO`/`START`, at
     their old numeric values - `rc/selection.sh` trimmed to match, `start_autobleem` the fallback for
     anything but `SEL_RETROARCH`). No more `ui=classic`/EvolutionUI choice (`Config` drops a stale `ui` key
     on load). Circle in the launcher's `Games` state is a no-op now - there is nothing left to fall back
     to. `GuiLauncher::loop()` polls the scan once a frame; a new bottom-of-screen line
     (`scanStatusLine`) shows its progress or a "Scan complete" summary, and any roster change reruns the
     current set's query and re-selects the same game by id (`reloadGames()`) rather than splicing the
     carousel - simpler, and it is the one place duplicates-across-folders and sub-dir rows already get
     settled right. Per request: `GuiSplash` now holds at full brightness for `SplashHoldDuration` (2s) and
     fades back out before returning instead of cutting away; `GuiLauncher` fades in from black over
     `LauncherFadeInDuration` (300ms) every time it is shown.
   - **Step 4** - `GuiSystemMenu` (`evoui/screens/evoui_system_menu.*`): the L2+R2 overlay with everything the
     classic menu offered - Re-Scan, RetroArch/EmulationStation, Memory Cards, Game Manager (refuses itself
     while `scanning()` - it deletes folders the scanner may be reading), Hardware Information, Options,
     About, Power Off. A dumb picker (translucent panel, launcher fonts/colours); `GuiLauncher::
     loop_openSystemMenu()` reads its `SystemMenuAction` back and runs it. Originally bound to a bare R2,
     moved onto L2+R2 (2026-09-17, on request): every other button was already committed to something in at
     least one launcher state, and L2+R2 used to power off the console directly - reaching for a bare R2
     with L2 still down from an L2+Select folder/playlist switch risked shutting down by mistake. L2+R2 now
     opens this menu instead, Power Off is one of its items (behind its own confirm), and bare R2 does
     nothing.

CHD support no longer depends on an external install: **`libmamecd` is vendored** (2026-09-17) under
`lib_ableem/third_party/libmamecd/` from the sources dropped into `!refactor/libmamecd-master/` - see the
"libmamecd" bullet under Build for what moved, the two upstream-bug fixes that were needed for a modern
mingw-w64/UCRT toolchain, and what got trimmed (VS project files, autotools cruft, docs, unused
encoder/grabbag headers). `AB_ENABLE_CHD` now defaults ON on every host, `make_win.sh` no longer forces it
OFF, and a full Windows/MinGW rebuild plus `ctest` were verified green with it vendored in.

## Raspberry Pi port (2026-09-17)

A second, *non-PSC* target: AutoBleem as an appliance on **32-bit Raspberry Pi OS Lite** (Bookworm or
Trixie), games on an exFAT partition of the SD card that behaves like the console's USB stick. **Running on
hardware since 2026-09-18**: a Pi 400 (BCM2711, same as a Pi 4) with 32-bit Trixie at 192.168.68.144 - the
installer shrank its root to 16 GB, built RetroArch 1.22.2 from source, downloaded 109 cores; the launcher
boots into the carousel with sound over HDMI and starts games in pcsx-ab. What that first session fixed, in
order (each its own commit): the `init=` shrink that could never work (now an initramfs `local-premount`
script), a `YES` swallowed by apt, `cp -a` failing on exFAT, an empty unit file after a power cut (atomic
writes now), ALSA defaulting to the DualShock's USB audio (`autobleem-session` writes `/etc/asound.conf` for
the connected HDMI), the splash gone before the TV synced (`SplashSettleDuration`), **no game launching on
any platform** since the classic menu's removal (`startingGame` -> `MENU_OPTION_START` lives in
`AutoBleem::run()` now), the launcher's window being the DRM master (`Gui::releaseDisplay()` around a
launch - see "Conventions"), and pcsx-ab's `fclose(NULL)` in its console-only cpu-temperature watcher.
Things to know when working on the Pi over ssh: `plink -pw` from `C:\Program Files\PuTTY` (the harness
will not install ssh keys), `sudo -S` with the password on stdin, the journal is not persistent, and the
launcher's logs are `System/Logs/AB_out.txt`/`AB_err.txt` on the partition. PS1 games run in **pcsx-ab** as on the console: the Pi build from `E:\Programming\pcsx-rearmed-develop`
(`AUTOBLEEM_DIR=../autobleem-develop ./make_rpi.sh` copies its `build_rpi/dist/` into
`payload_rpi/Autobleem/bin/emu/`, which is checked in like the console's `payload/Autobleem/bin/emu/`), and the
Pi `rc/launch.sh` builds `/tmp/runpcsx` exactly as the console's does (`.pcsx` -> the `!SaveStates` folder,
`bios` -> `System/Bios`, `plugins` -> `emu/plugins`, `-region 4`, `-load 1` on resume). The BIOS is the user's:
`System/Bios/romw.bin` + `romJP.bin` (pcsx.cfg's `Bios = SET_BY_PCSX` picks one by serial; HLE without them).
RetroArch's `pcsx_rearmed` core is only the fallback if the package shipped without pcsx-ab.

**RetroArch on the Pi** lives in `RetroArch/` on the data partition, in RetroArch's own standard tree (`cores`,
`info`, `system` = the cores' BIOS files, `roms` = the user's other-system games, `saves`, `states`,
`playlists`, `config`, `assets`, `autoconfig`, `database`, ...) with a generated `retroarch.cfg` whose every
directory key points in there; the Pi's `rc/launch_rb.sh`/`retroarch.sh` run `retroarch --config` on it, and
`Env::getPathToRetroarchDir()` is that folder on a Pi (`ableem::Environment::setRetroarchDir()`, set in
`setupEnvironment()` under `AB_PLATFORM_RPI`, tested in `tests/core/test_env_fixture.cpp`; the console keeps
`retroarch/`). `install.sh` builds the **latest tagged RetroArch from GitHub on the Pi** (KMS/EGL/GLES, udev,
ALSA; no X/Wayland/Qt/ffmpeg) because libretro's buildbot has every armhf *core* but no armhf *frontend*;
`--retroarch apt` uses the distribution's package (1.19 on Trixie), `--retroarch none` leaves it alone. Then it
downloads all ~130 cores from `buildbot.libretro.com/nightly/linux/armhf/latest/.index-extended` and the
`info`/`assets`/`autoconfig`/`database-rdb`/`database-cursors`/`cheats`/`overlays`/`shaders_glsl` bundles from
`assets/frontend/` into that tree (`--no-downloads` skips). Cores are `dlopen`ed off the exFAT partition, which
works because the fstab entry has no `noexec`. Trixie renamed packages for its 64-bit `time_t` transition
(`libpng16-16t64`, `libegl-dev`/`libgles-dev`); `pkg_first_available` in `install.sh` tries each name.

- **Toolchain**: `toolchains/rpi/RPitoolchain.cmake` over the Windows-hosted "SysGCC for Raspberry Pi"
  (`C:\sysGCC\raspberry`, gcc 14.2.0, `arm-linux-gnueabihf`, sysroot rsynced from a real Pi). `./make_rpi.sh`
  configures and builds into `build_rpi/`. Target is `armv7-a + neon-vfpv4`, so Pi 2/3/4/Zero 2 - **not**
  armv6 (Pi 1/Zero). Invoke it the way `make_win.sh` is invoked, from the MSYS2 UCRT64 shell, but keep
  `C:\sysGCC\raspberry\bin` *off* PATH: its `rm`/`mkdir`/`make` shadow the MSYS2 ones and break the script.
  The compilers are named by absolute path in the toolchain file, so they do not need to be on PATH.
- **SDL2 discovery**: that sysroot has the SDL2/image/mixer/ttf runtime `.so`s but no `-dev` package - no
  headers, no unversioned symlinks, no cmake config. `toolchains/rpi/cmake/FindSDL2.cmake` defines all four
  imported targets itself (lib_ableem does one `find_package(SDL2)` and then links four bare names), with
  headers from `toolchains/rpi/sdl2-devkit/include` (copied from the MSYS2 SDL2 package - the public headers
  are arch-independent, and 2.32.10 vs the Pi's 2.32.4 is ABI-safe) and `IMPORTED_LOCATION` pointed straight
  at the versioned `.so`, which is what makes the missing symlinks irrelevant.
- **`AB_PLATFORM_RPI`** (`core/services/environment.h`), set by the CMake option `AB_TARGET_RPI` which the
  toolchain file forces on. A Pi is a *real* target, not an `AB_DEBUG_HOST`: it forks emulators and halts for
  real. What it changes: `GameQueryService::showInternalGames()` is hard `false` and the "Show Internal Games"
  row is gone from the Options menu (a Pi has no `/gaadata`); `backup_internal.sh` is not run. The new
  `AB_ROOT_RELATIVE_LAYOUT` (debug host **or** Pi) is what now selects `setupEnvironment`'s "everything under
  the root given on the command line" branch, which the Pi shares with the 1-arg debug mode.
  `internal.db` is still *opened* on a Pi (it comes up empty) because `GameCatalogService`/
  `GameSettingsService` unconditionally expect the handle.
- Root `CMakeLists.txt`'s `^arm` branch is PSC-specific (it overwrites `CMAKE_CXX_FLAGS` with
  `-march=armv8-a+simd` and adds `/opt/toolchain/armv8-sony-...` to the include path), so `AB_TARGET_RPI` now
  takes its own branch there. Without that the Pi build was getting armv8 code, which would SIGILL on a Pi 2.
- **Package**: `payload_rpi/` is a sibling of `payload/`, not inside it, and is laid out as the package the
  Pi unpacks: `install.sh` + `README.md` at the top, `system/` for the host-side files (systemd unit, the
  `autobleem-session.sh` loop that replaces `rc/selection.sh`, the two shrink-root initramfs pieces), and the data-partition
  tree exactly as it lands on the exFAT partition - `Autobleem/rc/` (the Pi `launch.sh`/`launch_rb.sh`/
  `retroarch.sh`), `Autobleem/bin/emu/`, `Games/`, `Apps/` (empty dirs kept by `placeholder` files).
  `tools/make_rpi_package.sh` copies that tree, fills in `Autobleem/bin/autobleem` (`build_rpi/autobleem-gui`
  + `src/resources`, minus `internal.db`), `Autobleem/bin/db` (`db/covers*.db`) and `themes/`
  (`payload/themes`), strips the placeholders, and tars it to `build_rpi/autobleem-rpi.tar.gz`. `install.sh`
  then finds or creates the exFAT partition, copies `Autobleem/ themes/ Games/ Apps/` onto it as they are, and
  puts the launcher on tty1 via systemd with `getty@tty1` disabled. Documented as `sudo bash install.sh`
  because a package built on Windows loses the executable bit.
- The installer's data partition is meant to be on the SD card next to the root (the owner's preference);
  with a root already expanded over the card that is `--shrink-root <GiB>`: an offline shrink at the next
  boot done **from the initramfs** - `system/shrink-root-hook.sh` packs e2fsck/resize2fs/parted/sfdisk/
  mkfs.exfat in, `system/shrink-root-premount.sh` is an initramfs-tools `local-premount` script that shrinks
  the root while it is still unmounted, restoring `cmdline.txt` first; `disarm_shrink()` removes both on the
  next run. The first attempt used `init=` the way Pi OS's own first-boot resize does, and that cannot work
  for a shrink: by the time `init=` runs the root is mounted, and `resize2fs` will not shrink a mounted
  filesystem (Pi OS only ever *grows*, which works online). A USB stick with an exFAT partition labelled
  `AUTOBLEEM` also works (`--disk /dev/sda`), but is not the intended setup.
- Two gotchas the port turned up. `System::getAvailableSpace()` called a `floatToString()` that **has never
  existed anywhere in the code base** - the whole `#ifndef AB_DEBUG_HOST` branch had simply never been
  compiled, because no ARM build had ever run. Fixed with a file-local helper. And `config.ini`'s `Cfg=` key
  is an absolute console path (`/media/Autobleem/rc/autobleem_cfg.sh`) that `writeSelectionScript()` writes
  and the session wrapper reads back; the installer rewrites it to the real mount point per install.

## lib_ableem

A portable library (`lib_ableem/`, namespace `ableem`) in two CMake targets, mirrored in `include/ableem/`
and `lib_ableem/src/`:
- **`ableem_engine`** (`include/ableem/engine/`, umbrella `<ableem/engine.h>`) - no SDL at all: filesystem,
  strings, ini/cfg files, the SQLite game database, cover dbs, disc images, the scanner, RetroArch playlists.
  `starter` links only this. Vendored code lives in `lib_ableem/third_party/` (sqlite, nlohmann json) and
  `src/engine/unecm.c`, all private to the library - the app includes none of them.
- **`ableem`** (`include/ableem/ui/`, links `ableem_engine`) - owns every SDL/SDL_image/SDL_mixer/SDL_ttf
  call: Platform, Renderer, Texture, Font, Audio, Input, GuiBase, GuiScreen, types.h.
- `include/ableem/ableem.h` - the umbrella over both.

Never add a `#include <SDL2/...>` to anything under `src/code/`; if you need new SDL functionality, add it to
the library. Same for sqlite/json: extend `GameDatabase`/`RetroArchPlaylist` instead.

### engine

The app imports the engine names into its global namespace once, in `src/code/core/main.h` (explicit `using`
declarations - not `using namespace ableem`, because the app's `GuiScreen` shares its name with
`ableem::GuiScreen`), so app code writes `DirEntry::exists(...)`, `IniFile`, `GameDatabase` unqualified.

- **`Environment`** (`engine/environment.h`) - every path the engine touches. The library has no
  `AB_DEBUG_HOST`, `/media`, `/gaadata` or `/usr/sony` literals: `main.cpp`'s `setupEnvironment()` decides
  the layout for the platform and calls the setters (`setUsbRoot/GamesDir/RegionalDbFile/InternalDbFile/
  WorkingPath/SonyDataPath/ThemesDir/CoversDbDir/InternalGamesDir`) once; everything else is derived
  (`getPathToMemCardsDir()` = games + `!MemCards`, `getPathToMemcardTemplateDir()` = working + `memcard`, ...).
  The app's `Env` (`src/code/core/services/environment.h`) derives from it and only adds `autobleemKernel`/`hiddenMenuEnabled`.
  `starter` sets the two roots it needs itself.
- **`DirEntry`/`sep`** (`engine/filesystem.h`) and the string helpers (`engine/strings.h`: in-place `trim/
  lcase/...` free functions, copying `Strings::trim/replaceAll/toInt/...`) are the old `DirEntry.h`, `main.h`
  and `Util` string parts, unchanged in API. The app reaches them as `Strings::`; the process helpers
  (`runAndWait`, `execUnixCommand`, `powerOff`, ...) are `System` (`core/services/system.h`).
- **`game_types.h`** - `ImageType`, `GAME_INI`/`EXT_*`, `SAVESTATES_DIR_NAME`/`MEMCARDS_DIR_NAME`.
- **`Lang`** (`engine/lang.h`) - the translation table: `load(langDir, name)`, `translate`, `listLanguages`,
  `dumpUntranslated`, and `setCurrent`/`ableem::translate()` for a global `_()`. Was the app's `Lang` singleton.
- **`MemcardImage`** (`engine/memcard_image.h`) - a 128 KB .mcd (or DexDrive) image and its 15 slots: block
  kinds and chains, product code / game id / Shift-JIS title per save, delete/undelete, export/import of a save
  between cards, and each icon frame as 16x16 RGBA pixels. The app's `CardEdit` (`evoui/card_edit.*`) is the
  texture wrapper over it.
- **`IniFile`** (`load/reload/mergeFrom/save`), **`ConfigFileEditor`** (pcsx.cfg / RetroArch cfg line editing:
  `getValue/replaceUsb/replaceInternal/replace/replaceInFile`), **`MemcardManager`** (`create/remove/rename/
  list/swapIn/swapOut/backup/restore/restoreAll/storeToRepo` over `<games>/!MemCards`).
- **`GameDatabase`** (`engine/game_database.h`) - the SQLite wrapper for regional.db, internal.db and the
  covers dbs, file-local RAII `Stmt` class inside; add queries the same way. Rows come back as
  `GameRecord`s (`engine/game_record.h`); the app's `PsGame : ableem::GameRecord` adds the launcher-only fields
  and `PsGame::fromRecords()` wraps `loadUsbGames()`/`loadInternalGames()` results. `reloadUsbGame(*game)`
  refreshes one. Schema notes in `lib_ableem/src/engine/database_tables.txt`.
- **`CoverDatabase(coversDir)`** - the three regional covers dbs; `findBySerial/findByTitle(.., GameMetadata&)`
  (was `Metadata::lookupBy*` reaching into `gui->coverdb`). `Gui::coverdb` still points at the one instance
  owned by `runAutobleem()`.
- **`UsbGame`/`GamesHierarchy`/`GameScanner`** - the scan. `GameScanner::scanGamesDirectory(hierarchy,
  coverDb)` then `writeRegionalDatabase(hierarchy, db)`; progress is reported to a `ScanProgressListener`
  (`ScanStage::Scanning/Game/DecompressingEcm/UpdatingDatabase/GameFailedVerify`). The app's listener is
  `SplashScanProgress` (`gui/scan_progress.*`): `Gui::splash(_(...))` per stage, and the 3 s pause after a
  failed verify. `AutoBleem::rescan` constructs the `GameScanner` with one.
  `UsbGame::verify()` reasons are plain English (only written to `gamesThatFailedVerifyCheck.txt`).
- **`SerialScanner`** (`readSerial/readSerialFromImage/readSerialByWorkaround/serialFromMd5/normalizeSerial/
  serialToRegion`), **`IsoDirectoryReader::read`**, **`EcmDecoder::decode`** (+ `setProgressHandler`, which
  is how unecm.c's percentage messages reach the splash). Private: `cd_image_reader.h` (`CdImageReader`,
  `ChdImageReader` behind `ABLEEM_ENABLE_CHD`), `binary_reader.h`, `md5.*` (replaces `head|md5sum`).
- **`RetroArchPlaylist`** - `.lpl` files: `load/loadJson/loadSixLine/save` over `RetroArchPlaylistEntry`.
  `Gui::exportDBToRetroarch` and `RetroArchService` are the only callers.
- **`ThemeSpec`** (`engine/theme_spec.h`) - a theme as a typed struct (`music`, `classic`, `launcher`, `sounds`)
  with `load/save` of theme.json (never throws), `mergeOver(base)` for a partial theme over the default, and
  `resolveFiles()` (the theme's file if it exists, else the default's). `fileFields()` is the one list every
  file loop uses. Scalars a theme may omit are `Opt<T>`; colours are `ThemeColor` (`#rrggbb`).
  The app's `ThemeConverter` (`core/services/theme_converter.*`) is the only writer besides tests.
- **`ZipArchive`** (`engine/zip_archive.h`) - `list/extract` of a .zip over vendored miniz (`third_party/miniz/`,
  built with `MINIZ_NO_TIME` and no write side). Entry names are checked before anything is written: no
  `..`, no absolute paths, no backslashes. Themes dropped as zips are its only caller.

### ui

- **`Platform`** - owns SDL_Init/window/TTF_Init/Mix_Init (created by `GuiBase`). `isDevHost()` replaces the
  app's old per-call `AB_DEBUG_HOST` checks for cursor grab; `setPowerOffHandler()` is how the app supplies
  what "power off" means (main.cpp wires it once to `gui->drawText(...); System::powerOff();`) - `Input::poll()`
  calls it automatically on the console power button or Esc, so screens never check for that themselves.
  `Platform::shutdownSDL()` must be registered with `atexit()` before the first `GuiBase`/`Gui` is constructed
  (done once, in `main.cpp`) - it runs SDL_Quit() after everything else is destroyed.
- **`Renderer`** - the one SDL_Renderer, `clear/present/setDrawColor/fillRect/drawRect/drawLine/copy/setTarget`.
- **`Texture`** - shared handle (copy freely) with `loadFile/loadMemory/createTarget/createStreaming`, plus
  `PixelLock` (RAII `lock()`) for per-pixel `get/set` - replaces the old manual `SDL_LockTexture` +
  `SDL_AllocFormat`/`SDL_MapRGBA` dance (see `engine/cardedit.cpp`, the memory card icon renderer).
- **`Font`** - shared handle over SDL_FontCache: `textSize/width/lineHeight/draw/drawAlign/drawColor`. The
  app's own `Fonts`/`FontEnum` (`gui/gui_font.*`) is unchanged in spirit - it still maps FONT_15_BOLD etc to a
  themed .ttf path, just building `ableem::Font`s now instead of `FC_Font_Shared`s.
- **`Sound`/`Music`/`Audio`** - `Sound::play()` replaces `Mix_PlayChannel(-1, chunk, 0)`; `Audio::close()` is
  the old "close until `Mix_QuerySpec` fails" loop, now one call (`gui->audio().close()`).
- **`Input`** - one `poll(Event&)` replaces `SDL_PollEvent` + `PadMapper` + `gui/abl.c`'s PSC event filter
  (still there, moved to `lib_ableem/src/ui/psc_event_filter.c`, wired up by `Input`'s constructor). `Event::Type`
  is `Quit/ButtonDown/ButtonUp/DpadDown/DpadUp/KeyDown/KeyUp/TextInput/PadAdded/PadRemoved/RenderReset`;
  `Button`/`Key` replace `SDL_BTN_*`/`SDLK_*`. `dpadUp()/Down()/Left()/Right()/Centered()` are the old
  `PadMapper::isUp()` etc (state, not just "this event's direction" - screens read them right after `poll()`
  returns a Dpad event, same priority order as before: up, down, right, left, center).
  `setKeyboardAsPad(true)` (the default on a dev host) is what lets `tools/win_drive.ps1` drive the app -
  X/O/S/T = cross/circle/square/triangle, I/J/K/L = d-pad, Space/B = Start/Select, Q/E/1/2 = L1/R1/L2/R2.
- **`GuiBase`/`GuiScreen`** - `GuiBase` owns Platform+Renderer+Input+Audio in that order. The app's `Gui`
  (`gui/gui.h`) derives from it and adds theme/config/database/carousel state - lib_ableem has no idea what a
  theme or a database is. The app's own `gui/gui_screen.h` is now a thin shim: `class GuiScreen :
  public ableem::GuiScreen` that also carries `std::shared_ptr<Gui> gui` and `ableem::Renderer &renderer` as
  members, so every existing screen file keeps writing `gui->cursor.play()` / `renderer.copy(...)` unchanged -
  only the SDL-specific calls inside each screen needed converting, not every constructor caller. Screens are
  constructed with a `GuiBase&`, in practice always `*gui` (e.g. `GuiConfirm confirm(*gui);`).
- **CMake**: `add_subdirectory(lib_ableem)` from the root file; `ABLEEM_EMBEDDED_TARGET` is forced on for both
  ARM builds (no cursor grab, keyboard-as-pad off); the non-MinGW branch does one `find_package(SDL2)` and links
  the bare names `SDL2 SDL2_image SDL2_mixer SDL2_ttf`, which is why each cross toolchain ships its own
  `cmake/FindSDL2.cmake` defining those four imported targets; `ABLEEM_ENABLE_CHD` follows the root `AB_ENABLE_CHD`
  (libmamecd is linked by `ableem_engine`); `lib_ableem/examples/demo.cpp` (`ableem_demo` target) is a
  from-scratch smoke test of the ui library alone - texture + font + sound + input, no AutoBleem code involved.

## Build

Five targets in `CMakeLists.txt`, each linking only the one below it: `ab_core` (`src/code/core/`, the
app's SDL-free model+services layer, links `ableem_engine`), `ab_ui` (`gui/` and `app.*`: Gui, the
classic screens and menus, AppAudio, the splash and the `App` model; links `ab_core` + `ableem`), `ab_evoui` (`evoui/`: the carousel at the top, `screens/` and `controls/`; links `ab_ui`), `autobleem-gui`
(`main.cpp`, `autobleem.*`; links `ab_evoui`) and `starter` (small PCSX wrapper used
by the stock-UI path, links `ab_core` only). **C++14** (the Sony toolchain is GCC 8+). SQLite is
compiled into `ableem_engine` from `lib_ableem/third_party/sqlite/sqlite3ab.c`. Debug builds compile with
`-Wall -Wextra` (a few noisy categories off) - keep them warning-free.

- **PlayStation Classic (real target)**: `make_psc.sh` → `toolchains/psc/PSCtoolchainV8.cmake` → `build_psc/dist/`
  (`autobleem-gui` + `starter`), built **on the build server over ssh** - the same shape as pcsx-ab's
  `make_psc.sh`, so the two build side by side there. `ssh psc-build` (a `Host` entry in `~/.ssh/config`, in
  both the Windows profile and `C:\msys64\home\<you>` - MSYS2's ssh and Git for Windows' ssh have different
  homes; key `~/.ssh/id_ed25519`). Ubuntu x86_64, 2 cores, Sony's crosstool-NG toolchain at `/opt/toolchain`
  (GCC 8.2.0, sysroot `/opt/toolchain/armv8-sony-linux-gnueabihf/sysroot` with SDL2 2.0.4 + image/mixer/ttf
  `.so`s). The distro CMake is 3.10; `~/opt/cmake` (3.31) is what the script uses. The tree is rsynced to
  `~/autobleem` (minus `usb/`, `db/`, `payload*/`, `!refactor/`, the Pi devkit), built in `~/autobleem/build_psc`
  with Unix Makefiles `-j2`, and the two binaries come back by tar (rsync refuses NTFS modes). `-k` keeps the
  remote build dir for an incremental rebuild. Invoke from the MSYS2 UCRT64 shell like `make_win.sh`.
  `toolchains/psc/cmake/FindSDL2.cmake` defines the four imported SDL2 targets over the sysroot's `.so`s
  (2.0.4 predates `sdl2-config.cmake`). The console build is **dynamic** - the original toolchain file's
  `--static` was always overwritten by the root CMakeLists' `^arm` branch (`-march=armv8-a+simd -Os -s`), and
  `rc/autobleem.sh` unpacks `Autobleem/lib/libs.tar.gz` (SDL2, SDL2_mixer) to `/tmp/lib` at boot. First
  built this way 2026-09-17: GCC 8 warning-free, `Tag_CPU_arch: v8`, NEON, hard-float, and the binary needs
  at most `GLIBCXX_3.4.22` / `GLIBC_2.7`, which the console's stock libstdc++ 6.0.22 / glibc 2.24 provide
  (the toolchain's own libstdc++ is 6.0.25 - anything newer than 3.4.22 would fail to load on the console;
  check with `readelf -V` after adding C++ library features). **Not yet run on a console.**
- **Raspberry Pi (32-bit Pi OS)**: `make_rpi.sh` → `toolchains/rpi/RPitoolchain.cmake` → `build_rpi/`, then
  `tools/make_rpi_package.sh` for the installable tarball. See the "Raspberry Pi port" section above.
- **Mac/Linux**: `make_mac.sh`, `make_sys.sh`.
- **Windows/MinGW (dev + smoke test)**: `make_win.sh` → `build_win/autobleem-gui.exe`. Uses MSYS2 UCRT64
  (`C:\msys64`, installed 2026-09-15) with `mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,SDL2,SDL2_image,SDL2_mixer,SDL2_ttf,pkgconf}`.
  Invoke from PowerShell as `$env:MSYSTEM='UCRT64'; C:\msys64\usr\bin\bash.exe -lc "cd /e/Programming/autobleem-develop && ./make_win.sh"`.
  Run needs `C:\msys64\ucrt64\bin` on PATH (SDL DLLs); the `starter` target is skipped on Windows.
  Windows-only shims: `mkdir` one-arg, `sys/wait.h` guarded, `System::runAndWait` stubbed. The x86/Windows/Pi
  switch is the single macro `AB_DEBUG_HOST` (defined in `core/services/environment.h`) — use it, never
  `__x86_64__` directly.
- **`libmamecd`** (`#include <libmamecd/cdrom.h>`, link `mamecd`) is vendored (2026-09-17) under
  `lib_ableem/third_party/libmamecd/` - an updated libchdr fork for CHD disc images, used only by
  `lib_ableem/src/engine/cd_image_reader.h` (`ChdImageReader`). It builds from source on every host now (its
  own `CMakeLists.txt` there builds the libchdr sources plus vendored copies of its three codec deps - FLAC,
  lzma, zlib - each under `deps/`, all warnings-off like the other vendored code), so `AB_ENABLE_CHD`
  defaults to ON everywhere and no longer needs the PSC toolchain sysroot or a system install. OFF (which
  also sets `ABLEEM_ENABLE_CHD=OFF` / `ABLEEM_NO_CHD`) still compiles out `ChdImageReader` (`.chd` games then
  scan as "no serial") if it's ever unwanted. Two upstream bugs needed a fix to build with a modern
  mingw-w64/UCRT toolchain: FLAC's `compat.h` redefines `fseeko`/`ftello` to `fseeko64`/`ftello64` when
  `HAVE_FSEEKO` isn't set, which collides with UCRT's own `fseeko64` declaration - fixed by defining
  `HAVE_FSEEKO` for the vendored FLAC target on `MINGW` (its native fseeko/ftello are already correct); and
  `libchdr_cdrom.c`'s `physical_to_chd_lba` was a plain (non-`static`) C99 `inline` function with no
  out-of-line definition anywhere, which linked fine only because GCC happened to inline every call - fixed
  by marking it `static inline` (it is only ever used within that one file).
- External libs: SDL2, SDL2_image, SDL2_mixer, SDL2_ttf, pthreads. Vendored, all inside lib_ableem:
  SQLite, nlohmann json + `fifo_map`, miniz and libmamecd + FLAC/lzma/zlib (`lib_ableem/third_party/`),
  `unecm.c` and SDL_FontCache (`lib_ableem/src/`).
- `PRE_BUILD` step copies `src/resources/` next to the binary; the app expects to run from that dir.
- **Tests**: `tests/` builds two doctest executables against `ab_core` and runs under `ctest`
  (`ctest --test-dir build_win --output-on-failure`; `make_win.sh` does it for you). `AB_BUILD_TESTS=OFF`
  skips them, and both cross toolchain files force that. Every service extracted from a screen from here on
  ships with its tests in the same commit - see `docs/refactor-plan.md` section 4.
  - `tests/support/env_fixture.h` - **use it in any test that touches a path.** `ableem::Environment`'s
    setters are static, so without it tests inherit each other's roots and pass or fail by run order.
  - `tests/support/temp_dir.h` - a scratch tree that deletes itself; `makeSubDir`/`writeFile`/`readFile`.
  - Add a suite with `ab_add_test(<name> core/<file>.cpp)` in `tests/CMakeLists.txt`. Tests include app
    headers from `src/code`, e.g. `#include "core/services/config.h"`.
  - The test exes need `C:\msys64\ucrt64\bin` on PATH to run directly (ctest inherits it from the
    MSYS2 login shell; running one from another shell exits 127 without it).

### Smoke test layout (Windows)

**`python tools/make_usb.py usb`** builds this whole tree in `usb/` (git-ignored) from the repo - payload rc
scripts and themes, `src/resources` + the fresh exe next to the binary, cover DBs (copied from `db/`, or
empty ones with the real schema), a copy of `internal.db`, and one generated fake PS1 game (a 24-sector
MODE2 ISO with `SLUS_012.34`, so the scan finds a serial). Re-running refreshes what comes from the repo and
keeps what the app wrote; `--fresh` wipes it. The `.vscode/` tasks (`build`, `usb (refresh)`, `run`,
`smoke test`) and the F5 debug config (gdb from MSYS2, `preLaunchTask` = build + usb refresh) run against it;
`.vscode/` is git-ignored, so re-create it from this description if it is missing.

A fake USB root works for 1-arg mode. Minimum tree (cover DBs in `db/` are 5 KB stubs, fine for UI testing):
```
usb/Autobleem/bin/autobleem/   <- contents of build_win/ (exe + resources)
usb/Autobleem/bin/db/          <- db/covers*.db
usb/Autobleem/rc/              <- payload/Autobleem/rc/*
usb/System/Databases/internal.db  <- src/resources/internal.db
usb/System/Logs/               usb/Games/<game dirs>/   usb/themes/ <- payload/themes/*
```
Run `autobleem-gui.exe <usb>` from `usb/Autobleem/bin/autobleem`; stdout/stderr are the log. Expected noise on
Windows: `ALTER TABLE ... duplicate column` (the add-column-if-missing idiom) and a failed `popen` of
`backup_internal.sh`.

**Keyboard = gamepad on debug hosts** (`ableem::Input::setKeyboardAsPad`, on by default off the console):
`X O S T` = cross/circle/square/triangle, `I J K L` = d-pad, `Space` = Start, `B` = Select, `Q E 1 2` = L1 R1 L2 R2,
`Esc` = power off (exits). `tools/win_drive.ps1 -Usb <usb> -Sequence "x;5;space;8"` starts the exe, posts those keys
to its window, screenshots after each, and collects the logs — use it to smoke test without a controller.

### Running on PC (debug)

```
autobleem-gui /path/to/usb-root                       # 1-arg: everything read from a USB image/folder
autobleem-gui /path/regional.db /path/to/Games        # 2-arg: DB + Games dir, resources from cwd
```
The x86 ifdef blocks make PC runs use `Env::getWorkingPath()` (cwd) for themes/db/resources; the console uses
`/media/...` paths. On PC, launching a game shows a splash instead of forking the emulator.

## Runtime layout on the console

USB stick root = `/media` on the PSC:

```
/media/Autobleem/bin/autobleem/   autobleem-gui + resources (run.sh, themes fallback, lang/, evoimg/, ...)
/media/Autobleem/bin/emu/         pcsx-ab + plugins/*.so
/media/Autobleem/bin/db/          covers*.db (regional cover-art DBs; "../db" relative to the binary)
/media/Autobleem/rc/*.sh          boot/launch glue (see payload/Autobleem/rc)
/media/Autobleem/lib/libs.tar.gz  shared libs unpacked to /tmp/lib at boot
/media/Games/                     user games, one folder per game; !SaveStates/, !MemCards/ sub-dirs
/media/System/Databases/          regional.db (USB games), internal.db (copy of stock DB + extra columns)
/media/System/Logs/               AB_out.txt / AB_err.txt (stdout/stderr of autobleem-gui)
/media/themes/<name>/theme.json   UI themes (docs/theme-format.md); /media/retroarch/ RetroBoot; /media/Apps/ launchable apps
/gaadata/<id>/                    stock internal games (read-only console storage)
```

Boot chain: `rc/autobleem.sh` → unpack libs → `bin/autobleem/run.sh` → `autobleem-gui /media`.
Game launch: `rc/launch.sh` (PCSX, args: ssFolder, cdfile, lang, region, gameFolder, resume, aspect, filter, pad)
or `rc/launch_rb.sh` (RetroArch: file, core). `LaunchService::writeSelectionScript()` writes `rc/autobleem_cfg.sh`
(`AB_SELECTION=...`) which `rc/selection.sh` reads after `AutoBleem::run()`'s loop actually exits the process -
in practice only ever `MENU_OPTION_RETRO` (the L2+R2 system menu's RetroArch/EmulationStation item); starting a
game and returning from one both loop back into the launcher in-process and never reach it. `selection.sh`
falls back to relaunching AutoBleem for anything else.

## Source map (`src/code/`)

`src/code/core/` is the `ab_core` static library (no SDL, no screens - see "Current work"): `main.h`, `model/` and `services/`, nothing else at its top level; `gui/` and
`app.*` are `ab_ui`; `evoui/` is `ab_evoui`; `main.cpp` and `autobleem.*`
are the executable (`AutoBleem::run()` shows `GuiLauncher` directly - see "Current work", 2026-09-17). `core/model/timing.h` holds `TicksPerSecond` and the showing-timeout
defaults, which both the services and the screens need.

| Area | Files | Notes |
|---|---|---|
| Entry | `main.cpp` | `setupEnvironment()` parses argv and configures `ableem::Environment` for the platform (the only place that knows `/media`, `/usr/sony`, the 1-arg debug layout), registers `SDL_Quit`, then constructs the one `AutoBleem` and calls `run()`. |
| `autobleem.*` | `AutoBleem : App` | The program: `run()` opens the DBs, restores memcards, requests a scan up front when `games.fingerprint` doesn't match (or is missing, or there are loose game files, or `gamelist.xml` is gone), starts `scans()` and shows the splash, then loops `GuiLauncher` directly - `MENU_OPTION_START` → `launchGame()` (watching paused around it) → back to the launcher; `MENU_OPTION_RETRO` exits the loop. Chooses the `ProcessRunner` the launch service forks with (a splash on the dev host). In the executable, above both UI libraries. |
| `app.*` | `App` | The model: owns `Config`, `Lang`, `Theme`, `Clock`, `AppAudio`, the `GameLibrary`, the `Session`, every service (including `ScanService`, `app.scans()`) and the `Gui` singleton. Top of `ab_ui`. `App::get()` is for the few places that are not screens; screens use `GuiScreen`'s `app` member. |
| `core/model/session.h` | `Session` | Where we are across one run: `menuOption` (`MENU_OPTION_IDLE`/`RETRO`/`START` - the classic-UI values are gone), the game being started (`runningGame`, `EmuMode`, `resumePoint`), and `launcher`, the carousel's `GameSetSelection`. |
| `core/services/scan_service.*` | `ScanService` | The background scan: one worker thread (lowest OS priority - `System::lowerCurrentThreadPriority()`) does the filesystem work (`GamesFingerprint`, `GameScanner`, its own `CoverDatabase`) and queues `WorkerEvent`s; `poll()`, called once a frame from `GuiLauncher::loop()`, applies every regional.db write on the main thread and returns a `ScanUpdate` (added/updated/removed games, progress, finished). `requestScan()`/`scanning()`/`setWatching()`; `checkForChanges()` is the watcher's debounce, checked every `ScanWatchInterval` when nothing was requested directly. Owned by `App` (`app.scans()`). |
| `core/main.h` | | The `using` declarations that bring the lib_ableem engine names (`DirEntry`, `sep`, `ImageType`, `GAME_INI`, `trim`/`lcase`, `IniFile`, `GameDatabase`, ...) into the app's global namespace. |
| `core/services/environment.*` | `Env` | `struct Environment : ableem::Environment` + the two app flags and the `AB_DEBUG_HOST` macro. All path getters live in the library; extend `ableem::Environment` instead of adding new literal paths. |
| `core/services/system.*` | `System` | The process/console helpers: `execUnixCommand` (popen, returns "" on failure), **`runAndWait(exe, args)`** - the only fork/exec in the code base, `powerOff`, `getAvailableSpace`, `getRandom*`. The string helpers are `Strings::` (`ableem::Strings`, via `main.h`). |
| `core/main.h` | `_()` | The app's `_("...")` is `ableem::translate()`, which goes through the `ableem::Lang` the `App` owns and registered (`app.lang()`); `resources/lang/<Language>.txt` is pairs of lines, source then translation. Emoji markers like `\|@X\|` in strings are replaced by button textures by `TextRenderer`. |
| `core/services/clock.*` | `Clock` | The "last played" time as text: `displayTime(t)` in config.ini's `datetimeformat`, "" for a time the console could not have known (before 2020 - no battery clock). Owned by `App` (`app.clock()`). |
| `evoui/card_edit.*` | `CardEdit` | A memory card as the manager shows it: `ableem::MemcardImage` plus its 45 icon frames as textures, kept in step after every edit, and the translated "Free"/"Link Block" titles. |
| `core/services/config.*` | `Config` | `config.ini` on top of `ableem::IniFile`: app defaults (`language`, `aspect`, ...; keys are lower-cased on load, e.g. `values["theme"]`) and a few obsolete keys dropped on load, `ui` (the classic UI is gone) among them. Owned by `App`; read as `app.config().inifile.values["..."]`. |
| `core/services/theme.*` | `Theme` | The current theme's `theme.json` merged over `themes/default/theme.json` (so every key has a value), every file resolved to the theme's own or the default's. `load()` converts an old-layout folder first (`ThemeConverter`). Owned by `App`; read as `app.theme().classic().menuPanel.x`, `app.theme().launcher().footer`, `app.theme().sounds().cursor`. No platform `#ifdef`s - the paths come from `Env`. |
| `core/services/theme_installer.*` | `ThemeInstaller` | `<themes>/<name>.zip` -> `<themes>/<name>/` (root files or one folder inside; replaces an existing folder; a non-theme becomes `.zip.bad`). Run by `Theme::load()` and the Options theme list before they look at folders. |
| `core/services/theme_converter.*` | `ThemeConverter` | `theme.ini` + the PSC data tree -> `theme.json` + role-named files, in place: json first, then the renames, then the deletes. `needsConversion(dir)` is also what makes an old folder count as a theme in the Options menu. `tools/theme_convert` wraps it. |
| `gui/app_audio.*` | `AppAudio` | The background music track and the five UI sounds (`cursor`, `cancel`, `home_up`, `home_down`, `resume`), plus which track to play (theme's or the user's from `resources/music`) at which sample rate. Owned by `App`: `app.audio().cursor.play()`. Sits on `gui->audio()`, which is only lib_ableem's mixer device. |
| `gui/gui.*` | `Gui` singleton | The screen only: SDL window/renderer (via `ableem::GuiBase`), `assets()`, `text()`, and the background/logo/status drawing that combines them. `display(resume)` (re)inits and shows the splash (`resume=false`, boot only) or sets `session().resumingGui` for the launcher to pick up (`resume=true`, after a game exits). |
| `gui/screens/gui_splash.*` | `GuiSplash` | Fades in, holds at full brightness for `SplashHoldDuration` (2s), fades back out, then returns - `Gui::display(false)` is its only caller, once at boot. |
| `gui/theme_assets.*` | `ThemeAssets` | The current theme's textures (background, logo, jewel case, the `|@X|` button markers) and fonts (`themeFont` at the theme's size, plus the `themeFonts`/`sonyFonts` sets). `load()` re-reads theme.json (`Theme::load()`) and reloads everything from the resolved paths. Screens use `gui->assets()`. |
| `gui/text_renderer.*` | `TextRenderer` | The classic UI's text drawing: `|@X|` button markers laid out inline with text, `renderTextLine/ToColumns/Options`, selection and label boxes, the theme's menu-panel/status-bar rects, `toColor()`. Holds references to `Gui`'s theme font and button textures; screens use `gui->text()`. |
| `gui/gui_screen.h` | `GuiScreen` | Base for every screen: `init/render/loop` + virtual `doCross_Pressed()`-style handlers; `show()` runs them. Set `menuVisible=false` to exit. |
| `gui/menus/gui_*` | `GuiMenuBase`, `GuiOptionsMenuBase`, ... | Header-only templated list menus (string, two-column, playlist, game dir) and concrete Options / Memory Cards / Game Manager / Game Editor menus. |
| `gui/screens/gui_*` | | The rest of the classic screens, shown from the launcher's L2+R2 system menu or its sub-screens: About, Confirm dialog, on-screen Keyboard, pad test, memcard select, scroll window. `gui/starfx.*` is the star field the About screen draws. |
| `gui/gui_font.*` | `Fonts`, `FontEnum` | Theme/Sony SST font loader built on `ableem::Font` (SDL_FontCache itself is now in lib_ableem). |
| `evoui/screens/evoui_launcher.h`, `evoui_launcher_screen.cpp`, `evoui_launcher_input.cpp`, `evoui_launcher_actions.cpp` | `GuiLauncher` | EvolutionUI, the only screen `AutoBleem::run()` shows, in three files: the screen (assets, the sets - PS1 all/internal/favorites/history/sub-dir, RetroArch playlists, Apps - the metadata panel, state transitions, `render()`), the input (the event loop - polls `app.scans()` once a frame via `applyScanUpdate()`, before `render()` - and per-button handlers, L2+R2 among them), and the actions (what Cross does per state and menu icon, and L2+R2's system menu). Holds the `Carousel` as `carousel`. A black overlay fades out over `LauncherFadeInDuration` every time the screen is shown (`fadeAlpha`/`fadeStart`). `scanStatusLine` (bottom of the screen) shows the scan's progress or its "Scan complete" summary; `reloadGames()` re-runs the current set's query and re-selects the same game by id whenever the roster changed and no scroll animation is running. |
| `evoui/screens/evoui_system_menu.*` | `GuiSystemMenu` | The L2+R2 overlay: Re-Scan Games, RetroArch/EmulationStation, Memory Cards, Game Manager, Hardware Information, Options, About, Power Off - everything the classic main menu used to offer, Power Off included (no more direct L2+R2 shutdown). A dumb picker (translucent panel, launcher fonts/colours, Up/Down + wrap, Cross/Circle) - it returns a `SystemMenuAction` and `GuiLauncher::loop_openSystemMenu()` runs it. |
| `evoui/carousel.*`, `carousel_game.*` | `Carousel`, `PsCarouselGame` | The row of covers: `games` (with the fewer-than-13 duplication rule), `selected`, the 13 screen positions, the scroll/moveMainCover animations, texture load/free on visibility, `render()`. |
| `evoui/controls/evoui_*.{h,cpp}` | `PsObj` and subclasses | The EvolutionUI controls: the animated elements the launcher is built from (`PsObj` base, meta panel, menu, buttons, labels, the state selector). Class names keep their `Ps` prefix. |
| `core/model/ps_game.*` | `PsGame : ableem::GameRecord` | Game as seen by the UI (from DB via `PsGame::fromRecords`, or playlist). `PsGamePtr = shared_ptr<PsGame>`. Adds the RetroArch/App fields. A plain data record - the resume points are `ResumePointService`'s, the memcard `MemcardService`'s. |
| `core/services/game_catalog.*` | `GameCatalogService` | The writes: play history ranking, game delete, cover flush. Owned by `App` (`app.gameCatalog()`). |
| `core/services/resume_point.*` | `ResumePointService` | The save-state slots in a game's `!SaveStates` folder, and the prepare/save around a PCSX launch. Owned by `App` (`app.resumePoints()`); non-screens reach it via `App::get()`. |
| `core/services/memcard.*` | `MemcardService` | The `!MemCards` sets and a game's chosen card; the swap in/out around a launch. Owned by `App` (`app.memcards()`). |
| `core/services/game_settings.*` | `GameSettingsService` | The game editor's model: a game's Game.ini flags and pcsx.cfg values, read with `open()` and written one setter per option. Owned by `App` (`app.gameSettings()`). |
| `core/services/game_query.*` | `GameQueryService` | Which games a set shows and in what order - `gamesFor(selection)` is the whole of the old `switchSet` query. Owned by `App` (`app.gameQuery()`); RetroArch arrives through the `RetroArchGames` interface. |
| `core/services/retroarch.*` | `RetroArchService` | RetroArch's playlists as sets of foreign `PsGame`s: `.lpl` parsing (both formats via `ableem::RetroArchPlaylist`), core detection from `info/*.info` + `coreOverride.cfg`, Favorites/History. Implements `RetroArchGames`. Owned by `App` (`app.retroArch()`). |
| `core/services/launch.*`, `process_runner.*` | `LaunchService`, `ProcessRunner` | A game launch start to finish: argv for `rc/launch.sh` (PCSX) / `rc/launch_rb.sh` (RetroArch) / an App's `startup`, the memcard and resume-point work around it, the RetroArch config transfer, `writeSelectionScript()`. Runs through a `ProcessRunner`. Owned by `App` (`app.launcher()`). |
| `evoui/screens/evoui_mc_manager.*`, `evoui_app_start.*`, `evoui_btn_guide.*` | | Launcher sub-screens. |
| `evoui/controls/evoui_notification_line.*` | `NotificationLines` | The two timed text lines at the top of the launcher. |
| `starter.cpp` | separate binary | Wraps `/tmp/pcsx` for the stock SonyUI path; swaps memcard from `Game.ini`. |

Payload (`payload/`): the release USB tree — `rc/*.sh` scripts, themes (`ab2`, `aergb`, `autobleem`,
`default`, `evolution`), bundled Apps, release notes. `payload_rpi/` next to it is the Raspberry Pi installer
package, not part of the USB tree (see "Raspberry Pi port"). `db/` is git-ignored (cover DBs live there).

## Conventions and gotchas

- **A game launch gives the display up.** `AutoBleem::launchGame()` closes the audio, flushes the pads and,
  when the `ProcessRunner` says `needsExclusiveDisplay()` (the fork runner does, the dev host's splash runner
  does not), calls `Gui::releaseDisplay()`: every texture and font is dropped (`ThemeAssets::unload()`,
  `Fonts::closeAll()`), then the renderer and the window go (`GuiBase::releaseDisplay()` - SDL's video
  subsystem is quit, which is what drops the KMS/DRM master on a Pi). `Gui::display(true)` afterwards finds
  no window, acquires one and reloads the assets. So: **never keep an `ableem::Texture` or `Font` in an
  object that outlives a launch** other than `ThemeAssets` - SDL frees them with the renderer and the handle
  would free them again. Screens are stack objects that die before the launch, which is what makes this
  safe.
- The one remaining singleton is `Gui` (`static shared_ptr<Gui> getInstance()`).
  `Gui::db` / `Gui::internalDB` / `Gui::coverdb` are non-owning pointers; the objects are `unique_ptr`s in
  `runAutobleem()` (main.cpp). `GuiLauncher`'s named `PsObj*` members are non-owning shortcuts into
  `staticElements`/`frontElemets`, which own them (`addStaticElement(new T(...))`).
- Ownership rule: no raw owning pointers. Short-lived helpers and screens are stack objects; anything that
  must outlive a scope goes in a `unique_ptr`. Exceptions are never thrown on purpose; `main()` has a
  last-resort `catch` that logs to `AB_err.txt`.
- `sep` is the path separator (a `Sep` helper in `<ableem/engine/filesystem.h>` wrapping `separator`, which is
  `'/'` on every platform - Windows accepts it, and the code base compares paths as strings); paths are built by
  string concatenation. `path + sep` only appends when the separator is not already there.
- Ini keys are lower-cased: `cfg.inifile.values["theme"]`. Theme values are typed (`app.theme().classic()`), not a map.
- Bool-ish config values are the strings `"true"`/`"false"`; ints are parsed with `atoi`.
- Menu/emulator/state selections are plain `int`s with `#define`s (`EMU_PCSX`, `SET_PS1`, `STATE_GAMES`) —
  easy to mix up; converting to `enum class` is on the plan.
- The carousel duplicates games when fewer than 13 exist, so one `PsGamePtr` may appear in several
  `PsCarouselGame`s (see `Carousel::setGames` and the comment in `carousel.h`).
- SDL lifecycle: `TTF_Init`/`Mix_Init` once in `GuiBase`, `SDL_Quit` registered with `atexit` in `main` so it
  runs after the `Gui` singleton is destroyed. Audio is fully closed (`Mix_CloseAudio` loop) before forking PCSX.
- Console `stdout`/`stderr` go to `/media/System/Logs/AB_*.txt`; `cout` is the logging mechanism and is
  unit-buffered so the last lines survive a crash.
- Files are read/written by bare `ifstream`/`ofstream`; use `ios::binary` for anything that is not text
  (PNG blobs, .mcd cards, PBP headers) or the Windows build corrupts it.
- Scripts that edit sources from Python must pass `encoding='utf-8'` (CLAUDE.md got mangled once).
- Shell scripts and cfg/ini files must stay **LF** (enforced by `.gitattributes`). Do not let the Windows
  editor convert them.
- Keep the two `trim` families in mind: in-place `trim()` (`ableem::trim`, via `main.h`) vs copying `Strings::trim()`
  (`ableem::Strings::trim`).
- Layout rule: each UI library has its screens under `screens/` and its reusable elements under `controls/`
  (`gui/menus/` is the classic menu framework and its menu screens). File names are snake_case with the
  library's prefix: `gui_*` in `gui/`, `evoui_*` in `evoui/`. Class names did not change with the files
  (`PsObj`, `GuiLauncher`, `GuiManager`, ...).
- Match existing style: 4-space indent, `//***` banner comments above functions, `using namespace std;` in .cpp.

## Git

Repo was `git init`ed on 2026-09-15 from the final source snapshot (no upstream history here; the public
history is at github.com/screemerpl/cbleemsync). Commit per logical refactor step. `.gitattributes` forces LF.
