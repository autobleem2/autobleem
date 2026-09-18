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

The next structural step (its plan was `docs/refactor-plan.md`, removed once every step was done - the git
history has it) was to split `src/code` into `ab_core` (model +
services, no SDL, unit tested with doctest), `ab_ui` and `ab_evoui`, moving the game queries, settings,
memcard/savestate and launch logic out of the screens that currently hold them. The list below is folded into
that plan's phases. Phase A is done apart from step 3 (the ARM build), which is deferred - no toolchain on
this host yet:

- **Step 1** - the `#define`-int selections are `enum class`es now: `GameSet` (+ `nextGameSet` for the Select
  wraparound) and `Ps1SelectState` in `session.h`, `LauncherScreenState` in `gui_launcher.h`, and the
  `LauncherMenuOption` in `gui_launcher.h`. `PsMenu::selOption` stays an `int` - it is a
  generic index into the icon row that `PsMenu` animates by `++`/`--` - and is compared through
  `selOptionIs()`. `EmuMode` and `MenuOption` were already enums.
- **Step 2** - `ab_core` exists (`src/code/core/`, links `ableem_engine` only).
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
use, resolves each entry's core (its own if installed, else `resources/platform/<platform>.cores.cfg`, else the first
`.info` listing the playlist's database), keeps Favorites/History after the platforms and refreshes them
after a RetroArch run (`reloadFavoritesAndHistory()`). `escapeName()` is the boxart file name rule.

Still to do, in order:

1. ~~Continue the refactor plan - phase B, the service extractions~~ - done (see above); the rule stays: a
   service extracted from a screen ships with its tests in the same commit and moves into `ab_core`.
2. ~~Centralize the hard-coded paths in `Env`~~ - done (2026-09-18). The engine side, the theme loaders, the
   launch scripts and RetroArch paths (`LaunchService`) and `backup_internal.sh` went first; the last one,
   `config.ini`'s `Cfg=` key (the selection script as an absolute console path, which the Pi installer had
   to rewrite per install), is gone: `LaunchService::selectionScriptFile()` is `<rc>/autobleem_cfg.sh`, and
   `Config` drops a stale `cfg` key on load. `EnvironmentSetup` (core) and `main.cpp`'s `/autobleem` check are the only places left that
   spell a console path; `RetroArchService::mapPlaylistPath()`'s `/media` is the
   console playlist *format*, not this machine's layout.
3. ~~Split `GuiLauncher`~~ - done (phase D, 2026-09-16). **The refactor plan is complete.**
4. ~~Set up the Sony ARM toolchain~~ - done 2026-09-17: `make_psc.sh` builds on the remote server (see Build).
   `autobleem-gui` cross-compiles and links cleanly with the Sony GCC 8.2 toolchain. **Still to
   do: run it on a console** - nothing has run on real hardware yet; the Windows/MinGW build is the only one
   that has been executed. The Pi toolchain (below) is a different target and does not substitute for it.
5. Features. Done on 2026-09-17: **themes are `theme.json`** (`docs/theme-format.md`). `ableem::ThemeSpec` is
   the typed theme (engine, JSON in/out, partial-over-default merge, per-file fallback), `ThemeConverter`
   (`core/services/theme_converter.*`) turns an old `theme.ini` + PSC-data-tree folder into the new layout in
   place - `Theme::load()` does it on first contact, `tools/theme_convert` ahead of time - and `payload/themes`
   ships converted (aergb 334 -> 29 files). The stock SonyUI is no longer re-skinned (`rc/selection.sh`), and
   `src/resources/sony/` is just the two SST fonts. **The console tools are in the tree** (2026-09-18,
   `apps/`, see "Console tools"): pscbios and abflashkit build with the launcher, draw with its theme and
   run on Windows against fakes; `psctools/` (the 2020 sources) is deleted, the import commit has them. Screens read `app.theme().classic()/launcher()/sounds()`.
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
     their old numeric values - `rc/selection.sh` trimmed to match: RetroArch for `SEL_RETROARCH`, a reboot
     for anything else). No more `ui=classic`/EvolutionUI choice (`Config` drops a stale `ui` key
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

CHD support no longer depends on an external install: a CHD library is vendored (libmamecd on
2026-09-17, replaced by upstream **libchdr with zstd** on 2026-09-18 - see the "libchdr" bullet under
Build). `AB_ENABLE_CHD` defaults ON on every host.

**AutoBleem-NG port** (2026-09-18, plan in `~/.claude/plans/there-is-a-project-tingly-pixel.md`): the public
fork `github.com/AutoBleem-NG/autobleem` is 122 commits past the snapshot this repo started from (its
`924a02cb`, 2021-03-14, is byte-identical to our `src/code`). Done so far, one commit each: the Phase 0 bug
fixes (`play_us_ra` typo, locked games keeping their serial, CHD exported as `.chd.cue`, the `.m3u`
generator, the year on the meta panel, the per-size bold font cache `Fonts::boldAtSize`, translation
wrappers + sorted languages, music not restarting on theme browse, Favorites fallback, rc guards, the
stock-SonyUI/`.lic`/RetroBoot-patch cleanup), the libchdr refresh, and Phase 1 - `RdbReader`,
`MetadataLookup`, `ThumbnailLookup` (see lib_ableem/engine below), verified on the Pi 400 with the
libretro box arts mirrored by `payload_rpi/install.sh --thumbnails`; Phase 2, the multi-disc folder merge
(`DiscSuffix`, `mergeMultiDiscFolders`); pcsx-ab's libchdr refresh (its own repo); `core/version.h` + the
`make_psc.sh` link gates; Phase 3, lightgun games (`LightgunService`, `GameSet::Lightgun`, the editors);
plog (`<ableem/engine/log.h>`); the Key=Value language files + `tools/lang_tools.py`; fitted/wrapped/elided
text in `TextRenderer` with the Game Manager's preview pane and the launcher's `launcher.snapPanel`; and
`docs/menu-options.md` + `docs/translation.md`. **The port is complete** apart from what was left out on
purpose: the fork's Docker/CI pipeline, gtest (doctest does the job) and the RetroBoot-1.2.1 Apps payload.
**UPX** is in (2026-09-18): `make_psc.sh` packs the fetched console binary and `tools/make_rpi_package.sh` the
Pi one (`upx --best --lzma`, 3.1 MB -> 1 MB, MSYS2's `mingw-w64-ucrt-x86_64-upx`; `AB_NO_UPX=1` skips, and a
debug build is never packed - gdb cannot read a packed binary). The packed Pi binary was run on the Pi 400.
Its Options paging and "Font" rows came over on 2026-09-18 (`GuiOptions::render` spreads the rows over the
panel and pages by what fits at the font's height; `themefont`/`font` in config.ini, `Fonts::userFontPath`
picks the classic font from `retroarch/fonts`, `resources/fonts` or the theme folder). Its clang-format/clang-tidy setup came over afterwards (see "Code style" under
Build).

**Chinese (Simplified)** (2026-09-18): `resources/lang/Chinese_Simplified.txt` (the fork's file, completed for
our keys) plus a CJK font the fork never shipped - `resources/fonts/NotoSansSC-Regular.otf` (8 MB, Noto CJK
SC subset, SIL OFL; `OFL.txt` next to it). `Fonts::cjkFontFor(language)` names it for a language whose name
contains "Chinese" (and it exists), and `ThemeAssets::load()` then uses it as *every* font - the theme's
classic font and the launcher's medium/bold pair - because no theme font has the glyphs. A language change
in Options calls `gui->loadAssets(false)` so the swap happens live, both ways. Themes are untouched.

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
`Env::getPathToRetroarchDir()` is that folder on a Pi - **from `resources/platform/rpi.ini`** (`PlatformConfig`,
see the source map), which also names the Pi's PS1 core and where the `retroarch` binary may be; the
console's `psc.ini` keeps `retroarch/` and RetroBoot's core and binary. `roms/` gets a folder per system
named as RetroArch's playlists are, so *Import Content -> Scan Directory* on it lands in the set.
`install.sh` builds the **latest tagged RetroArch from GitHub on the Pi** (KMS/EGL/GLES, udev,
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
  `autobleem-session.sh` loop that replaces `rc/selection.sh`, the two shrink-root initramfs pieces, the plymouth theme), and the data-partition
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
- **Boot screen** (2026-09-18): the installer sets the KMS mode for the whole boot on the kernel command line
  (`video=HDMI-A-1:1280x720@60 video=HDMI-A-2:...`, `--hdmi-mode`; `config.txt`'s `hdmi_mode` is ignored by
  the KMS driver) - the launcher asks SDL for 1280x720 anyway, so plymouth and the launcher share one mode and
  the handover is not a modeset. The AutoBleem logo is a plymouth `script` theme, `payload_rpi/system/plymouth/`
  (`splash.png` 1280x720 on black), installed to `/usr/share/plymouth/themes/autobleem` and packed into **every** installed kernel's initramfs
  (`update-initramfs -u -k all` since 2026-09-18 - the 32-bit image carries one per board, v6/v7/v7l/v8,
  and a card set up on the Pi 400 showed the stock theme when moved to a Pi 3; the shrink hook stays on the
  running kernel, `-k $(uname -r)`, it is for this board's next boot - never bare `-u`). `cmdline.txt` gets `splash plymouth.ignore-serial-consoles`,
  `config.txt` gets `disable_splash=1` in its own `[all]` section. The handover: `autobleem.service`
  `Conflicts=plymouth-quit.service` (the display-manager pattern, so systemd does not take the splash down
  when the system is up) and `autobleem-session` runs `plymouth quit --retain-splash` first thing, before
  even checking for the binary - plymouth holds the DRM master, SDL needs it. `--no-boot-splash` skips
  plymouth, `--no-quiet-boot` implies it. Verified on the Pi 400 the same day: plymouthd comes up from the
  initramfs (PID ~181, `vc4.ko` is in there - plymouth's initramfs hook pulls the DRM modules in even with
  `MODULES=dep`), `plymouth-quit.service` stays inactive, `plymouth-quit-wait` finishes in the same second
  as the session's "boot splash taken down", the CRTC is 1280x720, kernel to launcher ~8 s.
- **BIOS pack** (2026-09-18): `install.sh`'s `download_bios_pack()` fetches `system/biospack.txt` into
  `RetroArch/system/` - one line per file, `<sha256> <size> <url> <path>`, wget + `sha256sum` per file, a
  `.part` renamed once the hash checks out, files already right are skipped (so a re-run only repairs) - and
  `install_ps1_bios()` copies SCPH-5501/5500 to `System/Bios/romw.bin`/`romJP.bin` for pcsx-ab unless the
  user's own are there. `--no-bios` skips both; `--no-downloads` does not. The manifest is written by
  **`tools/biospack.py`** from [RetroBIOS](https://github.com/Abdess/retrobios) (`install/retroarch.json`
  + `install/targets/retroarch.json`, pinned to one commit in `RETROBIOS_REF`): the `linux-armhf` target's
  cores minus `mame`, an allow-list of `Vendor/System` folders (everything `RA_ROM_SYSTEMS` has a folder for -
  consoles, handhelds and the Amiga/C64/MSX/Spectrum/PC-98/X68000 computers - plus arcade, Neo Geo CD,
  ScummVM, Doom/Wolfenstein engine data) and path excludes (arcade `samples/`, MAME's
  `history/mameinfo/cheat.dat`, stella's `.wav`, x86 `.dll/.so/.dylib`, `dc/`, `kronos/`). 626 files,
  189 MB (ScummVM is 94 of them), against 5.8 GB for RetroBIOS's whole RetroArch pack. `RA_ROM_SYSTEMS`
  names are RetroArch's rdb names (`database/rdb/*.rdb`) so a scan of `roms/` lands in the right playlist;
  `disksys.rom` (FDS) lives in RetroBIOS's `Arcade/FBNeo` folder, and DOS has a folder but no files (its
  only "BIOS" entries are x86 MIDI libraries).
  `--list` shows what is in and out, `--check DIR` verifies a `system/` folder. **No BIOS file is in this
  repository** - only their hashes and URLs.
- **Other systems run** (2026-09-18): ROMs in `RetroArch/roms/<system>/`, scanned by RetroArch's own Import
  Content (guide: `payload_rpi/README.md`, "Games for the other systems"), launched from the RetroArch set
  in-process like pcsx. What that took: `RetroArchService::mapPlaylistPath()` (no double `/media` prefix on a
  Pi), only installed cores in the database->core table, the `showOptions()` null deref on a fresh screen
  with a foreign game (the launcher used to die on the way back and be restarted with the splash),
  `platform/<platform>.cores.cfg` (Genesis Plus GX over picodrive, whose Cyclone core segfaults on the Pi -
  unexplained), and blueMSX's `Machines/*/config.ini` in the BIOS pack (`EXTRA_SOURCES` in
  `tools/biospack.py`). `make_rpi.sh --debug` + the PC's cross gdb is how a Pi core dump gets read (gdb on
  the Pi hangs in `snd_pcm_open` on the mapped ALSA device).
- **The ROM scan, step 1 of `docs/retroarch-scanner-plan.md`** (2026-09-18): `ScanService`'s cycle goes on
  from `Games/` to the RetroArch ROM folders **when RetroArch is detected** - `ScanService::romScanEnabled()`,
  the binary the platform ini names exists and `retroarch_roms_dir` (a new key: `roms` on the console and
  PC, `RetroArch/roms` on the Pi -> `Env::getPathToRetroarchRomsDir()`) is a directory; RetroArch is optional
  on every platform and without it none of this runs. `ableem::RetroArchScanner` (engine, see lib_ableem)
  writes `<playlists>/<system>.lpl` per `roms/<system>/` folder from the file names alone, merged over what
  is there; the worker reports `WorkerEvent::Kind::PlaylistsWritten`, `poll()` has `RetroArchService::
  reloadPlaylists()` re-read them and the launcher refreshes its playlist names and the RetroArch set
  (`ScanUpdate::playlistsWritten`). `roms.fingerprint` (`GamesFingerprint::takeAllFiles`) is watched next
  to `games.fingerprint`; `ScanService::fingerprintsMatchDisk()` is the startup check for both.
  `resources/platform/roms_folders.cfg` names the folders that are not named as their database is
  (`Arcade` and `SNK - Neo Geo` -> `FBNeo - Arcade Games`). Verified on the PC's fake tree (`tools/make_usb.py`
  fakes a RetroArch install: stub binary, one `.info`, zipped ROMs) and **on the Pi 400** over its 848
  ROMs - RetroArch's own playlists survive byte for byte, a copy/delete lands in the carousel 17 s later;
  **not yet on a console.** **Step 2** (2026-09-19): with `<retroarch>/database/rdb/<system>.rdb` there,
  the worker names every ROM the database knows before the merge (`RetroArchScanner::identify` - a zip
  member by the CRC the archive records, a loose file by hashing it up to 64 MB, an arcade set by
  `rom_name`; a miss keeps the file's name, nothing is dropped) and an identified name replaces an
  unidentified one an earlier scan wrote. `RetroArchService::ensureMetadata()` then reads the same rdb
  once per playlist for publisher/year/players, which the meta panel shows for a RetroArch game the
  database knows. On the Pi: 679 of 848 named in ~2 s. Steps 3-5 (box art, UI polish, `UpdateRoms.exe`
  for the console) are still in the plan.
- Two gotchas the port turned up. `System::getAvailableSpace()` called a `floatToString()` that **has never
  existed anywhere in the code base** - the whole `#ifndef AB_DEBUG_HOST` branch had simply never been
  compiled, because no ARM build had ever run. Fixed with a file-local helper. And `config.ini`'s `Cfg=` key
  was an absolute console path that the installer had to rewrite per install - gone since 2026-09-18, the
  selection script is `Env::getPathToRCDir() + autobleem_cfg.sh` on every platform.

## Console tools (`apps/`, 2026-09-18)

The two standalone tools the console runs from `Apps/` - **PSC-Bios** (`apps/pscbios/`: WiFi, timezone,
the gamepad mapping wizard) and **ABFlashKit** (`apps/abflashkit/`: the kernel flasher) - were 2020 forks
of the old AutoBleem GUI with their own copies of everything (`psctools/` in git history). They are targets
in this tree now, built on `ab_classic`: each has a `<tool>_core` static library (SDL-free, links `ab_core`,
tested from `tests/apps/`) and the program on top, its own `CLAUDE.md`, `resources/` (what ships next to
its binary in `payload/Apps/<tool>/`) and `resources/lang/` (Key=Value, validated by `make_win.sh`; the
tool loads the main GUI's language file first, then its own on top - `Lang::loadMore`). They draw with the
main GUI's theme through `AppBase` and read its `config.ini` - `EnvironmentSetup::forTool()` sets the
working path to the launcher's resources dir and pins `Env::getAppDir()` to the tool's own folder. The
console-only work is behind an interface with a fake for the dev host (`ConsoleBackend` -> `AbnetBackend` /
`FakeBackend`), the `ProcessRunner` pattern, so `usb/Apps/<tool>/<tool>.exe <usb root>` runs on Windows for
a visual test (`tools/make_usb.py` stages it, `tools/win_drive.ps1 -Tool <tool>` drives it). `make_psc.sh`
builds them next to the launcher, gates them with `check_psc_binary.sh`, packs them and copies each binary
plus its `resources/` into `payload/Apps/<tool>/`; a Pi does not build them. `tools/format.sh`,
`tools/lint.sh` and `.clang-tidy` cover `apps/`.

## lib_ableem

A portable library (`lib_ableem/`, namespace `ableem`) in two CMake targets, mirrored in `include/ableem/`
and `lib_ableem/src/`:
- **`ableem_engine`** (`include/ableem/engine/`, umbrella `<ableem/engine.h>`) - no SDL at all: filesystem,
  strings, ini/cfg files, the SQLite game database, cover dbs, disc images, the scanner, RetroArch playlists.
  Vendored code lives in `lib_ableem/third_party/` (sqlite, nlohmann json) and
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
- **`MetadataLookup(coversDir, rdbFile)`** (`engine/metadata_lookup.h`, 2026-09-18) - where a game's title,
  publisher, year and players come from: RetroArch's `Sony - PlayStation.rdb` (`Environment::
  getPathToPlayStationRdbFile()`, `<retroarch>/database/rdb/` on both targets' standard tree) through
  **`RdbReader`** (`engine/rdb_reader.h`: the whole `.rdb` in memory, rmsgpack, indexed by serial and name,
  `findBySerial` also takes a suffixed serial like `SLUS-01251GH`; since 2026-09-19 also `crc`, `size`,
  `rom_name` with `findByCrc`/`findByRomName` for the other systems' databases), else the three covers dbs through
  **`CoverDatabase`** - and the covers db is still asked for its PNG when the rdb answered, so a stick with
  no thumbnails keeps its art. `GameMetadata::recordName` is the rdb's name (`"Crash Bandicoot (USA)"`),
  `title` has the trailing tags stripped, `lastRegion` ("U"/"P"/"J", what pcsx.cfg's region is set from)
  comes from the rdb's region, PAL countries included. `GameLibrary::metadata()` owns the launcher's; the
  scan worker has its own (sqlite handles are per thread). Ported from AutoBleem-NG.
- **`ThumbnailLookup`** (`engine/thumbnail_lookup.h`, 2026-09-18) - where a cover/title screen/snap is in
  `<retroarch>/thumbnails/<db name>/Named_Boxarts|Titles|Snaps/`, and the user's own screenshots and save-state
  pictures: the rdb's record name first, then the title, each with trailing ` (...)` tags peeled one at a
  time, then a fuzzy `"<bare name> ("` match scored by shared tags and region. Listings are cached per
  instance (`DirEntry::listNames` - no stat per entry, Named_Boxarts is ~9000 files) - the scan makes its
  own, `App::thumbnails()` is the launcher's, cleared when a game returns. `escapeName()` is the one file
  name rule (`RetroArchService::escapeName` delegates). The scanner resolves every game's cover and snap
  and caches them in Game.ini (`Thumbnail_record_name`, `Cached_cover_path`, `Cached_snap_path`, read back
  onto `GameRecord::recordName/coverPath/snapPath`); the carousel's PS1 chain is the PNG next to the game
  -> the cached path while its file exists -> a fresh lookup (internal games) -> `default.png` drawn at
  draw time. **No `default.png` is copied next to a game any more** and a cover is not a verify()
  requirement; a placeholder byte-identical to `default.png` is removed by the scan once a thumbnail
  exists (`DirEntry::filesAreIdentical`). Titles of *unlocked* games lose their region tag on a rescan
  with an rdb around (`"Persona (USA)"` -> `"Persona"`).
- **`DiscSuffix::parse`** (`engine/disc_suffix.h`, 2026-09-18) - `"Game (Disc 2)"` / `"(Disk 2)"` / `"(CD 2)"` /
  `"(CD2)"` / `"Game - Disc 2"` -> `{base, disc}`. **`GameScanner::mergeMultiDiscFolders(gamesDir)`** runs in
  the scan worker before the tree is read: sibling folders whose names differ only by that marker become one
  `<base>` folder - the lowest disc's folder is renamed, the other discs' images (chd/pbp/cue/bin/img) are moved
  in, **their folders are deleted with their Game.ini/pcsx.cfg/save states** (the owner accepted the fork's
  behaviour), disc 1's Game.ini keeps its settings but loses its `Discs=` key (the scan rebuilds it) and a
  folder-derived `(Disc 1)` title. A group is skipped when `<base>` already exists as something else or a
  file would be overwritten. `ScanStage::MergingDiscs` is the status line. pcsx-ab gets the first disc's
  `.cue` as before (its frontend cannot open an `.m3u`; its disc picker lists the folder); RetroArch gets
  the `.m3u`. Ported from AutoBleem-NG's `mergeMultiDiscGames`, moved ahead of the scan because our
  `onGameVerified` events would otherwise have announced the per-disc folders first.
- **`UsbGame`/`GamesHierarchy`/`GameScanner`** - the scan. `GameScanner::scanGamesDirectory(hierarchy,
  metadata)` then `writeRegionalDatabase(hierarchy, db)`; progress is reported to a `ScanProgressListener`
  (`ScanStage::Scanning/Game/DecompressingEcm/UpdatingDatabase/GameFailedVerify`). The app's listener is
  `SplashScanProgress` (`gui/scan_progress.*`): `Gui::splash(_(...))` per stage, and the 3 s pause after a
  failed verify. `AutoBleem::rescan` constructs the `GameScanner` with one.
  `UsbGame::verify()` reasons are plain English (only written to `gamesThatFailedVerifyCheck.txt`).
- **`SerialScanner`** (`readSerial/readSerialFromImage/readSerialByWorkaround/serialFromMd5/normalizeSerial/
  serialToRegion`), **`IsoDirectoryReader::read`**, **`EcmDecoder::decode`** (+ `setProgressHandler`, which
  is how unecm.c's percentage messages reach the splash). Private: `cd_image_reader.h` (`CdImageReader`,
  `ChdImageReader` behind `ABLEEM_ENABLE_CHD`), `binary_reader.h`, `md5.*` (replaces `head|md5sum`).
- **`RetroArchPlaylist`** - `.lpl` files: `load/loadJson/loadSixLine/save` over `RetroArchPlaylistEntry`,
  plus the `RetroArchPlaylistHeader` (2026-09-18): every top-level field but `items`, as opaque JSON text
  in file order, so a playlist RetroArch 1.22 wrote (version 1.5, `sort_mode`, `scan_content_dir`, ...)
  survives a rewrite; `save()` puts `version` first. `GameLibrary::exportToRetroArchPlaylist`,
  `RetroArchService` and the scanner are the callers.
- **`CoreInfoTable`** (`engine/retroarch_cores.h`, 2026-09-18) - `<retroarch>/info/*.info` for the cores
  whose `.so` is installed (`CoreInfo`: display name, extensions, databases, `block_extract`), each database
  mapped to the first installed core listing it (most extensions first) unless the cores.cfg given to
  `load()` overrides it. Was `RetroArchService::loadCores`; the service holds one, the scan worker builds
  its own.
- **`RetroArchScanner`** (`engine/retroarch_scanner.h`, 2026-09-18) - the ROM scan:
  `scan(Options{romsDir, playlistsDir, targetRomsDir}, systemsFrom(cores))` walks each `roms/<system>/`
  folder that has a core, one entry per game (a `.cue` hides its bins, an `.m3u` its discs, a `.ccd` its
  image; a `.zip` for a core that does not read archives itself - `zip` among its extensions, or
  `block_extract` - is opened: one ROM inside is `zip#rom` named after the zip with the ROM's CRC from the
  central directory, several are one entry each, none is skipped; an arcade core gets the zip whole),
  label = the file's stem, and merges into the existing playlist (`Options::folderAliases` sends a folder
  named otherwise to its database's playlist): entries
  outside the folder stay, an entry whose file is still there is kept exactly (RetroArch's own label/CRC),
  the vanished go, the new join, sorted by label; `.tmp` + `DirEntry::replaceFile`, only when something
  changed. `targetRomsDir` is what the playlists name (the console's `/media/roms` when a PC writes them -
  the plan's step 5); `""` = `romsDir`. `AutoBleem`, `Applications` and `content_*` are never written.
  With `Options::rdbDir` (2026-09-19) each folder's `ScannedRoms` go through `identify()` first: the
  system's `.rdb` names a zip member by CRC, a loose file by `Crc32::ofFile` (up to `maxCrcBytes`), an
  arcade set (`wholeArchive`) by `rom_name`; the label becomes the record's name and, in the merge, an
  identified entry replaces an existing one for the same ROM whose label differs.
- **`ThemeSpec`** (`engine/theme_spec.h`) - a theme as a typed struct (`music`, `classic`, `launcher`, `sounds`)
  with `load/save` of theme.json (never throws), `mergeOver(base)` for a partial theme over the default, and
  `resolveFiles()` (the theme's file if it exists, else the default's). `fileFields()` is the one list every
  file loop uses. Scalars a theme may omit are `Opt<T>`; colours are `ThemeColor` (`#rrggbb`).
  The app's `ThemeConverter` (`core/services/theme_converter.*`) is the only writer besides tests.
- **`ZipArchive`** (`engine/zip_archive.h`) - `list/extract` of a .zip over vendored miniz (`third_party/miniz/`,
  built with `MINIZ_NO_TIME`). Entry names are checked before anything is written: no
  `..`, no absolute paths, no backslashes. Themes dropped as zips are its only caller. **`ZipWriter`**
  (`engine/zip_writer.h`, 2026-09-18) is the write side: `open/addFile(path, name)/addBytes/close`, files
  streamed through miniz's read callback with 64-bit offsets, so a partition image of any size goes in
  without being read into memory - for abflashkit's `LBOOT.EPB`. The entry size is given up front on
  purpose: it is what keeps the archive plain zip (no zip64), which is what the console's recovery reads.
- **`Md5`** (`engine/md5.h`, public since 2026-09-18) - RFC 1321: `ofBytes/ofString/ofFile` (streamed) and
  the incremental `update/hexDigest`. `SerialScanner::serialFromMd5` and abflashkit's kernel check use it.
- **`Crc32`** (`engine/crc32.h`, 2026-09-19) - `ofFile(path, crc, maxBytes)` streamed over miniz's
  `mz_crc32` (false, no read at all, over the cap), `ofBytes`, and `playlistText()` (`"%08X|crc"`). What
  the ROM scanner identifies a loose file by.

### ui

- **`Platform`** - owns SDL_Init/window/TTF_Init/Mix_Init (created by `GuiBase`). `isDevHost()` replaces the
  app's old per-call `AB_DEBUG_HOST` checks for cursor grab; `setPowerOffHandler()` is how the app supplies
  what "power off" means (main.cpp wires it once to `gui->drawText(...); System::powerOff();`) - `Input::poll()`
  calls it automatically on the console power button or Esc, so screens never check for that themselves.
  `Platform::shutdownSDL()` must be registered with `atexit()` before the first `GuiBase`/`Gui` is constructed
  (done once, in `main.cpp`) - it runs SDL_Quit() after everything else is destroyed.
- **Output scale** (2026-09-18): the app draws on a logical 1280x720 canvas; the window may be bigger by
  `Renderer::outputScale()` (`GuiBase(title, w, h, outputScale)`), and every `Renderer` call maps logical to
  output pixels (`toOutput()`, edges rounded so neighbours tile; identity at 1). `Texture::createTarget`
  allocates output pixels and carries `pixelScale()`, which `copy()` applies to a source rect, so a target is
  addressed like the screen; `Font::load` loads the face `scale` times bigger, draws in output pixels and
  measures in logical ones. Nothing in the app knows. `Gui::outputScale()` is the policy: a Pi on a >= 1080p
  display gets 1.5 (`Platform::desktopDisplaySize()`), a dev host reads `AB_OUTPUT_SCALE`, the console is 1.
  The Pi installer boots in 1920x1080 by default now (`--hdmi-mode`), the plymouth script scales the logo up.
- **MSAA** (2026-09-18): `GuiBase(..., multisampleSamples)` asks for a multisampled GL context before the
  window exists (`Platform::createWindow`: `SDL_GL_MULTISAMPLESAMPLES`, `SDL_WINDOW_OPENGL`, Windows also
  forced onto the "opengl" driver - direct3d would ignore it; a driver that refuses gets a plain window
  and `Platform::multisampleSamples()` says 0). SDL's GL renderer then rasterises every quad with it, the
  carousel's cover strips included, which now sit at fractional positions (`SDL_RenderCopyF`, SDL >= 2.0.10;
  the console's 2.0.4 headers keep the integer path). `Gui::multisampleSamples()`: 4 on a Pi and a dev host,
  `AB_MSAA` overrides (0 off), the console 0. Was costly on the Pi at 1080p (idle CPU 17% -> 60%) when
  a frame was ~3200 copies; the launcher performance work of 2026-09-18 (its plan, `docs/perf-plan.md`,
  was removed once done - the git log has it and the numbers) took that to 85:
  `AB_FRAME_STATS=1` logs frame times and copies every 5 s (`Renderer::present`) and slow
  `Texture::loadFile`s; the launcher defers the snap/resume-picture loads to the frame the carousel
  settles in and keeps `Carousel::Lookahead` covers past each end decoded; every animation is
  `easeOutCubic` (`core/model/timing.h`), a held stick chains steps without a pause and a tap during a
  scroll is queued; `TextRenderer` caches every run as a texture (`clearTextCache()` on font reload and
  display release); `copyTrapezoid` is one `SDL_RenderGeometry` call on SDL >= 2.0.18 with the tint in
  the vertex colours. Step 7 measured MSAA on the Pi 400 at 1080p: **even 2x drops to 30 fps for
  stretches**, so a Pi runs with 0 (`Gui::multisampleSamples`; a dev host keeps 4) and the covers' edges
  are smoothed by `CoverMargin` - each cover composed 2 px inset into a transparent-black texture, which the
  linear filter blends the edge into. 60 fps solid there, ~25% of a core idle.
- **`Renderer`** - the one SDL_Renderer, `clear/present/setDrawColor/fillRect/drawRect/drawLine/copy/setTarget`,
  and `copyTrapezoid(tex, src, VerticalEdge left, VerticalEdge right)` (2026-09-18): pseudo-3D for the
  carousel - a texture drawn into a trapezoid with vertical sides, one `SDL_RenderCopy` strip per screen
  column, the columns spread perspective-correctly with each side's height as its depth. Nothing newer than
  SDL 2.0.4 (`SDL_RenderGeometry` is 2.0.18, the console has 2.0.12).
- **`Texture`** - shared handle (copy freely) with `loadFile/loadMemory/createTarget/createStreaming`, plus
  `PixelLock` (RAII `lock()`) for per-pixel `get/set` - replaces the old manual `SDL_LockTexture` +
  `SDL_AllocFormat`/`SDL_MapRGBA` dance (see `engine/cardedit.cpp`, the memory card icon renderer).
- **`Font`** - shared handle over SDL_FontCache: `textSize/width/lineHeight/draw/drawAlign/drawColor`. The
  app's own `Fonts`/`FontEnum` (`gui/gui_font.*`) is unchanged in spirit - it still maps FONT_15_BOLD etc to a
  themed .ttf path, just building `ableem::Font`s now instead of `FC_Font_Shared`s.
- **`Sound`/`Music`/`Audio`** - `Sound::play()` replaces `Mix_PlayChannel(-1, chunk, 0)`; `Audio::close()` is
  the old "close until `Mix_QuerySpec` fails" loop, now one call (`gui->audio().close()`).
- **`Joystick`** (`ui/joystick.h`, 2026-09-18) - one device by index opened *raw*, for a pad-mapping wizard:
  `count/nameForIndex/guidForIndex/isGameControllerAtIndex`, `open(i)`, `update()` into `state()` (every
  axis, button and hat as SDL's joystick API reports them, hats as `HatUp|...` masks) and `controllerState()`
  (the 15 standard buttons and 6 axes through the mapping, when it has one). `Input::addMapping(line)`,
  `mappingForDeviceIndex(i)` and `currentMappingPath()` are its companions; `Input::setPowerKeyAsKey(true)`
  makes the power button/Esc arrive as `Key::Sleep` instead of calling the power-off handler (a wizard uses
  it as "cancel"); `Key::Reset`/`Key::Open` are the console's other front buttons (AUDIOPLAY/EJECT scancodes).
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
  (libchdr is linked by `ableem_engine`); `lib_ableem/examples/demo.cpp` (`ableem_demo` target) is a
  from-scratch smoke test of the ui library alone - texture + font + sound + input, no AutoBleem code involved.

## Build

Six targets in `CMakeLists.txt`, each linking only the one below it: `ab_core` (`src/code/core/`, the
app's SDL-free model+services layer, links `ableem_engine`), **`ab_classic`** (2026-09-18: `app_base.*` and
the game-agnostic part of `gui/` - Gui, ThemeAssets, TextRenderer, Fonts, AppAudio, the splash/confirm/
keyboard/about/hardware-info screens and the list-menu framework; links `ab_core` + `ableem`; what the
console tools under `apps/` draw with), `ab_ui` (`app.*` and the game-aware classic screens in `gui/`:
Options, the game editors, Game Manager, memory cards, playlists; links `ab_classic`), `ab_evoui`
(`evoui/`: the carousel at the top, `screens/` and `controls/`; links `ab_ui`), `autobleem-gui`
(`main.cpp`, `autobleem.*`; links `ab_evoui`). `ab_classic` and `ab_ui` share the `gui/` directory - the
split is the CMake source lists, and the include rule: nothing in `ab_classic` includes `app.h`. **C++14** (the Sony toolchain is GCC 8+). SQLite is
compiled into `ableem_engine` from `lib_ableem/third_party/sqlite/sqlite3ab.c`. Debug builds compile with
`-Wall -Wextra` (a few noisy categories off) - keep them warning-free.

- **PlayStation Classic (real target)**: `make_psc.sh` → `toolchains/psc/PSCtoolchainV8.cmake` → `build_psc/dist/`
  (`autobleem-gui`), built **on the build server over ssh** - the same shape as pcsx-ab's
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
  (the toolchain's own libstdc++ is 6.0.25 - anything newer than 3.4.22 would fail to load on the console).
  **`make_psc.sh` checks that on the server before fetching the binary** (`tools/check_psc_binary.sh`:
  highest `GLIBC_`/`GLIBCXX_` version needed, and no RPATH/RUNPATH - the toolchain file sets
  `CMAKE_SKIP_RPATH`, since `FindSDL2.cmake` links the sysroot's `.so` files by absolute path), and passes
  the git facts up as `AB_GIT_*` environment variables because the tree goes up without `.git`. **Not yet
  run on a console.**
- **Raspberry Pi (32-bit Pi OS)**: `make_rpi.sh` → `toolchains/rpi/RPitoolchain.cmake` → `build_rpi/`, then
  `tools/make_rpi_package.sh` for the installable tarball. See the "Raspberry Pi port" section above.
- **Mac/Linux**: `make_mac.sh`, `make_sys.sh`.
- **Windows/MinGW (dev + smoke test)**: `make_win.sh` → `build_win/autobleem-gui.exe`. Uses MSYS2 UCRT64
  (`C:\msys64`, installed 2026-09-15) with `mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,SDL2,SDL2_image,SDL2_mixer,SDL2_ttf,pkgconf}`.
  Invoke from PowerShell as `$env:MSYSTEM='UCRT64'; C:\msys64\usr\bin\bash.exe -lc "cd /e/Programming/autobleem-develop && ./make_win.sh"`.
  Run needs `C:\msys64\ucrt64\bin` on PATH (SDL DLLs).
  Windows-only shims: `mkdir` one-arg, `sys/wait.h` guarded, `System::runAndWait` stubbed. The x86/Windows/Pi
  switch is the single macro `AB_DEBUG_HOST` (defined in `core/services/environment.h`) — use it, never
  `__x86_64__` directly.
- **`libchdr`** (`#include <libchdr/chd.h>`, link `chdr`) is vendored under `lib_ableem/third_party/libchdr/` -
  upstream libchdr at `8bba774` (2025-06-08), the snapshot AutoBleem-NG bundles, replacing the older libmamecd
  fork on 2026-09-18 because chdman's default **zstd** codec was missing there (a fresh CHD would not open).
  Used only by `lib_ableem/src/engine/cd_image_reader.h` (`ChdImageReader`), which now reads hunks with
  `chd_read` and takes track 0's length from `CDROM_TRACK_METADATA(2)` - upstream has no `cdrom_*` layer.
  Builds from source on every host (its own `CMakeLists.txt` there builds the libchdr sources - FLAC is the
  header-only dr_flac - plus vendored LZMA SDK 24.05, zlib 1.3.1 and zstd 1.5.6 under `deps/`, each trimmed
  to what its CMake build needs, all warnings-off like the other vendored code). `AB_ENABLE_CHD` defaults
  ON; OFF (which sets `ABLEEM_ENABLE_CHD=OFF` / `ABLEEM_NO_CHD`) compiles `ChdImageReader` out (`.chd`
  games then scan as "no serial"). `make_win.sh` passes `-DAB_ENABLE_CHD=ON` explicitly: a `build_win/`
  configured before the library was vendored had OFF cached, and that silently outlived the default
  becoming ON - `tests/core/test_cd_image.cpp` (over the zstd-compressed `tests/data/test.chd`, NG's
  fixture) is what noticed. **pcsx-ab** (`E:\Programming\pcsx-rearmed-develop`) got the same refresh the
  same day (its `8f26911`); the Pi payload binary was rebuilt from it then, the console one
  (`payload/Autobleem/bin/emu/`, via its `make_psc.sh`) on 2026-09-18 too - both play zstd CHDs.
- External libs: SDL2, SDL2_image, SDL2_mixer, SDL2_ttf, pthreads. Vendored, all inside lib_ableem:
  SQLite, nlohmann json + `fifo_map`, miniz, plog and libchdr + lzma/zlib/zstd (`lib_ableem/third_party/`),
  `unecm.c` and SDL_FontCache (`lib_ableem/src/`).
- `PRE_BUILD` step copies `src/resources/` next to the binary; the app expects to run from that dir.
- **Code style** (2026-09-18, from AutoBleem-NG): `.clang-format` at the root (LLVM, 4-space indent, 120
  columns, includes left alone; two choices of our own - `AccessModifierOffset: -4` so `public:` sits at the
  class's column, and `AllowShortFunctionsOnASingleLine: Inline` so a banner-commented function keeps its
  body on its own lines). **`tools/format.sh`** formats every source we own (`--check` is what `make_win.sh`
  runs last, and fails the build on an unformatted file - so run `tools/format.sh` before building); the
  vendored trees carry a `DisableFormat: true`. The whole tree was reformatted in one commit, listed in
  `.git-blame-ignore-revs` (`git config blame.ignoreRevsFile .git-blame-ignore-revs` once per clone).
  `.clang-tidy` is the fork's Chromium-derived check set (bugprone-*, google-explicit-constructor /
  readability-casting / default-arguments, the modernize-* fix-its); **`tools/lint.sh`** runs it over every
  .cpp with `build_win/compile_commands.json` and reports each finding once (`--fix` applies the fix-its
  file by file - not through `run-clang-tidy -fix`, which on Windows sees a header included as `../x.h`
  and as `x.h` as two files and applies the same edit twice); `cmake -DAB_ENABLE_CLANG_TIDY=ON` runs it
  during a build. The tree is clean today; keep it so. Both tools come from MSYS2's
  `mingw-w64-ucrt-x86_64-clang-tools-extra` (clang 22); the scripts find them without PATH changes.
  A deliberately implicit conversion gets a `// NOLINT` with its reason (`ThemeSpec`'s `Opt<T>`).
- **Tests**: `tests/` builds two doctest executables against `ab_core` and runs under `ctest`
  (`ctest --test-dir build_win --output-on-failure`; `make_win.sh` does it for you). `AB_BUILD_TESTS=OFF`
  skips them, and both cross toolchain files force that. Every service extracted from a screen from here on
  ships with its tests in the same commit (the refactor plan's rule, kept after the plan itself was done).
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
/media/System/Logs/               AB_out.txt / AB_err.txt (stdout/stderr of autobleem-gui), autobleem.log (plog,
                                  rolling), launch.log / pcsx.log (the launch scripts' and pcsx-ab's), ui_menu.log
/media/System/lightguns.txt       RetroArch games flagged as light-gun games, one image path per line
/media/System/Bios|Preferences|Region|UI/   rc/backup.sh's copies of the console's own files, made at boot
/media/themes/<name>/theme.json   UI themes (docs/theme-format.md); /media/Apps/ launchable apps
/media/retroarch/                 RetroBoot's RetroArch tree: database/rdb/Sony - PlayStation.rdb (game metadata),
                                  thumbnails/Sony - PlayStation/Named_*/ (covers), screenshots/, states/, playlists/
/gaadata/<id>/                    stock internal games (read-only console storage)
```

Boot chain: the exploit payload in `/media/028c18a9-ec4b-4632-b2cf-d4e20f252e8f/` runs `Autobleem/start.sh` →
`rc/boot.sh` (bind-mounts `rc/20-joystick.rules` over `/etc/udev/rules.d` and re-triggers udev, which is what
lets two pads through one hub; `killsony.sh`; `backup.sh`) → `rc/autobleem.sh` → unpack `libs.tar.gz` (SDL2
2.0.12 + SDL2_mixer built with the **Wayland** video backend - the console has no X; SDL2_image/ttf are the
firmware's own 2.0.4, so no SDL API newer than 2.0.4) → `bin/autobleem/run.sh` → `autobleem-gui /media`.
`/autobleem` existing on the console means the AutoBleem kernel is installed (`Env::autobleemKernel`: a real
clock, so "Last played" is shown).
Game launch: `rc/launch.sh` (PCSX, args: ssFolder, cdfile, lang, region, gameFolder, resume, aspect, filter, pad)
or `rc/launch_rb.sh` (RetroArch: file, core). `LaunchService::writeSelectionScript()` writes `rc/autobleem_cfg.sh`
(`AB_SELECTION=...`) which `rc/selection.sh` reads after `AutoBleem::run()`'s loop actually exits the process -
in practice only ever `MENU_OPTION_RETRO` (the L2+R2 system menu's RetroArch/EmulationStation item); starting a
game and returning from one both loop back into the launcher in-process and never reach it. `selection.sh`
reboots for anything else (a crash, a missing `autobleem_cfg.sh`), which brings AutoBleem back up. The stock
SonyUI exit - `starter` mounted over `/usr/sony/bin/pcsx`, USB games linked into `/gaadata` with a `.lic`
each (`link.sh`/`overmount.sh`/`startsony.sh`) - is gone with it (2026-09-18, as in AutoBleem-NG), and so is
`.lic` handling in the scanner. RetroBoot's own update hook went the same day: `autobleem.sh` no longer
runs `retroboot/bin/init.sh` at boot, and the `/tmp/.rbpatching` guards, `rb_patch_background.sh` and
`rb_monitor.sh` are deleted - an RB_Patch dropped on the stick is not applied by AutoBleem any more.

## Source map (`src/code/`)

`src/code/core/` is the `ab_core` static library (no SDL, no screens - see "Current work"): `main.h`, `model/` and `services/`, nothing else at its top level; `gui/` and
`app.*` are `ab_ui`; `evoui/` is `ab_evoui`; `main.cpp` and `autobleem.*`
are the executable (`AutoBleem::run()` shows `GuiLauncher` directly - see "Current work", 2026-09-17). `core/model/timing.h` holds `TicksPerSecond` and the showing-timeout
defaults, which both the services and the screens need.

| Area | Files | Notes |
|---|---|---|
| Version | `core/version.h` (generated) | `Version::VERSION` (the last git tag, else `AB_VERSION_FALLBACK` in CMakeLists - was `config.ini`'s `Version=` key, dropped on load now), `GIT_HASH`, `GIT_BRANCH`, `GIT_DIRTY`, `BUILD_TIMESTAMP`, `FULL_VERSION` (`v2.0.0-pre0 (master@a83777b*)`). Written by `cmake/generate_version.cmake` into `<build>/generated/core/` on every build (`ab_version` target; the header only changes when the facts do). The splash, About and the log's first line use it. Include as `"core/version.h"`. |
| Entry | `main.cpp` | Strips `--sysinfo`, has `EnvironmentSetup::fromArguments()` configure `ableem::Environment`, registers `SDL_Quit`, then constructs the one `AutoBleem` and calls `run()`. |
| `core/services/environment_setup.*` | `EnvironmentSetup` | The layouts a program can be started with (2026-09-18, was `main.cpp`'s `setupEnvironment()`): `fromRoot(root)` (everything under one root - the console's `/media`, the Pi's data partition, the 1-arg debug mode: `Games/`, `System/Databases/`, `Autobleem/bin/autobleem` as the resources dir, `Autobleem/bin/db`, `themes/`; the Sony data tree is the console's own or `<resources>/sony` under `AB_ROOT_RELATIVE_LAYOUT`), `fromDbAndGames()` (the 2-arg debug layout), `fromArguments()` (autobleem-gui's command line) and `forTool(argc, argv, name)` for a console tool in `Apps/<tool>` (optional root, `/media` by default on the console; pins `Env::getAppDir()` - the tool's own folder, `getPathToAppLangDir()` its `lang/` - before anything can chdir). Every one applies `PlatformConfig`. The only place besides `Env::platformName()` that spells `/media` or `/usr/sony`. Tested in `tests/core/test_environment_setup.cpp`. |
| `autobleem.*` | `AutoBleem : App` | The program: `run()` opens the DBs, restores memcards, requests a scan up front when `games.fingerprint` doesn't match (or is missing, or there are loose game files, or `gamelist.xml` is gone), starts `scans()` and shows the splash, then loops `GuiLauncher` directly - `MENU_OPTION_START` → `launchGame()` (watching paused around it) → back to the launcher; `MENU_OPTION_RETRO` exits the loop. Chooses the `ProcessRunner` the launch service forks with (a splash on the dev host). In the executable, above both UI libraries. |
| `app_base.*` | `AppBase` | The model of any program drawn with the classic UI: `Config`, `Lang`, `Theme`, `Clock`, the `Gui` singleton (whose window title it sets - `Gui::setWindowTitle` before the first `getInstance()`) and `AppAudio`. Top of `ab_classic`; every `GuiScreen`'s `app` member is one. `AppBase::get()` for the non-screens (Gui, Theme, AppAudio, Fonts). |
| `app.*` | `App : AppBase` | AutoBleem's model on top of it: the `GameLibrary`, the `Session`, every service (including `ScanService`, `app.scans()`). Top of `ab_ui`. `App::get()` is a `static_cast` of `AppBase::get()`; a game-aware screen declares its own `App &app = App::get();` over `GuiScreen`'s `AppBase &app` (the seven that do: the two game editors, Game Manager, memory cards, playlists, select-memcard, `GuiLauncher`). |
| `core/model/session.h` | `Session` | Where we are across one run: `menuOption` (`MENU_OPTION_IDLE`/`RETRO`/`START` - the classic-UI values are gone), the game being started (`runningGame`, `EmuMode`, `resumePoint`), and `launcher`, the carousel's `GameSetSelection`. |
| `core/services/scan_service.*` | `ScanService` | The background scan: one worker thread (lowest OS priority - `System::lowerCurrentThreadPriority()`) does the filesystem work (`GamesFingerprint`, `GameScanner`, its own `CoverDatabase`, and - with RetroArch detected, `romScanEnabled()` - `ableem::RetroArchScanner` over the ROM folders with its own `CoreInfoTable`) and queues `WorkerEvent`s; `poll()`, called once a frame from `GuiLauncher::loop()`, applies every regional.db write on the main thread, has `RetroArchService` reload rewritten playlists, and returns a `ScanUpdate` (added/updated/removed games, `playlistsWritten`, progress, finished with the game and ROM counts). `requestScan()`/`scanning()`/`setWatching()`; `checkForChanges()` is the watcher's debounce over both `games.fingerprint` and `roms.fingerprint`, checked every `ScanWatchInterval` when nothing was requested directly; `fingerprintsMatchDisk()` is the startup check. Owned by `App` (`app.scans()`, constructed with `&retroArch_`). |
| `core/main.h` | | The `using` declarations that bring the lib_ableem engine names (`DirEntry`, `sep`, `ImageType`, `GAME_INI`, `trim`/`lcase`, `IniFile`, `GameDatabase`, ...) into the app's global namespace. |
| `core/services/environment.*` | `Env` | `struct Environment : ableem::Environment` + the two app flags, the `AB_DEBUG_HOST` macro, `platformName()` (`"psc"`/`"rpi"`/`"pc"` - the one place the build macros decide a path), `retroArchInstalled()` and `padMappingFiles()` (the `gamecontrollerdb.txt` list `Gui`'s constructor hands `Input::loadMappings()` - the kernel's `/etc/autobleem` one on the console, then the shipped one in the resources dir; **loaded since 2026-09-18** - until then nothing called `loadMappings` and the pscbios wizard's output was never read). All path getters live in the library (`getPathToKernelConfigDir()` is `""` off the console); extend `ableem::Environment` instead of adding new literal paths. |
| `core/services/platform_config.*` | `PlatformConfig` | **What differs per target about where things are, as data**: `resources/platform/<platform>.ini` (`psc.ini`, `rpi.ini`, `pc.ini`) - `retroarch_dir` (relative to the USB root), `retroarch_core` (the PS1 core the exported playlist names, relative to that dir), `retroarch_binary` (`;`-separated candidates; "RetroArch" in the system menu and Square on a game are offered when one exists), `retroarch_roms_dir` (the other systems' ROM folders the scan writes playlists for, relative to the USB root; 2026-09-18). `main.cpp` loads and `apply()`s it after the roots are set; a missing file means the console's layout. Add per-platform paths here, never as `#ifdef AB_PLATFORM_RPI` in the services. **`<platformName>.cores.cfg`** next to it (2026-09-18) is which core plays which RetroArch playlist on that platform (`<database name>=<part of a core display name>`, `#` comments), read by `RetroArchService` ahead of its `.info` mapping - was the one `coreOverride.cfg` for every platform; the Pi's prefers Genesis Plus GX (picodrive's Cyclone core segfaulted on the Pi 400), plain Snes9x and blueMSX. Tested in `tests/core/test_platform_config.cpp`. |
| `core/services/system.*` | `System` | The process/console helpers: `execUnixCommand` (popen, returns "" on failure), **`runAndWait(exe, args)`** - the only fork/exec in the code base, `powerOff`, `getAvailableSpace`, `getRandom*`. The string helpers are `Strings::` (`ableem::Strings`, via `main.h`). |
| `core/main.h` | `_()` | The app's `_("...")` is `ableem::translate()`, which goes through the `ableem::Lang` the `App` owns and registered (`app.lang()`); `resources/lang/<Language>.txt` is `English text=Translated text` lines under a `#` header (since 2026-09-18; the old pairs-of-lines layout is still read when the first line is not a comment). **`tools/lang_tools.py`** keeps them in step: `extract` (English.txt from every `_("...")`), `update [--remove-obsolete]`, `validate` (run by `make_win.sh`), `compare <Lang>`, `convert`, `merge <dir>`. A key cannot contain `=` - decorate at render time (`".-= " + _("Testing") + " =-."`). Emoji markers like `\|@X\|` in strings are replaced by button textures by `TextRenderer`. |
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
| `gui/gui_screen.h` | `GuiScreen` | Base for every screen: `init/render/loop` + virtual `doCross_Pressed()`-style handlers; `show()` runs them. Set `menuVisible=false` to exit. Carries `gui`, `renderer` and `app` (an `AppBase &` - see `app_base.*`). |
| `gui/menus/gui_*` | `GuiMenuBase`, `GuiOptionsMenuBase`, ... | Header-only templated list menus (string, two-column, playlist, game dir) and concrete Options / Memory Cards / Game Manager / Game Editor menus. |
| `gui/screens/gui_*` | | The rest of the classic screens, shown from the launcher's L2+R2 system menu or its sub-screens: About (`credits` settable by the caller, AutoBleem's by default - a tool shows its own), Confirm dialog, on-screen Keyboard, memcard select, `GuiTextPage` (a titled page of static `lines`, Circle back - a tool's instructions). `gui/starfx.*` is the star field the About screen draws. (`GuiScrollWin`/`GuiPadTest` were deleted on 2026-09-18 - nothing had shown them since the classic menu went.) |
| `gui/screens/gui_hardware_info.*` | `GuiHardwareInfo` | The Hardware Information screen (2026-09-18) for a machine without the PSC-Bios app: `SystemInfoService`'s sections plus a "Display and input" one only the running program can fill (render driver + MSAA, video driver, display mode, canvas/scale, audio driver, SDL version, the pads by name - `Platform::linkedVersion/videoDriverName/displayModeString`, `Renderer::driverName`, `Input::pads`). Classic layout, rows paged like Options, re-read every second, Up/Down a row, L1/R1/Left/Right a page, Circle back. `autobleem-gui <root> --sysinfo` prints the same sections (minus the display/input one) to stdout and exits - for bug reports and for checking the Linux branch over ssh; verified on the Pi 400 (64-bit kernel: no `model name` in cpuinfo, the core comes from `armCoreName()`'s part-id table). The system menu's item runs `Apps/pscbios/run.sh` on the console when it exists and shows this screen otherwise (always, under `AB_ROOT_RELATIVE_LAYOUT`). |
| `core/services/system_info.*` | `SystemInfoService` | What that screen shows, SDL-free: `collect()` = `system()` (os-release/uname, hostname, uptime, load; the registry on Windows), `hardware()` (device-tree model, cpuinfo, cpufreq, thermal_zone0, meminfo), `storage()` (the data root first, then every block filesystem in `/proc/mounts` - or the fixed/removable drives - with `statvfs`/`GetDiskFreeSpaceEx`), `network()` (IPv4 per interface, `getifaddrs`/`GetAdaptersAddresses` - ab_core links `iphlpapi ws2_32` on Windows), `software()` (version, build, platform, roots, RetroArch). The parsers and formatters are static and tested (`tests/core/test_system_info.cpp`). |
| `gui/gui_font.*` | `Fonts`, `FontEnum` | Theme/Sony SST font loader built on `ableem::Font` (SDL_FontCache itself is now in lib_ableem). |
| `evoui/screens/evoui_launcher.h`, `evoui_launcher_screen.cpp`, `evoui_launcher_input.cpp`, `evoui_launcher_actions.cpp` | `GuiLauncher` | EvolutionUI, the only screen `AutoBleem::run()` shows, in three files: the screen (assets, the sets - PS1 all/internal/favorites/history/sub-dir, RetroArch playlists, Apps - the metadata panel, state transitions, `render()`), the input (the event loop - polls `app.scans()` once a frame via `applyScanUpdate()`, before `render()` - and per-button handlers, L2+R2 among them), and the actions (what Cross does per state and menu icon, and L2+R2's system menu). Holds the `Carousel` as `carousel`. A black overlay fades out over `LauncherFadeInDuration` every time the screen is shown (`fadeAlpha`/`fadeStart`). `scanStatusLine` (bottom of the screen) shows the scan's progress or its "Scan complete" summary; `reloadGames()` re-runs the current set's query and re-selects the same game by id whenever the roster changed and no scroll animation is running; a highlighted game that vanished (folder pulled, or merged by the scan) falls back to the set's first game, closes a resume-slot picker that was showing its slots, and keeps the cover raised while the game menu is open (`Carousel::snapMainCover`). |
| `evoui/screens/evoui_system_menu.*` | `GuiSystemMenu` | The L2+R2 overlay (either order - `powerOffShift` + `r2Held`, 2026-09-18): Re-Scan Games, RetroArch/EmulationStation, Memory Cards, Game Manager, Hardware Information (PSC-Bios on the console, `GuiHardwareInfo` elsewhere), Options, About, Power Off - everything the classic main menu used to offer, Power Off included (no more direct L2+R2 shutdown). A dumb picker: a panel over the launcher's dimmed background (`background` texture handed in by the launcher), launcher fonts and theme colours, the panel as tall as its rows need and scrolling with edge markers when more than fit, the launcher's X/O hint icons in its footer; Up/Down + wrap, Cross/Circle - it returns a `SystemMenuAction` and `GuiLauncher::loop_openSystemMenu()` runs it. |
| `evoui/carousel.*`, `carousel_game.*` | `Carousel`, `PsCarouselGame` | **Two kinds of box** (2026-09-18): a PS1 game is the art in the theme's jewel case (`cdJewel`, thin - `JewelCaseThickness` 8%); a RetroArch game or an App is a **big box** - the art at its own aspect (tall NES, wide SNES) with `evoimg/bigbox.png` laid over it as a 9-slice (`drawNineSlice`, 7 px border; `tools/make_bigbox_frame.py` draws the file, replace it with real artwork any time) and `BigBoxThickness` 22% deep. `PsCarouselGame::content` is where the box is in the 226x226 texture and `thickness` its depth; `renderTurnedCover` turns the box about *that* rect and puts the spine on its edge, so a tall box no longer has its spine floating in the transparent part of the texture. The row of covers: `games` (exactly the set's games, a bounded row - see "Conventions"), `selected`, the 13 screen positions, the scroll/moveMainCover animations, texture load/free on visibility, `render()`. **Cover flow** since 2026-09-18: `PsScreenpoint::angle` (degrees about the vertical axis, negative = left of the middle, facing in) is interpolated like x/y/scale; `PsCarousel::createCoverPoint(distance, side)` lays out the `PsCarousel::SideCovers` (14 - enough that the outermost slot is off a 1280-wide screen, so a cover scrolls in from the edge rather than popping up) slots a side as a shelf receding from the middle: the nearest at half size 190 px out, each further one 3.5% smaller, 15 shades darker, a step (50 px, scaled with the cover) further out and turned more (40..72°) - the shrinking is what makes an inner cover drawn over an outer one read as being in front of it; `render()` draws far-to-near, the selected cover as a plain copy and every turned one via `renderTurnedCover()` - front face through `Renderer::copyTrapezoid`, plus a spine (`CoverThickness` = 8% of the width, textured with a strip from the cover's near edge, darker) and a Lambert-ish darkening with the turn. `ViewerDistance` (600 px) is the perspective strength. |
| `evoui/controls/evoui_*.{h,cpp}` | `PsObj` and subclasses | The EvolutionUI controls: the animated elements the launcher is built from (`PsObj` base, meta panel, menu, buttons, labels, the state selector). Class names keep their `Ps` prefix. `PsMeta` shows a RetroArch game the database knows as title / "publisher, year" / core / "n Players" (2026-09-19); one it does not know as title / core, as before. |
| `core/model/ps_game.*` | `PsGame : ableem::GameRecord` | Game as seen by the UI (from DB via `PsGame::fromRecords`, or playlist). `PsGamePtr = shared_ptr<PsGame>`. Adds the RetroArch/App fields. A plain data record - the resume points are `ResumePointService`'s, the memcard `MemcardService`'s. |
| `core/services/game_catalog.*` | `GameCatalogService` | The writes: play history ranking, game delete, cover flush. Owned by `App` (`app.gameCatalog()`). |
| `core/services/resume_point.*` | `ResumePointService` | The save-state slots in a game's `!SaveStates` folder, and the prepare/save around a PCSX launch. Owned by `App` (`app.resumePoints()`); non-screens reach it via `App::get()`. |
| `core/services/memcard.*` | `MemcardService` | The `!MemCards` sets and a game's chosen card; the swap in/out around a launch. Owned by `App` (`app.memcards()`). |
| `core/services/game_settings.*` | `GameSettingsService` | The game editor's model: a game's Game.ini flags and pcsx.cfg values, read with `open()` and written one setter per option. Owned by `App` (`app.gameSettings()`). |
| `core/services/game_query.*` | `GameQueryService` | Which games a set shows and in what order - `gamesFor(selection)` is the whole of the old `switchSet` query. Owned by `App` (`app.gameQuery()`); RetroArch arrives through the `RetroArchGames` interface. |
| `core/services/retroarch.*` | `RetroArchService` | RetroArch's playlists as sets of foreign `PsGame`s: `.lpl` parsing (both formats via `ableem::RetroArchPlaylist`), the core for an entry from its `ableem::CoreInfoTable` (`info/*.info` + `platform/<platform>.cores.cfg`, `coresCfgPath()`), Favorites/History, `reloadPlaylists()` after the scan rewrote them, and `ensureMetadata()` - publisher/year/players from `<rdb dir>/<playlist>.rdb` by label, read once per playlist on first use and dropped again (Favorites/History copy from the source playlist). Implements `RetroArchGames`. Owned by `App` (`app.retroArch()`). |
| `core/services/launch.*`, `process_runner.*` | `LaunchService`, `ProcessRunner` | A game launch start to finish: argv for `rc/launch.sh` (PCSX) / `rc/launch_rb.sh` (RetroArch) / an App's `startup`, the memcard and resume-point work around it, the RetroArch config transfer, `writeSelectionScript()`. Runs through a `ProcessRunner`. Owned by `App` (`app.launcher()`). |
| `evoui/screens/evoui_mc_manager.*`, `evoui_app_start.*`, `evoui_btn_guide.*` | | Launcher sub-screens. |
| `evoui/controls/evoui_notification_line.*` | `NotificationLines` | The two timed text lines at the top of the launcher. |

| `apps/pscbios/`, `apps/abflashkit/` | `PscBios`, `AbFlashKit` | The console tools (see "Console tools"), each with its own CLAUDE.md, a `<tool>_core` library and a program on `ab_classic`. |

Payload (`payload/`): the release USB tree — `rc/*.sh` scripts, themes (`ab2`, `aergb`, `autobleem`,
`default`, `evolution`), bundled Apps, release notes. `ab2`'s launcher menu icons (gear, gamepad, memory card,
the save-state frame - which must keep its 68x52 window at (25, 33), where `PsMenu::render` pastes the picture)
and its blue `on.png`/`off.png` switch are drawn by `tools/make_ab2_icons.py` (2026-09-18); the tile sits high in
the 118 slot so it clears the footer bar in the launcher's Games state. Where the resume icon takes the picture is
the theme's `launcher.menuIcons.resumePicture` (`ThemeRect`, unset = the original (25, 33) 68x52); ab2 centres it on
its tile. ab2's classic font is **Selawik Light** (`selawik-light.ttf`, OFL, Microsoft's open metric-compatible
replacement for Segoe UI) since 2026-09-18 - `sul.ttf` was Segoe UI Light itself, not redistributable and with its
`(` `)` cut out; the console's SST fonts and Typodermic's Zrnic in the other themes are as they always were. `payload_rpi/` next to it is the Raspberry Pi installer
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
- The carousel is a bounded row (2026-09-18): exactly the games given, nothing before the first or after the
  last, a scroll past either end refused (`Carousel::canSelectNext/Previous`, checked by the launcher's
  `next/prevCarouselGame`, which also stop a held stick there). It used to repeat a short list to fill the
  13 slots and wrap around, which showed the same few games over and over. The slots past the ends are
  not bare, though: **empty boxes** (`PsCarouselGame::emptyBox()`, `Carousel::leftFill/rightFill`) stand
  in them and scroll with the games - an empty jewel case or an empty big box (`BoxKind`, from the first
  game in the row, or the set for an empty row: `setGames(games, kind)`), one shared texture per row
  (`loadPlaceholderTex`: the case/frame over a dark translucent inside), drawn at 55% shade and alpha 150
  (`PlaceholderShade/Alpha` in `carousel.cpp`). An empty set is a shelf of empty boxes.
- SDL lifecycle: `TTF_Init`/`Mix_Init` once in `GuiBase`, `SDL_Quit` registered with `atexit` in `main` so it
  runs after the `Gui` singleton is destroyed. Audio is fully closed (`Mix_CloseAudio` loop) before forking PCSX.
- Logging (2026-09-18): `PLOG_INFO/WARNING/ERROR/DEBUG` (plog, vendored header-only under
  `lib_ableem/third_party/plog`, behind `<ableem/engine/log.h>`). `main()` calls
  `ableem::Log::initConsoleOnly()` first thing - a `PLOG_*` before any init is silently dropped, which is
  how a bad command line's USAGE line used to vanish - and `Log::addFile()` once the logs directory is
  known; the tests' main stops at `initConsoleOnly()`. Every line goes to stdout *and* to
  `System/Logs/autobleem.log`, rolling 1 MB x 3, as `HH:MM:SS LEVEL [function:line] message`; the file's
  first line is the build (`Version::FULL_VERSION`). **There is no `cout` anywhere** - the screens, the ui
  library and the demo log the same way (diagnostic chatter is `PLOG_DEBUG`, off in release builds);
  `tools/theme_convert`'s `cout` is that CLI's output, not a log. Console `stdout`/`stderr` still go to
  `System/Logs/AB_*.txt` and are unit-buffered so the last lines survive a crash. A `PLOG_*` inside an
  unbraced `if` wants braces (the macro is itself an if/else; `-Wdangling-else` says so).
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
  clang-format settles the rest (`tools/format.sh`); `override` on every overrider, `explicit` on every
  single-argument constructor, `static_cast` not C casts, `= default` for a trivial special member,
  `make_unique`/`make_shared` over `reset(new ...)` - clang-tidy (`tools/lint.sh`) flags each of these.

## Git

Repo was `git init`ed on 2026-09-15 from the final source snapshot (no upstream history here; the public
history is at github.com/screemerpl/cbleemsync). Commit per logical refactor step. `.gitattributes` forces LF.
