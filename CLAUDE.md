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

`ResumePointService` (`core/services/resume_point.*`) owns the save-state slots in a game's `ssFolder` -
**`Games/!SaveStates/<game folder name>/`** (internal games: `/<id>/`), central since 1.x and keyed by the
folder's *name*, so a game moved into a sub-folder keeps its states and its own card (2026-09-21, see
`docs/legacy-1x-analysis.md`) - `slotIsActive`/`pictureForSlot`/`lastPicture`/`storePictureForSlot`/`removeSlot`/
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
   `autobleem-gui` cross-compiles and links cleanly with the Sony GCC 8.2 toolchain. Superseded on
   2026-09-19 by the Docker image's Stretch/gcc-6 toolchain (see "CI" under Build), and **that build has run
   on a console**: the launcher came up on the owner's PSC (stock kernel, a FAT32 stick) with every cover in
   place, PS1 and RetroArch alike. **The full console pass happened on 2026-09-19** (a stick made in a
   Claude Desktop session, the owner at the console): after the two fixes noted under Build, sound (ALSA),
   a PS1 launch through the image's pcsx-ab and back, a Mega Drive game in RetroArch (RetroBoot 1.2) with
   the RetroArch set from playlists the offline ROM scan wrote on the PC (`UpdateRoms.exe` - the console
   itself never fetches), and both console tools (pscbios, abflashkit) from the system menu - all as
   expected. The console is no longer the untested target.
5. Features. Done on 2026-09-17: **themes are `theme.json`** (`docs/theme-format.md`). `ableem::ThemeSpec` is
   the typed theme (engine, JSON in/out, partial-over-default merge, per-file fallback), `ThemeConverter`
   (`core/services/theme_converter.*`) turns an old `theme.ini` + PSC-data-tree folder into the new layout in
   place - `Theme::load()` does it on first contact, `tools/theme_convert` ahead of time - and `payload/Themes`
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
libretro box arts mirrored by `payload_linux/install.sh --thumbnails`; Phase 2, the multi-disc folder merge
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
`payload_linux/Autobleem/bin/emu/`, which is checked in like the console's `payload/Autobleem/bin/emu/`), and the
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
- **`AB_PLATFORM_RPI`** (`core/services/environment.h`), from `-DAB_TARGET=rpi` (the toolchain file forces
  it; see "The platform model" under Build). A Pi is a *real* target, not an `AB_DEBUG_HOST`: it forks
  emulators and halts for real. What `AB_APPLIANCE` (rpi or pcusb) changes: `GameQueryService::
  showInternalGames()` is hard `false` and the "Show Internal Games" row is gone from the Options menu (no
  `/gaadata`; `AB_HAS_INTERNAL_GAMES` is psc/dev only); `backup_internal.sh` is the console's alone.
  `AB_ROOT_RELATIVE_LAYOUT` (everything but the console) is what selects `EnvironmentSetup`'s "everything
  under the root given on the command line" branch, which the Pi shares with the 1-arg debug mode.
  `internal.db` is still *opened* on a Pi (it comes up empty) because `GameCatalogService`/
  `GameSettingsService` unconditionally expect the handle.
- Root `CMakeLists.txt`'s console branch is PSC-specific (it overwrites `CMAKE_CXX_FLAGS` with
  `-march=armv8-a+simd` and adds `/opt/toolchain/armv8-sony-...` to the include path), so the appliances
  (`rpi`, `pcusb`) take their own branch there, flags from the toolchain file. Without that the Pi build was
  getting armv8 code, which would SIGILL on a Pi 2.
- **Package**: `payload_linux/` is a sibling of `payload/`, not inside it, and is laid out as the package the
  Pi unpacks: `install.sh` + `README.md` at the top, `system/` for the host-side files (systemd unit, the
  `autobleem-session.sh` loop that replaces `rc/selection.sh`, the two shrink-root initramfs pieces, the plymouth theme), and the data-partition
  tree exactly as it lands on the exFAT partition - `Autobleem/rc/` (the Pi `launch.sh`/`launch_rb.sh`/
  `retroarch.sh`), `Autobleem/bin/emu/`, `Games/`, `Apps/` (empty dirs kept by `placeholder` files).
  `tools/make_rpi_package.sh` copies that tree, fills in `Autobleem/bin/autobleem` (`build_rpi/autobleem-gui`
  + `src/resources`, minus `internal.db`), `Autobleem/bin/db` (`db/covers*.db`) and `themes/`
  (`payload/Themes`), strips the placeholders, and tars it to `build_rpi/autobleem-rpi.tar.gz`. `install.sh`
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
  the handover is not a modeset. The AutoBleem logo is a plymouth `script` theme, `payload_linux/system/plymouth/`
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
- **BIOS pack** (2026-09-18, per-architecture manifest 2026-09-19): `install.sh`'s `download_bios_pack()`
  fetches `system/biospack.txt` (armhf) or `system/biospack-arm64.txt` (arm64, picked by `$ARCH`) into
  `RetroArch/system/` - one line per file, `<sha256> <size> <url> <path>`, wget + `sha256sum` per file, a
  `.part` renamed once the hash checks out, files already right are skipped (so a re-run only repairs) - and
  `install_ps1_bios()` copies SCPH-5501/5500 to `System/Bios/romw.bin`/`romJP.bin` for pcsx-ab unless the
  user's own are there. `--no-bios` skips both; `--no-downloads` does not. Both manifests are written by
  **`tools/biospack.py --arch armhf|arm64`** from [RetroBIOS](https://github.com/Abdess/retrobios)
  (`install/retroarch.json`, pinned to one commit in `RETROBIOS_REF`) filtered to the cores that
  architecture actually has, minus `mame`, an allow-list of `Vendor/System` folders (everything
  `RA_ROM_SYSTEMS` has a folder for - consoles, handhelds and the Amiga/C64/MSX/Spectrum/PC-98/X68000
  computers - plus arcade, Neo Geo CD, ScummVM, Doom/Wolfenstein engine data) and path excludes (arcade
  `samples/`, MAME's `history/mameinfo/cheat.dat`, stella's `.wav`, x86 `.dll/.so/.dylib`, `dc/`,
  `kronos/`). **Where the core list comes from differs by architecture**: armhf reads RetroBIOS's own
  `install/targets/retroarch.json` (`"linux-armhf"` - 109 cores); arm64 has no matching entry there (only
  `android-arm64-v8a`/`osx-arm64`/`ios-arm64`, none of them this target), so it reads the real listing at
  `buildbot.libretro.com/nightly/linux/aarch64/latest/.index-extended` instead - the same URL
  `download_retroarch_content()` itself downloads cores from - via `biospack.py`'s `buildbot_cores()`. 222
  cores there vs 109 for armhf, so the arm64 pack is bigger: 694 files/230 MB vs 647 files/188 MB (both
  numbers as of the pinned commit; `--list` reprints them live) against 5.8 GB for RetroBIOS's whole
  RetroArch pack. **`buildbot.libretro.com`'s directory is `linux/aarch64`, not `linux/arm64`** - Debian's
  `dpkg --print-architecture` says `arm64`, the buildbot's own path segment does not match it; `install.sh`
  maps one to the other (`RA_ARCH=aarch64` when `$ARCH` is `arm64`) for both the cores download and the BIOS
  pack. `RA_ROM_SYSTEMS` names are RetroArch's rdb names (`database/rdb/*.rdb`) so a scan of `roms/` lands
  in the right playlist; `disksys.rom` (FDS) lives in RetroBIOS's `Arcade/FBNeo` folder, and DOS has a
  folder but no files (its only "BIOS" entries are x86 MIDI libraries).
  `--list` shows what is in and out, `--check DIR` verifies a `system/` folder. **No BIOS file is in this
  repository** - only their hashes and URLs.
- **Other systems run** (2026-09-18): ROMs in `RetroArch/roms/<system>/`, scanned by RetroArch's own Import
  Content (guide: `payload_linux/README.md`, "Games for the other systems"), launched from the RetroArch set
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
  database knows. On the Pi: 679 of 848 named in ~2 s. **Step 3** (2026-09-19): `OnlineAssets`
  (`core/services/online_assets.*`) fetches, through the platform ini's `download_command` (`curl -sfL
  -m 20 ...` on the PC and the Pi, nothing on the console), the databases bundle when `database/rdb/`
  is empty and the box art of every ROM without one - only with config.ini `online=true` (Options: "Fetch
  box art online"), after one probe per cycle, and only when there is something to fetch; a server miss
  is remembered in `Named_Boxarts/.autobleem-missing.txt`. Pi: 36 covers fetched, 31 not on the server.
  **PS1 games get the same since 2026-09-19** (`ScanService::fetchMissingPs1BoxArt`, after
  `scanGamesDirectory`): a game with no PNG next to it and nothing in the thumbnails tree is asked for by
  its rdb record name (libretro-thumbnails' file name), else its title, and its Game.ini gets the cached
  path at once; `OnlineAssets::fetchMissingBoxArt` takes `(database, label)` requests now, with an
  on-fetched callback, and `ps1Requests()` is the selection (both tested). `config.ini`'s `online`
  defaults to **true**; the gate is the platform's `download_command` (Pi, PC - never the console). With
  that, `install.sh --thumbnails` defaults to `none` - the ~9300-file mirror (now four streams with a
  percentage) is `--thumbnails boxarts`, for covers offline. The owner first said no to this and then
  yes, for network-guaranteed platforms only.
  **Step 5** (2026-09-19): `apps/updateroms/` - `UpdateRoms.exe`, run from the stick in a PC, does the
  same scan with the PC's network and the target's paths (`usb_root` in the platform inis); see its
  CLAUDE.md. **The plan is complete**; what is left is listed at the end of the plan doc. On a console
  (2026-09-19): the RetroArch set from playlists `UpdateRoms.exe` wrote on the PC works as expected; the
  console's own scan stays offline by design (no `download_command` in `psc.ini`).
- Two gotchas the port turned up. `System::getAvailableSpace()` called a `floatToString()` that **has never
  existed anywhere in the code base** - the whole `#ifndef AB_DEBUG_HOST` branch had simply never been
  compiled, because no ARM build had ever run. Fixed with a file-local helper. And `config.ini`'s `Cfg=` key
  was an absolute console path that the installer had to rewrite per install - gone since 2026-09-18, the
  selection script is `Env::getPathToRCDir() + autobleem_cfg.sh` on every platform.

**Flashable image for Raspberry Pi Imager** (2026-09-19, plan at `docs/rpi-image-and-update-plan.md`). A
second way to get AutoBleem onto a Pi, alongside the tarball + `install.sh` flow: `tools/make_rpi_image.sh`
takes an official Raspberry Pi OS Lite image (downloaded automatically per architecture from Raspberry Pi
Foundation's stable "latest" redirect, sha256-verified against its published checksum, or a local `--base`),
loop-mounts it and injects an AutoBleem package plus `payload_linux/system/autobleem-firstboot.{service,sh}`
onto its root filesystem under `/opt/autobleem-image/` - **injection only**, deliberately: no chroot, no
qemu, no package pre-install, nothing from the base image is ever executed at build time (`systemctl
enable`'s effect is one hand-crafted symlink instead), so the same mechanism works for either architecture
from either architecture's build host. Two edits on the boot partition: `cmdline.txt` loses the word
**`resize`** - on Trixie that is what the initramfs (`local-premount/resize_early`, and `set_partuuid`)
keys on to grow the root over the whole card, which would leave `install.sh` no room for the data
partition - and **`autobleem.txt`** (from `payload_linux/system/`) is added: AutoBleem's first-boot options
as `key=value`, editable from any PC (`root_gib` default 8, `hdmi_mode`, `retroarch`, `thumbnails`, `bios`,
`downloads`; CRLF/BOM tolerated), and **ssh is enabled** (the owner's rule, 2026-09-20: once installed the launcher owns
tty1 and the keyboard, so there is no console to enable it from - the `multi-user.target.wants/ssh.service`
symlink in the root plus the official empty `ssh` file on the boot partition; the host keys are made by Pi OS's
own `regenerate_ssh_host_keys.service`). cloud-init's `user-data`/`network-config`/`meta-data` are left exactly as
shipped, so Raspberry Pi Imager's OS customisation (this base image is cloud-init 25.2 + rpi-cloud-init-mods:
`init_format: cloudinit-rpi`) lands on top as on a stock image. **`install.sh --grow-root GIB`** is the
counterpart to `--shrink-root`: `sfdisk --no-reread --force -N` + `partx -u` + `resize2fs` grow the still
image-sized root online (proven on a mounted loop-device filesystem on the Pi), capped so 2 GiB stay for the
data partition, run before apt (a fresh Lite root has ~400 MB free) and skipped when a data partition exists.
The output is `autobleem-<version>-rpi-<arch>.img.xz`: `make_rpi_package.sh` writes a `VERSION` file into the
tarball from the build's generated `version.h` (the tag, plus short hash and `-dirty` unless clean and exactly
at the tag), `make_rpi_image.sh` reads it from the tarball (`--version` overrides) and also puts it in the
Imager JSON description. (Found on the way: `generate_version.cmake`'s `git diff-index` stamped clean trees
dirty until an `update-index --refresh` was added before it - stale stat info, worse with MSYS2's git and
Git for Windows sharing one checkout.) **RetroArch is a question on the first boot** (owner's rule, see the
memory note: optional everywhere): unless `autobleem.txt` says `retroarch=`, the script asks, a minute's
silence means yes; `n` means `--retroarch none --no-downloads`, and `install.sh` makes that a lean PS1-only
install - `download_thumbnails` is its own step (box art is the launcher's, not RetroArch's) and the BIOS
pack shrinks to `scph5501.bin`/`scph5500.bin`.
**The first boot has a screen** (2026-09-20, the owner's ask): `payload_linux/system/autobleem-install-ui.py`
draws on `/dev/fb0` (RGB565 or XRGB, from sysfs) with nothing but python3's stdlib - it decodes the plymouth
`splash.png` itself (a small PNG reader), renders text from the console's Terminus PSF fonts
(`/usr/share/consolefonts`, PSF1/2 with their unicode tables) and puts tty8 in `KD_GRAPHICS`. The logo,
"Setting up AutoBleem", bar 1 = the phase (from `@@phase N/9 text` lines `install.sh`'s `phase()` prints
with `AB_UI_MARKERS=1`), bar 2 = the last percentage seen in the output (the download loops, wget) or a
pulse, and a box with the last 8 lines (a `\r` progress line rewrites the box's last line). The first-boot
script pipes `install.sh` through `tee` (the log) and the screen. **The questions are on the same screen**
(the owner's ask, the same day): `menu` / `input` / `message` modes draw a panel under the logo and read
keys raw from tty8 (termios; arrows, Enter, Esc = exit 3, a one-character item key as a hotkey, a countdown
to `--default`); the script's `ui_menu`/`ui_input`/`ui_message` wrappers call them and fall back to the old
text prompts without a framebuffer or when the program fails (any exit but 0/3 sets `UI_OK=0`). The console
stays in graphics mode from the first dialog to the reboot; `text_mode()` before a failure message. The
WiFi flow (country, scan list, hidden SSID, password, Ethernet - **no skip**, the owner's rule: the install
needs the network, the menu comes back until it is there) and the RetroArch question are all dialogs now. ~40 ms a frame on a PC; `--render out.ppm --fonts DIR` draws one frame on a PC for a look.
Every dialog is its own process, and decoding + scaling the splash PNG in pure Python cost ~1 s on the PC
and several seconds on the Pi - the owner pressed Enter again into a panel that had not changed - so
the prepared logo rows are cached in `splash.png.cache` next to the PNG (keyed on its size/mtime and the
framebuffer geometry; 22 ms warm) and a taken answer redraws the panel with a "Please wait..." footer
before the program exits (`accept()`); stale key presses are flushed when the next dialog opens.
The image build injects the script and the splash into `/opt/autobleem-image/`. Verified on the Pi 400:
the real installer through the progress screen, and the three dialog kinds with the Pi's keyboard.
`autobleem-firstboot.service` (`WantedBy=multi-user.target`, `ConditionPathExists=!/opt/autobleem-image/.done`,
`After=multi-user.target cloud-final.service userconfig.service`, `StandardInput/Output=tty` on
**`/dev/tty8`**, its own VT, switched to with `chvt 8` and back with `chvt 1`) **owns the screen and keyboard
for the first boot**, the way `autobleem.service` does later: the whole install is watched, not a silent
journal-only job. It deliberately does *not* `Conflicts=getty@tty1.service`: a `Wants=`-pulled unit whose
`Conflicts=` target is in the same boot transaction gets its job silently dropped (that first version never
ran at all - nothing in the journal, `ConditionResult=no` never evaluated). The script
waits up to 40 s for network; **with none it asks** - `rfkill unblock` + the WiFi country (default from
`cmdline.txt`'s `cfg80211.ieee80211_regdom=`, else the locale; Raspberry Pi OS keeps WiFi soft-blocked
until one is set, `raspi-config nonint do_wifi_country`), an `nmcli` scan listed by signal, pick / hidden
SSID / "I plugged in Ethernet" (skip removed 2026-09-20), password, `nmcli device wifi connect`, then a real fetch check -
then waits for NTP (`timedatectl ... NTPSynchronized`), then runs `install.sh --yes` + the `autobleem.txt`
options, output on tty8 and tee'd to `/var/log/autobleem-firstboot-install.log`. Failure or a skipped
network question switches back to tty1 (the login prompt) and retries on the next boot (a counter caps it at 20, then it
disables itself and leaves a note); success deletes the staged package, disables the unit and reboots once
more (boot splash and HDMI mode take effect on the boot after `install.sh` sets them). WiFi presets are not
an AutoBleem key on purpose - Imager's screen and the boot partition's own `network-config` already are
that. `tools/rpi_imager_repo.json` is the checked-in template for Imager's metadata; `make_rpi_image.sh`
writes a filled-in copy (real `extract_size`/`extract_sha256`/`image_download_size`/`image_download_sha256`/
`release_date`) to its output directory per architecture built, `url`/`icon`/`devices` left as placeholders
(no publishing pipeline yet). **`tools/rpi_imager_local_manifest.py`** turns that into what Imager's own
`doc/local_json/create_local_json.py` produces for local files - a `*.rpi-imager-manifest` with `file://`
URLs (double-click it, or App Options -> Content Repository -> Use custom file, or `--repo`) - which is what
makes Imager offer the customisation screen (user/WiFi/SSH) for a locally built image. `payload_linux/README.md`'s
"Flashing with Raspberry Pi Imager" section has the walkthrough.

**The download repository** (2026-09-19; its plan, `docs/repo-server-plan.md`, was removed on 2026-09-20 when
every step was done - the git log has it): **`https://autobleem.retromenele.pl/`**,
the build server's `/home/claude/autobleem-repo` served by a Caddy container (`docker/repo/`) on 443 (a
Let's Encrypt certificate Caddy obtained by TLS-ALPN-01 once the owner's `A` record existed - the host's
nginx keeps port 80, and no root was needed; Caddy renews it) and on 9090 as plain HTTP
(`http://212.71.244.78:9090/`, the same tree). Read-only to the world; publishing is `tools/repo_publish.sh
<release|image|retroarch|db|assets|index> ...` - rsync over `ssh psc-build` (or `--local` on the server),
`.sha256` sidecars, then `tools/repo_index.py` rerun on the server from `<repo>/.tools/` (uploaded with every
publish, but only when its `INDEX_VERSION` is at least the published one's - a `--local` run from the
server's stale rsync tree once regenerated the page without the manual link). The tree:
`releases/<tag>/` (the five packages, `SHA256SUMS`, `release.json`) with `releases/latest.json` (newest
stable) and `unstable.json` (the one pre-release), `rpi-imager/os_list.json` + `images/<v>/` (Raspberry Pi
Imager's "Add repository" URL - `<repo>/rpi-imager/os_list.json` - with the `rpi_imager_repo.json`
placeholders filled in), `rpi/retroarch/<tag>/` + `latest.json`, **`psc/retroarch/<tag>/` + `latest.json`** (2026-09-20: the
console's RetroArch from `github.com/autobleem/retroarch-psc` - its `make publish` runs `repo_publish.sh
psc-retroarch <tag> retroarch-psc-<tag>.zip manifest.json`; the tag is `v<RetroArch version>-<build>`,
`psc_version_key` orders it, the newest kept as for the Pi builds), **`psc/cores/cores-psc-<date>.tar.gz`**
+ `.json` + `latest.json` (the console's cores, `repo_publish.sh psc-cores`, newest date kept - see
"RetroArch for the console" below), **`psc/libs/`** and **`psc/apps/`** (the same shape: `libs-psc-<date>.tar.gz`,
the libraries and the xpad module for `Autobleem/lib/`, from retroarch-psc's `tools/pack_retroboot_libs.py`;
`apps-psc-<date>.tar.gz`, the eight third-party Apps as `Apps/<name>/`, from `tools/pack_psc_apps.py` over a
stick's Apps folder - `repo_publish.sh psc-libs|psc-apps`), **`emu/pcsx-abnxt/<version>/`** and **`emu/pcsx-ab/<version>/`** + `latest.json` each (2026-09-20: the two emulators' packages, one per platform, from each repo's `tools/make_packages.sh` - pcsx-abnxt's version is `git describe` (`r26-24-g0f4727f1`), pcsx-ab's `<date>-<hash>` (`20260920-fc8c992`, no tags there); `repo_publish.sh pcsx|pcsx-ab <version> dist/packages/*`; the newest version kept; `EMULATORS` in `repo_index.py` renders one panel each under "Every platform"), **`psc/bios/biospack.txt`** + `latest.json` (the console's BIOS *list* only - `repo_publish.sh psc-bios payload/RetroArch/bios/biospack.txt`; the installer fetches the files from RetroBIOS, the owner's rule: no BIOS file on the site), `db/` (the three cover databases), **`samples/`** (the sample-games pack, below), `assets/`. **Retention** (the owner's rules): a pre-release *replaces* the previous one (packages and image
sets alike - `repo_index.py` deletes the older ones), only the newest RetroArch build is kept, stable
releases stay. **Since INDEX_VERSION 22 a package kind the new pre-release does not bring is carried
over** from the one it replaces (a publish of the console's packages alone no longer drops the Pi's, and
the other way round - two sessions publishing into the pre-release did exactly that on 2026-09-20).

**The web pages are generated, not stored**: `tools/repo_index.py` holds `PAGE_CSS` and renders
`index.html` (the landing page, **by platform since 2026-09-20** - PlayStation Classic, Raspberry Pi, PC,
then "Every platform"; each platform is an *Install* panel (what a user installs from: the console's USB
package, the Pi **images**, the Windows launcher + UpdateRoms - the latest stable release and, under it,
the one pre-release marked as a development build) and a dashed *Build inputs* panel (what the
installers, the image build and the CI fetch: the RetroArch builds, the cores tarballs, the Pi tarball
with `install.sh`; the cover databases are the inputs every platform shares) - the owner's reading of the
tree, an image or a package is the artefact, everything else feeds one)
and `rpi-install.html` (the Pi manual: `RPI_MODELS` - which image for which Pi, 32-bit recommended because
pcsx-ab's dynarec is ARM32-only, 64-bit for the bigger core set; requirements; Imager steps; what the first
boot does; where games go - files dropped straight into `Games/` are sorted into folders by the scan;
options and updates). Styled after the **ab2 theme** at the owner's request: its `abback2.jpg` (the
AutoBleem 2 logo is painted into it; `ab.png` is blank) as the hero, the navy/cyan palette, Selawik Light -
staged as `assets/` from `payload/Themes/ab2` by `tools/repo_assets.py` (`tools/repo_icon.png` is the
emblem cut out for the favicon and Imager's icon, checked in because MSYS2's python has no Pillow). To
change the pages: edit the render functions, bump `INDEX_VERSION`, `tools/repo_publish.sh index`.
**The page generator is merged on every publish, never copied** (2026-09-20, after two sessions had
overwritten each other's page twice): `tools/repo_index_merge.py` three-way merges this checkout's
`tools/repo_index.py` with the copy the repository runs (`<repo>/.tools/repo_index.py`) over the older of
the two develop versions they started from - mine from `git merge-base HEAD origin/develop`, the
repository's stored next to its copy as `.tools/repo_index.base.py` + `.rev` - with `git merge-file`;
`repo_publish.sh` uploads the result and the newer base. Distinct lines combine (a panel one session added
survives the other's publish); the same lines changed on both sides are a conflict and **nothing is
published** - the file with the markers is left in a temp dir, resolve it, commit to develop, publish
again. `INDEX_VERSION` is informational now (the larger of the two, +1 when the merge changed the
repository's). The server keeps `.tools/repo_index.prev.py` and falls back to it when the merged copy
fails to run. A tree without git (the server's rsync trees) merges over the repository's stored base,
right as long as that tree is at least as new as it. Two lines conflict easily - `render_index`'s
signature and `main()`'s summary `print` - so a new panel is best committed to develop before the next
session publishes. (The PC USB stick's `pc/` panels landed on develop with the merge of 2026-09-20.) Two
things the first publishes through it showed: the server's python is 3.6, so the tool uses no
`capture_output`; and a **stored base older than a region both sides had added** conflicts on every
publish that touches that region, whichever side is right - the base only moves on a successful publish
**from a git checkout** (the server's rsync tree has none), and now moves even when the two copies are
already identical. So: land the page change on develop, publish from the PC checkout.
`AB_REPO_URL` is the base URL everything generated starts with.

It holds the cover databases, the images and **RetroArch v1.22.2 for armhf and arm64** - `ci/build_retroarch.sh`
cross-builds it in the Docker image (a `retroarch` stage after `psc` with the foreign-arch dev packages;
same configure as the installer's source build, no FLAC - its soname differs between Bookworm and Trixie;
`retroarch.version`/`retroarch.depends` under `usr/local/share/autobleem`), and **`install.sh --retroarch
prebuilt` is the default** (`--repo`, `autobleem.txt` `repo=`; falls back to the source build when the
repository is unreachable). Verified on the Pi 400 the same day: installed in a couple of minutes, plays a
NES game. **Since 2026-09-20 the images are built on the server too**, rootless: `docker/run.sh
tools/make_rpi_image.sh --arch armhf --package dist/rpi/autobleem-rpi.tar.gz --work build_rpi_image --out
build_rpi_image/out` after `ci/build.sh rpi rpi64` - `--rootless` (the default without root) does the
five writes into the ext4 root with `debugfs -w` on `<img>?offset=N` and the two boot files with `mcopy`
on `<img>@@N`, offsets read from the MBR; `--mount` is the old loop-mount way for a machine with root.
7 minutes per image at the default xz level 4 (12 at level 6 for ~2% less size), then
`tools/repo_publish.sh --local image ...` on the same machine - no Pi in the loop. The Pi 400 only *tests*
an image, on the owner's request, never as part of CI (there is no Pi in the cloud). The Pi package leaves
the cover databases out (`make_rpi_package.sh --with-covers` puts them back; `install.sh` fetches them
from the site's `db/`), and the installer takes the cores as one tarball (`rpi/cores/`, `ci/build_cores.sh`
- a download of buildbot's cores and bundles, no compiling) before falling back to buildbot's per-core
download. The `c3a684c` pre-release (the two Pi tarballs) and its image set are on the site. **CI feeds the
site, for the Pi only** (2026-09-20, the owner's scope): `ci.yml`'s `site` job on a `v*` tag publishes the
two Pi tarballs to `releases/<tag>/` and builds + publishes both images, rootless, on the self-hosted
runner (`/home/claude/autobleem-repo` mounted into the job container as `REPO_DIR`); `site-refresh.yml`
(monthly, or `workflow_dispatch`) rebuilds RetroArch and re-downloads the cores tarballs. Both are written
and unrun: nothing in Actions runs until the runner is registered (the owner's PAT, `docs/ci-plan.md`).
The console zip is not on the site and its cover databases still come from the Docker image's baked copy
(the console has no network, so the zip must carry them) - deliberately left as is.

**Sample games** (2026-09-20, the owner's ask: an install should not start with an empty shelf): `install.sh`'s
`install_sample_games()` (phase 8, after the payload; `--no-samples`, `autobleem.txt` `samples=no`) fetches
`samples/latest.json` from the site and unpacks the newest `samples-<date>.tar.gz` onto the data partition -
`Games/` and `SAMPLES.md` always, the `RetroArch/` part (roms + thumbnails) only with RetroArch installed -
once (`System/samples.txt` remembers it, so a re-run never puts back a sample the user deleted). The pack is
**`tools/build_samples.py`** over **`tools/samples/samples.json`**: each game's files from its upstream release
URL, sha256-checked (a zip member taken out), laid out as they land on the partition; only the stdlib, so it
runs in the Docker image; `tools/repo_publish.sh samples <tar.gz> <json>` publishes it (`repo_index.py`
keeps the newest date, `latest.json` carries the games list, the landing page's "Sample games" panel shows
them with licence links). **Licence first**: every entry names its licence and URL, and only games we may
*redistribute* went in - a free download is not enough. Today: Tetrade (PS1, MIT), Nova the Squirrel (NES,
GPL-3.0), Asteroids + Castle Platformer (SNES, MIT, undisbeliever), Alex vs Bus - The Race (Mega Drive,
GPL-3.0 + CC BY-SA assets, the `pre3` release) - and that is the set: the owner closed the list on
2026-09-20, no further sample games are planned. The PS1 game gets a **locked `Game.ini`** (`Automation=0`, the launcher's own lock: the scanner then
skips its create/update branch, so the title/publisher/year/players and the PNG next to the game stay - no
serial, no rdb, no covers db involved; verified on the PC: regional.db row `Tetrade / Logan Campbell / 2025 /
2`, ini untouched). The ROMs are named as the launcher's ROM scan labels a ROM no rdb knows (the file's stem)
with a box art of the same name under `RetroArch/thumbnails/<db>/Named_Boxarts/`. The covers are drawn by
`tools/samples/make_covers.py` (Pillow, so run on the PC and checked in as `tools/samples/covers/`): the
game's own title screenshot (`tools/samples/shots/`, from its repository) cropped to the box shape the
carousel draws for the system (square PS1, tall NES/MD, wide SNES), a navy band with the title in Selawik.
**Tetrade crashed pcsx-ab in the BIOS shell** (2026-09-20, the owner on the Pi: its custom boot logo - the
fork had upstream's "skip BIOS logos" jump commented out, so the shell always ran). Hence **the boot logo is
a per-game option**: pcsx-ab's `Config.SlowBoot` (pcsx-ab2 `377da70`; 1 = run the shell, the default on
every existing card; 0 = return into the kernel past it, as upstream always does), pcsx.cfg `SlowBoot`,
the game editor's "Boot logo" row (`GameSettingsService::setBootLogo`, `PcsxSettings::bootLogo`; no line =
shown), `LaunchService` passing it to RetroArch as `pcsx_rearmed_show_bios_bootlogo`, and the sample
pack shipping a `pcsx.cfg` next to the PS1 game (`skip_boot_logo` in the manifest -> `SlowBoot = 0`; the
Pi's `launch.sh` copies the game folder's cfg over the `!SaveStates` one at every launch, so it is what
pcsx-ab reads). With that, **`ConfigFileEditor::replaceProperty` appends a key the file does not have** -
it used to replace lines only, so a pcsx.cfg copied from an older default could never take a newer
option (the test that pinned that is flipped). The pack on the site was rebuilt in place (same date).
**Skipping the shell was not enough** (the same day, traced on the Pi 400 with a per-second pc/EPC print and
an I/O-write trace): the game booted and sat in PSn00bSDK's `DrawSync` forever, because **the 2013-era
core decoded the I/O registers by the literal `0x1f80....` addresses the PsyQ libraries use** - PSn00bSDK
reaches them through KSEG1 (`0xbf80....`), and every such write fell through `psxHwWrite16/32`'s switch to
plain memory: no GPU DMA ever started, no I_MASK write took effect. pcsx-ab2 `1feb7e2` masks the address
(`switch (add & 0x1fffffff)`, as upstream does); with it Tetrade boots with the interpreter and the
dynarec, through the shell and without it. Two more pcsx-ab changes from the same hunt: the real-BIOS
fast boot now **loads the executable itself** (`aeac3f9`, upstream's "manual booting" - SYSTEM.CNF, the
exe, pc/gp/sp - instead of the 2013 `pc = ra` jump, which relied on the kernel booting the CD), and
LoadCdrom logs it. The pack keeps `SlowBoot = 0` for Tetrade: its image has no licence data, so the shell
shows a garbled logo before the game. **Any PSn00bSDK homebrew was broken on pcsx-ab until this fix.**
**That fix was not the whole story**: with it the game boots, sets 320x240, runs its DMAs and IRQ acks
through the init, and then goes quiet after the ordering-table clear - never draws, the screen stays
black (RetroArch's current pcsx_rearmed plays it). The owner stopped the emulator work there
(2026-09-20): **Tetrade is out of the sample pack, and the pack has no PS1 game** - the four ROMs stay;
`build_samples.py` keeps its PSX layout code (locked Game.ini, pcsx.cfg with `SlowBoot = 0`) for the day a
PS1 homebrew with a redistributable licence, a disc image and a working boot on pcsx-ab turns up. The
KSEG1 and fast-boot changes stay in pcsx-ab (they are right regardless).

**Where the packages come from now**: the build server's Docker image (`docs/ci.md` - `ssh psc-build`,
`cd ~/autobleem`, `docker/run.sh ci/build.sh rpi rpi64`, `dist/<target>/`), which builds pcsx-ab from the same
run and bakes in the **real cover databases** (the PC checkout's `db/` holds 16 KB stubs, so a package made
on the PC ships no covers) - both Pi packages in ~2 min incremental. `~/autobleem` there is an rsync tree
(from the MSYS2 shell, *including* `payload*/`; pass `AB_GIT_*` for the version); its clock ran ~5 min behind
the PC, which made rsync'd files "future" and ninja loop ("manifest 'build.ninja' still dirty") - `touch` the
`-newermt now` files. The images themselves are built on the Pi 400 (loop mounts need root; the server has no
sudo), packages streamed server -> PC -> Pi, `--work/--out` on the data partition (the 8 GB root is too
small - the script checks free space first since it ran out once).

**What has run for real** (2026-09-19, Pi 400 as the build host over ssh, then as the target): the image
build itself for both architectures (download, verify, mount, inject, recompress; the *output* re-mounted
and its contents checked; a second run fills the other architecture's JSON entry alongside); two build-script
bugs found only against real hardware (`--dry-run` demanding `/sbin`-only tools; the wget redirect parsing
matching neither of wget 1.25.0's two `Location` line formats, so the download was named after the alias
URL and the `.sha256` check failed). **The first real boot of the first image** (arm64, "Use custom", no
presets) is what shaped the first-boot script above: the wizard asked for a keyboard layout, WiFi stayed
rfkill-blocked with no country set, `autobleem-firstboot` ran `install.sh --yes` silently in the background
with no network and died at `apt-get install` (`Temporary failure resolving 'deb.debian.org'`), leaving a
login prompt and no clue on screen - hence the script's own VT, the WiFi prompt, the NTP wait; and the root had
been grown over the whole card - hence the `resize` removal and `--grow-root`. `--grow-root`'s partition
mechanics were proven on a mounted loop-device filesystem on the Pi. **The second flash** (arm64, via the
local manifest with Imager's presets) went end to end: cloud-init applied the presets, the firstboot script
took the screen on tty8, asked the RetroArch question, grew the root 2400 -> 8192 MiB, made the 110 GB exFAT
data partition, built RetroArch, fetched cores + BIOS, rebooted into the launcher. Both `933bd2f` images
(armhf and arm64, from the server's Docker packages) were then built on the Pi, re-mounted and checked, and
sit in `build_rpi_image/` on the PC with a two-entry `os_list_local.rpi-imager-manifest`. **The first armhf flash** (2026-09-19, no presets) answered the WiFi question - the prompt works - and then
died unpacking the package: the 306 MB tarball is staged on the still image-sized root and unpacks to 327 MB
(290 MB of cover databases), *before* `--grow-root` had run; arm64 had just enough free to get away with it.
`autobleem-firstboot.sh` now extracts `install.sh` alone, runs it with `--grow-root N --grow-only` (preflight +
grow, then stop - a no-op on a retry), checks the free space against `gzip -l`'s unpacked size, and only
then unpacks the rest, behind an `.extracted` marker (a half-unpacked tree from a failed attempt is removed,
not run). **Verified the same evening on that card**: the root grown by hand to 8 GiB over ssh, the fixed
script and a re-packed tarball dropped into `/opt/autobleem-image/`, and the retry went end to end -
`--grow-only` a no-op, the package unpacked, RetroArch built (~25 min), cores, BIOS, splash, reboot into
the launcher on the 109 GB data partition. So the armhf image's first boot is proven apart from the
`--grow-only` growing a root for real (it has only ever found one already grown) - that is what the next
image build from `develop` checks. A 32-bit Trixie root also makes a 2 GB `/var/swap` (dphys-swapfile)
on its first boot with room; `root_gib=8` covers it.

### Raspberry Pi 64-bit (2026-09-18)

A second Pi architecture, alongside the 32-bit port above, not a separate app target: `AB_PLATFORM_RPI` and
`AB_TARGET_RPI` do not branch on word size, so every application-level behaviour (no internal games,
root-relative layout, install-tree paths) is identical on both. Only the toolchain, three build-system
files, and the checked-in pcsx-ab binary differ. **Builds cleanly, unrun on hardware** - `make_rpi64.sh`,
`pcsx-rearmed-develop`'s `make_rpi64.sh` and `make_rpi_package.sh --arch arm64` all verified end to end
2026-09-18/19 (real aarch64 ELF binaries, a working `autobleem-rpi-arm64.tar.gz`); the owner has no 64-bit
Pi OS card imaged yet, so nothing has booted on real hardware. Targets 64-bit **Trixie**, same as the 32-bit
port targets 32-bit Trixie (and Bookworm).

- **Toolchain**: `toolchains/rpi64/RPi64toolchain.cmake`, over "SysGCC for Raspberry Pi (64-bit)" -
  gnutoolchains.com/raspberry64 (Sysprogs OÜ, the same vendor family as the 32-bit `C:\sysGCC\raspberry`;
  sysprogs.com's own site says it has no 64-bit Pi toolchain, but gnutoolchains.com's free prebuilt one
  does - GCC 14.2.0, built against `2025-12-04-raspios-trixie`, same GCC version as the 32-bit toolchain).
  Installed 2026-09-18 to **`E:\sysGCC\raspberry64`** (the owner's call - not `C:`, unlike the 32-bit one),
  via `raspberry64-gcc14.2.0.exe /S /D=E:\sysGCC\raspberry64` (its NSIS installer takes silent-install
  flags). Same compiler/sysroot layout as the 32-bit toolchain assumed:
  `aarch64-linux-gnu-{gcc,g++,strip}.exe` in `bin/`, sysroot at `<root>/aarch64-linux-gnu/sysroot` with
  SDL2/SDL2_image/SDL2_mixer/SDL2_ttf/libpng16 runtime `.so`s under `usr/lib/aarch64-linux-gnu`.
  `./make_rpi64.sh` configures and builds into `build_rpi64/` (`--debug` -> `build_rpi64_dbg/`; incremental, `--clean` wipes), mirroring
  `make_rpi.sh` exactly. Target is plain `armv8-a` - every 64-bit-capable Pi (3/4/5/400/Zero 2 W) is that
  core, so there is no armv7-style board split to make.
- **SDL2 discovery**: `toolchains/rpi64/cmake/FindSDL2.cmake` is the same borrowed-headers-plus-imported-.so
  trick as the 32-bit module, and *reuses* `toolchains/rpi/sdl2-devkit/include` by relative path rather than
  keeping a second copy - the public SDL2 headers are pure C and arch-independent. Only the library
  directory differs: the sysroot's `usr/lib/aarch64-linux-gnu` instead of `usr/lib/arm-linux-gnueabihf`.
- Root `CMakeLists.txt`'s `ABLEEM_EMBEDDED_TARGET` switch matched `CMAKE_SYSTEM_PROCESSOR MATCHES "^arm"`,
  which `aarch64` does not match - fixed to also check `STREQUAL "aarch64"` so cursor-grab/keyboard-as-pad
  are disabled on a 64-bit Pi the same as everywhere else embedded. No other CMakeLists.txt change was
  needed: the PSC's `^arm`-and-`NOT AB_TARGET_RPI` branch and the Pi's own `elseif (AB_TARGET_RPI)` branch
  both already keyed off `AB_TARGET_RPI` rather than the processor string.
- **pcsx-ab has no aarch64 dynarec** in this fork (verified: no repo-authored aarch64 anywhere in
  `pcsx-rearmed-develop`, and Ari64's dynarec/NEON GPU-GTE assembly is 32-bit-ARM-only) - PS1 games on a
  64-bit Pi run through its C interpreter, like the PC build, not the NEON dynarec the 32-bit Pi gets. Still
  correct, just slower per clock. `pcsx-rearmed-develop`'s `CMakeLists.txt:35-39` sets `_pcsxab_is_arm` from
  `CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|ARM)"`, which is deliberately **not** extended to aarch64 for this
  reason - folding aarch64 into that branch would try to build 32-bit ARM assembly with the 64-bit compiler.
  Its own `toolchains/rpi64/RPi64toolchain.cmake` and `make_rpi64.sh` (mirroring the 32-bit ones there) build
  it as a plain aarch64 Linux target instead, `PCSXAB_GLES`/dynarec left off. Built 2026-09-19: a real
  aarch64 `pcsx-ab` + the three plugins, committed into `emu-arm64/` below.
- **Package**: one `payload_linux/` tree still serves both architectures - `install.sh` reads
  `dpkg --print-architecture` (`armhf` or `arm64`) into `$ARCH`, and separately into `$RA_ARCH` (`armhf` or
  **`aarch64`**, not `arm64` - buildbot.libretro.com's own directory name for 64-bit ARM does not match
  Debian's; caught and fixed 2026-09-19 before it shipped, `nightly/linux/arm64/` 404s) for the
  `buildbot.libretro.com/nightly/linux/$RA_ARCH/latest` cores URL. Only the emulator binaries are
  architecture-specific, so pcsx-ab (with its `plugins/`) is checked in twice: `payload_linux/Autobleem/bin/emu/` (armhf) and
  `payload_linux/Autobleem/bin/emu-arm64/` (arm64, built and committed 2026-09-19).
  `tools/make_rpi_package.sh --arch armhf|arm64` (armhf is the default, unchanged output name)
  picks the matching `build_rpi`/`build_rpi64` source directory and, for `arm64`, moves `emu-arm64/`'s
  contents over `emu/` while staging so the on-device path stays `Autobleem/bin/emu/pcsx-ab` either way; the
  tarball is named `autobleem-rpi.tar.gz` (armhf) or `autobleem-rpi-arm64.tar.gz` (arm64) so the two never
  collide on the Pi's home directory during `--push`. Both verified 2026-09-19 - `autobleem-gui` packs with
  UPX as `linux/arm64` (1.2 MB), the 32-bit package rebuilt clean alongside it (no regression).
- **BIOS pack** (2026-09-19): `tools/biospack.py --arch arm64` builds `payload_linux/system/biospack-arm64.txt`
  (694 files, 230 MB) from the real `buildbot.libretro.com/nightly/linux/aarch64` core listing, since
  RetroBIOS's own per-target file has no 64-bit Linux entry to read instead - see "BIOS pack" under the
  32-bit section above for the full mechanism, now shared by both architectures. Spot-verified: downloaded
  and hashed 4 random entries against the manifest, all matched.
- **A pre-existing repo bug this work turned up** (fixed on `develop`, 2026-09-18, unrelated to word size):
  `.gitignore`'s bare `build/` line matched at any depth, silently excluding
  `lib_ableem/third_party/libchdr/deps/zstd-1.5.6/build/` - zstd's own vendored `CMakeLists.txt`, not a
  build output directory - from every commit. A truly fresh clone's CMake configure failed outright on
  *every* platform (`add_subdirectory given source ... which is not an existing directory`); it only ever
  worked in the long-lived `autobleem-develop` checkout because those files were on disk from before the
  line was added, untracked. Anchored to `/build/` and the nine missing files recovered from that checkout.
- **First boot on hardware 2026-09-20**: the `c3a684c` arm64 image (built rootless on the server) on
  the Pi 400 - the first boot went end to end (`--grow-only` grew the image-sized root, the cores tarball,
  the cover databases, the prebuilt arm64 RetroArch, the boot splash from the single 64-bit kernel) and
  the launcher came up **black, with no sound**: SDL's "opengl" renderer could not dlopen `libGL.so.1`
  (`libgl1` was never a dependency of anything - on the 32-bit card it had come in with RetroArch's
  source-build packages, and the prebuilt RetroArch ends that) and silently gave a context with no
  shaders and no render targets. `install.sh` installs `libgl1 libgl1-mesa-dri libegl1 libgles2 libgbm1`
  since; `ableem::Renderer` logs an error when a renderer has no render-target support. On the 64-bit
  kernel `/dev/dri/card0` is v3d and `card1` the vc4 display - SDL picks card1 by itself. The same boot
  came up in the **default theme**: the kernel log said "exFAT-fs: Volume was not properly unmounted" -
  the first boot's `reboot` had left the data partition dirty and `config.ini` (copied last, small) came
  back as an empty file, which the launcher read as "no settings". Three fixes: the first-boot script
  unmounts the exFAT partitions before rebooting (a `sync` alone was not enough), `IniFile::save` writes
  atomically (`.tmp` + `DirEntry::replaceFile`) and `IniFile::load` warns about an empty file, and `Config`
  defaults `theme` to `ab2` in code (tested). Audio was fine all along (the owner's mistake).

## The PC USB stick (2026-09-20, `pcusb`)

The Pi appliance on a **32-bit x86 PC** (the owner's call: older CPUs must boot it), first of the two PC
targets of `docs/pc-targets-plan.md` (the other, the Windows product, comes after). **Debian 12 Bookworm
i386** - the last Debian with a 32-bit x86 kernel (Trixie has none; Bookworm LTS runs to mid-2028) - and
everything of the Pi's: the launcher on tty1 over kmsdrm, the exFAT data partition, the first-boot screen,
`install.sh`, the online update through `autobleem-update`. What is shared and what differs:

- **One package tree, `payload_linux/`** (was `payload_rpi/`, renamed 2026-09-20): one `install.sh` with
  `PLATFORM=rpi|pcusb` (`detect_platform()`: a Pi by its device tree, a PC by its architecture; `--platform`
  overrides) and the platform-specific parts in `*_rpi` / `*_pcusb` functions - the boot files (a Pi's
  firmware partition + `cmdline.txt`/`config.txt`; a PC's `/boot/efi` + `/etc/default/grub` and
  `update-grub`, the same quiet-boot/splash words through `boot_cmdline_words()`), the disk (the boot
  partition's, or the root's), the arch gate (`i386` -> buildbot's `x86`), the site's folder for RetroArch
  builds and cores (`rpi/` or `pc/`, `PLATFORM_DIR`), the BIOS manifest (`biospack-i386.txt`, 730 files -
  `tools/biospack.py --arch i386`), desktop OpenGL in a source-built RetroArch on x86. `--shrink-root` and
  `--hdmi-mode` stay Pi mechanisms (a PC's KMS driver takes the screen's native mode). **A PC user never reads
  "Pi"** (the owner's rule): `install.sh`'s messages say `$MACHINE`/`$MEDIUM` ("the Pi"/"the card",
  "the PC"/"the stick"), the first-boot dialogs likewise, and `system/autobleem-pc.txt` is the stick's
  edition of the options file. A PC has an RTC, so the first boot's NTP wait is the Pi's alone.
  `autobleem-session.sh` picks the HDMI/DP output whose ELD reports a screen on a PC (`pc_hdmi_audio`; the
  Pi's `vc4hdmi` cards as before). The first-boot script finds the staged package by name
  (`autobleem-pcusb-i386.tar.gz` -> the PC paths) and makes ssh host keys when an image shipped without them.
- **The build**: `AB_TARGET=pcusb` (`toolchains/pcusb/PcUsbToolchain.cmake`: Debian's `i686-linux-gnu`
  cross compiler from the Docker image's `pcusb` stage, `-march=i686 -mtune=generic` - no SSE2, Debian's own
  i386 baseline - `-D_FILE_OFFSET_BITS=64`; **the unit tests build and run there**, i386 being native on the
  amd64 server - the first run as an appliance build found three suites assuming internal games).
  `docker/run.sh ci/build.sh pcusb` -> `dist/pcusb/autobleem-pcusb-i386.tar.gz` (41 MB, top dir
  `autobleem-pcusb`; `tools/make_rpi_package.sh --arch i386`, emu from `Autobleem/bin/emu-i386/` once
  pcsx-ab has an i386 build - **it has none yet**, so the package ships no pcsx-ab and PS1 runs through
  RetroArch's pcsx_rearmed core, `launch.sh`'s fallback). Site: the `pcusb` release kind, `pc/retroarch/`
  (`ci/build_retroarch.sh i386` - the compiler triplet is `i686-linux-gnu`, the multiarch dir
  `i386-linux-gnu`), `pc/cores/i386/` (`ci/build_cores.sh i386`, buildbot's `linux/x86`), `pc/images/`
  (`repo_publish.sh pc-image|pc-retroarch|pc-cores`, `repo_index.py` indexes `pc/` as it does `rpi/`).
  `pcusb.ini`'s `retroarch_catalog=pc/retroarch/latest.json` is where the launcher's update looks for RetroArch.
- **The image, `tools/make_pc_image.sh`**: Debian publishes no i386 disk image, so it is built from packages -
  `mmdebstrap` (root mode under `docker/run.sh --privileged` on the server: its Ubuntu 18.04 kernel refuses
  `newuidmap` in a container, so `--rootless`/`--userns` is unproven there) with a customize hook that
  finishes the root from inside (our staged files, the `autobleem:autobleem` user in `sudo`, plymouth's
  theme in every initramfs, the GRUB menu, no ssh host keys, no machine-id; grub2-common's kernel hook
  diverted for the build since `update-grub` cannot probe a device in a chroot), `mke2fs -d` on the unpacked
  tar (never the tar itself: mke2fs built `--disable-nls` reads it in the C locale and refuses the first
  non-ASCII name), an MBR with p1 the ESP (FAT32 `ABBOOT`: `EFI/BOOT/BOOTIA32.EFI` + `BOOTX64.EFI` from
  `grub-mkimage` with an early config that finds the root by label, and `autobleem.txt`) and p2 the root
  (ext4 `AUTOBLEEM_ROOT`, 4 GB, grown to `root_gib` on the first boot; the data partition is made of the
  rest), `boot.img`/`core.img` written into the MBR gap by hand (`grub-bios-setup` is not in the `-bin`
  packages). No loop device anywhere. **Three kernels** because GRUB's x86_64-efi loader refuses a 32-bit
  kernel: `linux-image-686`, `-686-pae` and **`linux-image-amd64:amd64` as a foreign-architecture package**
  (Bookworm's i386 archive has no amd64 kernel; Debian's release notes describe this route) - the userland
  is i386 whichever runs; `tools/pc_image/10_autobleem` (in place of `10_linux`) picks by `cpuid -l`/`-p`.
  Secure Boot must be off. `--reuse-root` keeps the 8-minute mmdebstrap result while iterating. Output
  `autobleem-<v>-pcusb-i386.img.xz` (593 MB, 4.25 GB raw); the Docker `all` stage carries the tools
  (`grub-*-bin`, `fdisk`, `dosfstools`, `uidmap`, e2fsprogs 1.47.2 from source).
- **First boot in VirtualBox** (2026-09-20, a 32-bit VM with PAE, BIOS; `E:/tmp/pcimg`, VM `ab-pcusb-bios`,
  the VDI grown to 16 GB so the data partition has room): GRUB picked the PAE kernel, plymouth, the
  first-boot screen with the RetroArch question - then it sat on "Preparing the system partition":
  **Bookworm's util-linux no longer carries `sfdisk`** (the `fdisk` package does), and the failure was only
  in the log because the dialog had left the screen in graphics mode - both fixed (the image has `fdisk`, a
  failed grow shows a dialog). `ssh.service` failed five times over on the boot screen for want of host keys
  (a drop-in runs `ssh-keygen -A` ahead of `sshd -t`). To get a shell when the first-boot screen owns the
  keyboard: hold Shift for GRUB, `e`, append `systemd.mask=autobleem-firstboot.service` to the `linux` line,
  Ctrl+X, log in as `autobleem`/`autobleem`; VBoxManage's `keyboardputscancode`/`keyboardputstring` drive it.
  QEMU was tried first and removed again - the owner's VirtualBox is the VM for this. **The second image
  went end to end** the same day: the root grown, the exFAT data partition made, RetroArch (that run took
  the slow road - `pc/retroarch/` was not published yet, the source build died on the mixed GL flags since
  fixed, and Debian's `retroarch` package came in with its Qt desktop - ~400 MB the appliance never uses,
  so on `pcusb` a failed build now means "go on without RetroArch, retry on a later run", and the cores
  are only fetched for a RetroArch that is there), the cores and bundles, 730 BIOS files, the payload, the
  three initramfs (redundant on the image - the theme-unchanged check now applies to every run, two
  minutes saved), a reboot, and **the ab2 launcher on the screen**. The site has `pc/retroarch/`
  (v1.22.2, 6 MB: desktop OpenGL only - GLES and GL together leave RetroArch's gl1 driver unlinkable) and
  `pc/cores/i386/` (212 cores, 779 MB; `unzip`'s "done with warnings" over the cheats bundle used to kill
  `build_cores.sh`), so a fresh first boot takes the fast road - **~8 minutes to the launcher, seen on a
  BIOS VM and on a 64-bit UEFI VM** (the amd64 kernel with the i386 userland: no 64-bit RetroArch exists or
  is needed). The `7ad9b85` image is on the site (`pc/images/`, the `pcusb` package in the pre-release,
  `pc-install.html`). Untested: 32-bit UEFI, real hardware, a pad (keyboard-as-pad is off on an appliance,
  so the VM shows the carousel and no more). Found on the way: **every Pi and PC-stick install had been
  losing its sample games** - the pack has had no `Games/` since Tetrade left and `tar` was told to extract
  it by name (fixed: the members come from the pack).

## The Windows product (2026-09-20, `win`)

The second PC target of `docs/pc-targets-plan.md` (phases B and C): AutoBleem as a Windows program from an
NSIS installer - **per user, no administrator rights** (the owner's call), the program in
`%LOCALAPPDATA%\Programs\AutoBleem`, the data tree (games, settings, themes, RetroArch) in a folder of the
user's choosing, `Documents\AutoBleem` by default. **Full screen always** (the owner's rule for every real
target): the launcher, pcsx-ab, RetroArch. `AB_TARGET=win` is a real target, not a dev host: `make_win.sh
--product` -> `build_win_product/` on the PC, `ci/build.sh win` in the image (the mingw toolchain with
`AB_TARGET=win` into `build_mingw_product/`, `make_win_package.sh --product`, `makensis` ->
`dist/win/AutoBleemSetup-<v>.exe`; nsis and p7zip joined the image's `all` stage). What it took, in order:

- **B1 - where things are.** `Environment::setStateDir()/getPathToStateDir()`: what a program writes about
  itself - `config.ini`, the scan's fingerprints and report files, the online probe files - goes there;
  the default is the working path (the console, the Pi and the PC stick unchanged), the Windows product sets
  `<data>/System` (the program folder is not the user's to write). `EnvironmentSetup::fromWindowsInstall(
  HostFacts)`: the data root is the registry's `DataRoot` (what the installer wrote), else `dataroot.txt`
  next to the exe (a portable copy), else `<Documents>\AutoBleem`; the tree is made and the shipped
  `<program>/Themes` copied in once; `fromArguments()` with no argument is that on `AB_PLATFORM_WIN`.
  `WindowsHost` (`core/services/windows_host.*`) reads the facts (`GetModuleFileNameW`, `RegGetValueW`,
  `SHGetKnownFolderPath`); the decision is tested on every host with hand-filled facts. `win.ini`:
  `RetroArch/bin` with `.dll` cores, `launch_mode=direct`, `pcsx_dir=emu`, `pcsxnxt_dir=emunxt`, no
  RetroArch catalog (libretro's own build there, not ours to update).
- **B2 - the launches.** A launch is a `LaunchPlan` (exe, args, cwd) built by `LaunchService::planPcsx/
  planRetroArch/planApp` and run by the `ProcessRunner` - script mode (`rc/launch.sh` with its ten
  arguments) unchanged, `launch_mode=direct` runs the programs themselves. **Both PS1 emulators, chosen as
  on the console and the Pi** (the owner's ask): Options -> "PS1 Emulator" picks `emu/` (pcsx-ab) or
  `emunxt/` (pcsx-abnxt); a chosen folder with no binary falls back to the other, both missing to RetroArch's
  PS1 core - launch.sh's order. They start differently: **pcsx-abnxt** from its folder with `-dotdir <save
  states> -biosdir <System/Bios> -fullscreen` (the options its branch `feature/launch-dirs` adds -
  `E:\Programming\pcsx-abnxt-launch`, a worktree, pushed and not yet merged: the other session had
  uncommitted work on that repo's `develop`); the **old pcsx-ab**, which knows only launch.sh's run
  directory, from `System/runpcsx` laid out with directory links (`System::makeDirectoryLink` - a junction
  through `mklink /J` on Windows, no privilege needed, NTFS only; a symlink elsewhere, the target made
  absolute) and cleared after (`removeDirectoryLink` - the link, never its target); a FAT stick where no
  junction can be made falls back to RetroArch's core. RetroArch: the first `retroarch_binary` that exists,
  `--config <its cfg> -L <core> --fullscreen <file>`. `System::runAndWait` is real on Windows
  (`CreateProcessW`, the command line quoted by the CRT's rules, `CREATE_NO_WINDOW`), `WinProcessRunner`
  keeps the display and has the window minimised for the run and raised after (`Platform::minimizeWindow/
  restoreWindow`). Verified on the PC: the product started pcsx-abnxt full screen with `-dotdir` (its
  profile made under the game's save-state folder), the launcher back when the emulator went.
- **B3.** `System::runShellCommand` (cmd with `CREATE_NO_WINDOW`; `OnlineAssets`/`UpdateService`'s default
  runner - no console flashes over the launcher; cmd parses its own line, `/c "<line>"`, so it is built by
  hand) and `System::diskSpace` (statvfs / `GetDiskFreeSpaceExW` - the free space measured on every
  platform, the `df` pipeline is gone). `tests/core/test_system.cpp`.
- **B4.** `GuiBase/Platform` take a fullscreen flag (`SDL_WINDOW_FULLSCREEN_DESKTOP`, no mode change); the
  `Renderer` draws the 1280x720 canvas as big as fits, centred - black bars on a desktop of another shape,
  through the window target's viewport. `Gui::fullscreen()` is true on everything but the dev build
  (`AB_WINDOWED=1` asks a product build for a window). The exe is GUI-subsystem (`-mwindows`; started from
  a console it attaches to it, so `--sysinfo` prints) with the icon (`tools/make_icon.py` - a PNG-in-ICO from
  `tools/repo_icon.png`, stdlib only - `src/win/autobleem.ico`) and a version block (`src/win/autobleem.rc.in`).
  `make_win_package.sh --product` stages the program folder as `autobleem-win-product-<v>.zip`: the exe,
  its resources, the DLLs (on MSYS2 every DLL `ldd` finds, so the folder runs on a PC without it),
  `AutoBleemWinSetup.exe`, `Themes/`, `emu/` and `emunxt/` from a local build (`AB_PCSX_WIN_DIST` /
  `AB_PCSXNXT_WIN_DIST`) or the site's win64 package of each (`emu/<name>/latest.json`),
  `dataroot.txt.example`. Two things the first run showed: the no-cover-db warning was drawn before the
  theme was loaded (a black screen - now after `display()`), and the `AutoBleem.lpl` export failed without
  a RetroArch playlists dir (skipped now).
- **C1 - the setup helper.** `AutoBleemWinSetup.exe` (`apps/installer`, its CLAUDE.md): the data tree from
  the download repository - the cover databases, RetroArch (the site's repack of libretro's own Windows
  build, else the official `RetroArch.7z` unpacked by the new **`ableem::SevenZipArchive`** over the LZMA
  SDK's 7z reader vendored as `lib_ableem/third_party/lzma-7z` - the official setup exe wants administrator
  rights, checked), the cores (the site's `win/cores` pack, else one zip per core from buildbot), the BIOS
  files (`win/bios`, `tools/biospack.py --arch win64`), the samples. The site side: `ci/build_retroarch.sh
  win64` (the 7z repacked as a tarball), `ci/build_cores.sh win64`, `repo_publish.sh win-retroarch|
  win-cores|win-bios`, `repo_index.py`'s `index_win`.
- **C2 - the installer.** `installer/windows/autobleem.nsi`: welcome, the program folder, the data folder
  (its own page; a OneDrive folder warned about), the components (covers, RetroArch, BIOS, samples),
  then `AutoBleemWinSetup --run` with them (`--run`: straight to its progress page - started without it,
  from the Start Menu's "AutoBleem Setup", the helper asks the questions again to add RetroArch later; the
  owner saw the questions twice before that flag). The choices go to `HKCU\Software\AutoBleem` so a silent
  `/S` run (an update) repeats them; Start Menu and Desktop shortcuts, the Add/Remove entry, an uninstaller
  that leaves the data folder. `.onInit` waits for a running launcher (the mutex below). Run on the PC:
  the wizard by the owner, then `/S` as an update (22 s, covers kept, the fingerprints removed). **Start
  it from PowerShell or Explorer** - the MSYS2 shell rewrites `/S` into a path and the wizard shows.
- **C3 - the launcher's own update.** On `AB_PLATFORM_WIN` the launcher holds the mutex
  `Global\AutoBleemLauncher` (one instance; the installer's cue), looks for the site's `win-setup`
  package (no RetroArch check), and after the download starts the installer detached
  (`System::startDetached`) with `/S /RESTART` and leaves; the installer waits for the mutex, replaces the
  program folder, re-runs the helper `--quiet --update` and starts the new launcher. Not yet exercised end
  to end against the site (no `win-setup` published before the first `ci/build.sh win`).

The site shows the PC platform as two pills, *PC USB stick* and *Windows* (`repo_index.py`'s `tabbed()`
second level: `<h3 class="subtab">` headings inside a section; `#pc-windows` opens it). Not done: pcsx-ab's
Windows build with `-dotdir` (the old emulator runs from the junction run dir instead), a real game
through the whole chain (the fake disc boots to "Booting up..."), and the tests under wine in the image.

## The online update (2026-09-20, a Pi and the dev hosts - never the console)

The launcher keeps itself current from the download repository, the way the owner asked: **check at
start and once a day, say so, ask, and on a yes fetch everything and re-run the installer with the
first-boot screen**. The console has no network and updates from a stick, so **none of this is compiled
into the console build**: the CMake option `AB_ONLINE_UPDATE` (on by default) is switched off in the
PSC branch of the root CMakeLists, and every hook in the launcher sits behind `#ifdef AB_ONLINE_UPDATE`.

- **`UpdateService`** (`core/services/update_service.*`, `App::updates()`, configured by
  `App::applyUpdateSetting()` at start and after Options): a worker thread fetches
  `releases/unstable.json` (the "latest" channel - the one pre-release; falls back to `latest.json`
  when there is none) or `releases/latest.json` ("stable") plus `rpi/retroarch/latest.json` through the
  platform's `download_command`, and `compare()` calls a version that is not the installed one an update
  - the site keeps one pre-release, so different is newer; AutoBleem's installed version is
  `Version::VERSION-GIT_HASH` on the latest channel, the tag alone on stable; RetroArch's is the
  `/usr/local/share/autobleem/retroarch.version` stamp (no stamp = not checked). `checkDue()` is once
  per `CheckInterval` (24 h) with `System/update.json` remembering the last check, the postponement and
  the skipped versions (`ableem::UpdateState`). `startDownload()` fetches each tarball into
  `System/Updates/` with the platform's **`update_download_command`** (no short timeout - `rpi.ini`,
  `pc.ini`; `repo_url` next to it, both new `PlatformConfig` keys -> `Env::repoUrl()` /
  `updateDownloadCommand()`), checks it with the new engine **`ableem::Sha256`** against the catalog,
  and writes `pending.json` (`ableem::PendingUpdate`) for the Pi. Progress is the `.part` file's size.
  The JSON side lives in the engine (`engine/update_catalog.*`: `ReleaseCatalog`, `RetroArchCatalog`,
  `UpdateState`, `PendingUpdate`) - the app includes no JSON library. Tested in
  `tests/core/test_update_service.cpp` against a fake site.
- **Options -> "Updates"**: `stable` | `latest` | `off` (config.ini `updates`; the default follows the
  build - a `-pre` version takes `latest`). **Shown on a Pi only** for now (the owner's call).
- **The screens** (`evoui/screens/evoui_update.*`): `GuiUpdatePrompt` - "Update available", the
  versions and sizes, *Update now / Remind me tomorrow / Skip this version* (Circle = later);
  `GuiUpdateProgress` - the check, the download bar, the outcome. `GuiLauncher::pollUpdates()` (once a
  frame, next to the scan poll) raises the prompt when a check lands with something not skipped or
  postponed; the L2+R2 menu's **"Software Update"** item (`loop_softwareUpdate()`) checks on the spot.
  `offerUpdate()` runs the download and then, on a Pi, leaves the launcher with the new
  **`MENU_OPTION_UPDATE` (6)** in `autobleem_cfg.sh`; a dev host stops at a "downloaded into
  System/Updates" message (try it with `AB_UPDATE_PLATFORM=rpi` and `AB_UPDATE_RETROARCH_VERSION=...` in
  the environment - verified on the PC against the live site: prompt, 48 MB download, pending.json).
- **The Pi's apply step**: `autobleem-session.sh` sees `AB_SELECTION=6` and runs
  **`autobleem-update`** (`payload_linux/system/autobleem-update.sh`, installed to `/usr/local/bin` by
  `install.sh`'s `install_update_helper()` together with the first-boot screen, its splash and a copy
  of the installer under `/usr/local/share/autobleem/`): it unpacks the new package and runs its
  **`install.sh --update`** (`--yes`, RetroArch `prebuilt` only where the stamp says one is installed,
  nothing repartitioned, everything already there kept - config.ini, cores, BIOS, samples - and the
  initramfs not rebuilt when the splash files are unchanged) piped through the progress screen on
  tty1, with **`--retroarch-tarball`** pointing at the pre-downloaded build (a RetroArch-only update
  uses the installed release's installer copy). Success removes `System/Updates`; the session loop then
  starts the new launcher. Log: `System/Logs/update.log`. **The first real update (2026-09-20, the Pi
  400 on the `a84b8b4` image -> `6338ac9`)**: the check, the prompt and the 43 MB download all worked;
  the apply step died in one second at `tar` - the helper staged the package on the exFAT data
  partition, and tar run as root restores the archive's uid/gid, which exFAT refuses ("Cannot change
  ownership ... Operation not permitted", exit 2). The stage is `/var/tmp/autobleem-update` on the root
  filesystem now, with `--no-same-owner`. And **an installed launcher's first start after an update
  rescans** (the owner's rule): `install_payload()` removes `games.fingerprint`/`roms.fingerprint`. Nothing on the screen said why: the launcher had left, the
  helper's failure message is a 6 s dialog on tty1 and the session loop restarted the launcher - so
  read `update.log` first when an update "does nothing".

## pcsx-abnxt - the next emulator (`github.com/autobleem/pcsx-abnxt`, started 2026-09-20)

pcsx-ab (`autobleem/pcsx-ab2`, `E:\Programming\pcsx-rearmed-develop`) is a 2017 upstream snapshot (master
`bebe989b`, r22 + 25 commits - what Sony's firmware took) with Sony's and our patches; it stays the shipped
emulator until pcsx-abnxt's phase 8. **pcsx-abnxt** (`E:\Programming\pcsx-abnxt`) is a public GitHub fork of
`notaz/pcsx_rearmed` at **r26** with our own `autobleem/libpicofe` fork as the submodule, re-implementing
what Sony and we added (the front buttons, the resume-point contract, the autosave ring, disc change, the
menu, filters, two pads, `SET_BY_PCSX`) on top of what upstream has now - aarch64 dynarec, lightrec, C-SIMD
gpu_neon, lid emulation, SlowBoot, a per-serial hack database. Sony's 131-serial per-title hacks are **not**
ported (tested instead, ported on evidence). **Its port plan is complete** (2026-09-20 night, the owner's call -
the plan file is deleted, `git show` has it; the compatibility pass and the release, phases 7-8, are deferred
to the owner's testing) and `docs/reference/` is the inventory of the old delta with two patches; its CLAUDE.md the decisions. **Nothing on this side changes**:
the launch scripts, `pcsx.cfg`, `ResumePointService`'s files and `LaunchService` are the contract the new
emulator keeps, and the binary keeps the name `pcsx-ab` in the payloads.

**Both ship, the user picks** (2026-09-20, the owner's ask): Options -> **"PS1 Emulator"** (`config.ini`
`emulator` = `pcsx-ab` | `pcsx-abnxt`, `pcsx-ab` the default and the fallback for any other value - `Config`),
which `LaunchService::launchPcsx` passes as the **10th argument** of `launch.sh`; the console's and the Pi's
scripts run `Autobleem/bin/emu/pcsx-ab` or **`Autobleem/bin/emunxt/pcsx-ab`** (the same binary name and
`plugins/` layout, `emunxt-arm64/` for the 64-bit Pi as `emu-arm64/`) and fall back to `emu/` when the
chosen folder has no binary. Both read the same `.pcsx` (pcsx.cfg, memory cards); a resume point one wrote
does not load in the other (save-state versions differ) - the game starts fresh. `ci/build.sh` builds
pcsx-abnxt into `emunxt/` from `AB_PCSXNXT_DIR` / `../pcsx-abnxt` next to pcsx-ab; `make_rpi_package.sh`,
`install.sh`, the PC installer's update list and `install_autobleem.py` know the folder. The checked-in
`emunxt/` binaries are `r26-24-g0f4727f1` (console, Pi armhf, Pi arm64); on a PC a game launch is a splash
either way, so the row is only carried through there.

**The emulator speaks the launcher's language** (2026-09-20, the owner's rule: every language the launcher
has, Chinese included - not Sony's 13 PNG sets): pcsx-abnxt's own screens (its disc picker and the two
messages around it) are drawn from its own `lang/<Name>.txt` files, one per launcher language in the same
`Key=Value` format, chosen by **`-language <Name>`** - `config.ini`'s `language` value, the **11th argument**
of `launch.sh`, which both scripts pass on to nxt alone (the classic pcsx-ab would take an unknown option
for a file to run); direct mode adds it to the nxt command line. The scripts also link the emulator's
`lang/` and the launcher's `Autobleem/bin/autobleem/fonts/` (as `fonts/`) into the run directory: the
emulator's font is Selawik Light in its `skin/ui.ttf`, and `Chinese_Simplified.txt` names
`NotoSansSC-Regular.otf` (`|@font|`), which it finds through that link. The emulator packages carry
`skin/` and `lang/` since pcsx-abnxt `82d77a16`; `ci/build.sh` copies the whole dist into `emunxt/`. On
Windows (direct mode) Chinese needs a `fonts/` folder next to the emulator - not wired yet.

## RetroArch for the console (`github.com/autobleem/retroarch-psc`, 2026-09-19/20)

The console runs RetroArch from **our own build**, not RetroBoot's any more - a separate private repo,
`autobleem/retroarch-psc` (`E:\Programming\retroarch-psc`), merged from AutoBleem-NG's `retroarch-psc` +
`libretro-cores-psc` (the NG org and its repos are **gone from GitHub**; the owner's zips in Downloads were
the source). One Dockerfile with NG's crosstool-ng toolchain stage (kept as `make retroarch-ctng`, the
cores' route), but **`make retroarch` builds with the `autobleem-build` image's `/opt/psc` toolchain**
(`retroarch/build.sh`: Stretch gcc-6 + the console's glibc 2.24 sysroot + our SDL2 2.0.12 - the compiler
pcsx-ab and the launcher use; Stretch's freetype and liblzma .debs unpacked into the sysroot for the
container's life; a **wayland-scanner 1.12** built from Stretch's tarball because the image's 1.21 emits
`wl_proxy_marshal_flags()`, which the console's libwayland 1.12 lacks). NG's four patches (wl_shell
fallback, PowerVR ribbon shader, pipeline limit, ALSA S16) plus **ours, `xz_core_loading.patch`**:
`dylib_load` unpacks a core whose file is an xz stream (KMFD's `km_*` cores, 147 of the owner's 178) with
liblzma into `/tmp/retroarch-cores/` and dlopens the copy - kept while the source's path/size/mtime match,
one core cached at a time (`HAVE_XZ_CORES=1`, `-l:liblzma.a`, the firmware has no liblzma). Result: v1.22.2
`autobleem-<build>`, 10.4 MB stripped / 3.4 MB UPX'd, GLIBC <= 2.22, libstdc++ static, 17 firmware
libraries; tag `v1.22.2-1`, `make package-retroarch` -> `retroarch-psc-<tag>.zip` + `manifest.json`
(`tools/make_manifest.py`, what the PC installer will read), `make publish` -> `psc/retroarch/` on the
download repository. **Ran on the console 2026-09-20**: XMB, the PSC pad autoconfig, Wayland/EGL/GLES 3.2
hw context, ALSA, the xz unpack - all in `retroarch/logs/retroarch.log`. GitHub Actions is written but
gated off (`CI_ENABLED`); the owner builds on the server only.

**Cores**: not built by us yet - `cores/cores.txt` is the RetroBoot roster (81) in build-priority order,
NG's full 170 kept as `cores-full.txt`; the estimate for building the 81 on the 2-core server is ~a day
(guessed from source sizes, not measured). **For now the console gets RetroBoot 1.2's cores as they are**:
`tools/check_cores.py` (pyelftools) reports a `cores/` folder - xz or ELF, ABI, GLIBC/GLIBCXX against the
firmware's 2.24/3.4.22, NEEDED against the firmware, libretro exports, GL; on the owner's stick 173 of 178
load in any RetroArch (5 cannot on a stock console: two need glibc 2.28/2.29, `km_emux_chip8` is an x86-64
build, `bsnes` needs libgomp, `km_imageviewer` is soft-float). `tools/pack_retroboot_cores.py` (`make
pack-retroboot-cores RETROBOOT_DIR=F:/retroarch`) packs the 171 + their info files + `cores-psc-<date>.json`
(sizes, sha256, glibc, GL, display names, and what was left out and why) -> `psc/cores/`. How RetroBoot
injects libraries, for the record: `LD_LIBRARY_PATH=retroboot/lib` (liblzma + a GLIBCXX 3.4.25 libstdc++)
for every RetroArch launch, `retroboot/assets/lib` -> `/tmp/rblib` for EmulationStation and the apps; none
of the cores needs either. Its `launch_rfa_rom.sh` relaunches RetroArch **five times** on a non-zero exit
(the blinking red LED, ~45 s) before returning to the launcher - our own launch scripts should give up
after one.

**What a RetroBoot-era `retroarch.cfg` gets wrong on 1.22.2** (all in the repo's `theme/retroarch-psc.cfg`,
the installer's cfg fragment): `xmb_theme = "8"` was RetroSystem in 1.9.0 and is Monochrome Inverted now
(RetroSystem is 7); `quit_on_close_content` (new since 1.10, default never) must be `2` or Close Content
stays in XMB instead of returning to the launcher as 1.9.0 did; `video_context_driver` must be `wayland`.
`menu_swap_ok_cancel_buttons = "true"` (Cross = OK in RetroArch's menus) is unchanged and works - a
core's own menu is the core's mapping (prboom: RetroPad A = Circle = enter), not RetroArch's. `theme/` in
the repo is the **ab2 XMB theme** for 1.22.2: `Autobleem2.png` (made by `make_wallpaper.py` from `payload/Themes/ab2/images/AB-EvoBack.jpg`
- logo bottom right, out of XMB's way, the bottom band a reflection of the texture), Selawik Light, the
RetroSystem icons (a 2020 RetroBoot stick lacks 19 that 1.22.2 asks for - `disc.png`, `movie.png`, `Sega -
Mega Drive - Genesis.png`, ... from libretro's retroarch-assets), `retroarch-theme.cfg` with the 1.22.2
keys. `video_context_driver` must be `"wayland"` (empty = KMS first, which Weston blocks). All of this is
on the owner's F: stick (the RetroBoot binary kept as `retroarch.retroboot-1.9.0`, the cfg as
`retroarch.cfg.retroboot`) with free test content in `roms/` (Peter Lemon's SNES/NES/GB/GBA homebrew,
mamedev's free arcade ROMs, Doom/Quake shareware, Cave Story); N64 needs a real game - the homebrew RSP
tests crash GLupeN64 (a core dump inside the core, not RetroArch). **Verified on the console 2026-09-20**:
Cave Story, Doom, Quake run and return.

**Coming back from RetroArch** (2026-09-20): the PSC's GPU frees the emulator's memory 3-4 s after the
process is gone; the launcher's window rebuilt sooner had its buffer uploads fail (`PVR: glBufferSubData:
No memory for object data` in `AB_err.txt`), Weston dropped the client (`wl_display@1: error 0: invalid
object 16`), SDL posted a Quit and `AutoBleem::run()` took it for the window's close button - out through
`selection.sh`'s reboot (what looked like the console going to sleep). Now `launchGame()` waits 2 s after a
RetroArch session (300 ms after pcsx) and `run()` treats a Quit on the console as a lost display: release,
1 s, rebuild, three times before giving up (after Quake it took two rebuilds; the log says
`The display went away (attempt n of 3)`).

**The splashes around RetroArch** (2026-09-20): `absplash` (`src/tools/absplash.cpp`, lib_ableem's ui
only, shipped packed next to the launcher with `src/resources/splash/{retroarch,autobleem}.jpg`) shows a
picture in the same full-screen window the launcher and RetroArch use - `absplash IMAGE --until-exists F |
--until-gone F | --seconds S [--timeout S]`. On the stick RetroBoot's `launch_rfa_rom.sh` (patched by hand,
copy at `E:/tmp/launch_rfa_rom.sh.ab2` - the model for our own launch script) runs it: the RetroArch
picture from launch until RetroArch's log says `Found display driver` (+1 s, `/tmp/.ra_up`), the AutoBleem
2 picture from RetroArch's exit until the launcher's window is back - `launchGame()` removes
`/tmp/.abload` after `display(true)`; `rc/launch_rb.sh` no longer does. **The reason nothing showed for a
day**: the stick's `retroboot/retroboot.cfg` had `show_splash=0` (RetroBoot's own setting; the backup
copy had 1) - the splash functions never ran. RetroBoot's rbimage/abimage (a 1280x720 toplevel window,
a 200x200 BMP at (540,260)) are replaced, not fixed. Verified on the console.

**The stick's layout, and our own launch scripts** (2026-09-20, the owner's cleanup of the F: stick - the
final structure the PC installer makes): `Themes/` (was `themes/`), and everything of RetroArch's under
**`RetroArch/`** - `bin/` is RetroArch's own tree (what was `retroarch/` at the root: the binary, cores,
info, assets, playlists, saves, `retroarch.cfg`, and RetroBoot's leftover `retroboot/` and `apps/`
folders, unused), `bios/` its system directory (was `retroarch/system`; `retroarch.cfg`'s
`system_directory`), `roms/` the other systems' games (was `roms/` at the root). `psc.ini`/`pc.ini` say
`retroarch_dir=RetroArch/bin`, `retroarch_roms_dir=RetroArch/roms`, `retroarch_bios_dir=RetroArch/bios`
(`Env::getPathToRetroarchBiosDir()`; the Pi's is `RetroArch/system`), `retroarch_core=cores/pcsx_rearmed_libretro.so`
(`km_pcsx_rearmed_neon` never existed on a RetroBoot 1.2 stick); the engine's defaults are the same, and
`UpdateRoms` tells a console stick by its `RetroArch/bin`. FAT/exFAT are case-insensitive, so an old stick
or card keeps working with `Themes`. **RetroBoot's scripts are not run any more**: `rc/launch_rb.sh <file>
<core>` starts RetroArch itself (`NEON`/`PEOPS` -> pcsx_rearmed/swanstation as RetroBoot mapped them; the
tree's directories and the console's PS1 BIOS into `RetroArch/bios` first; `XDG_CONFIG_HOME` is a `/tmp`
dir whose `retroarch` is a symlink to `RetroArch/bin`, so the cfg's "default" directories - favorites,
history - land there as RetroBoot's `XDG_CONFIG_HOME=/media` put them in `retroarch/`; the absplash
pictures; a non-zero exit keeps `logs/retroarch_crash.log` with dmesg and blinks the red LED four times,
**no relaunch**), `rc/retroarch.sh` runs it with nothing loaded and restarts AutoBleem, `rc/app_env.sh` is
what an App's `run.sh` sources (links `Autobleem/lib/apps/*` into `/tmp/applib` with the soname links -
the stick cannot hold symlinks - and exports `LD_LIBRARY_PATH`; RetroBoot's `init_libs.sh`), `boot.sh`
insmods `Autobleem/lib/modules/*.ko` (xpad), `launch_rb.sh` puts `Autobleem/lib/retroarch` on RetroArch's
path when present (our build needs nothing from it). `Autobleem/lib/{apps,retroarch,modules}` is the
site's **libs pack** unpacked (its groups are named so). **The Apps**: `payload/Apps` keeps only pscbios and
abflashkit; the eight third-party apps (amiberry, doom, eduke32, openbor, opentyrian, sdlpop,
shadowwarrior, wolf4sdl) are the site's **apps pack** (`tools/pack_psc_apps.py F:/Apps`), each
self-contained under `Apps/<name>/` - the RetroBoot-era ones had their binaries in `retroarch/apps/<name>`
and got them moved in, their scripts pointed at `/media/Apps/<name>`, `/media/System/Logs` and
`app_env.sh`. `Apps/retroboot` (RetroBoot's own menu as an app) is gone - the system menu's RetroArch item
is that. **`tools/install_autobleem.py --stage layout`** converts an older stick in place (renames, moves,
`retroarch.cfg` and every playlist's paths, `Applications.lpl` removed, the libraries copied out of
`retroboot/`, the apps made self-contained) and is idempotent; it ran on the owner's stick, then the new
launcher, `absplash`, the tools, the platform inis, the rc scripts and `UpdateRoms.exe` (which wrote
`/media/RetroArch/roms/...` playlists) went on. **Verified on the console 2026-09-20** (the owner: "looks like it works ok").
`payload/RetroArch/` is the folder's skeleton (README files) plus **`bios/biospack.txt`**, the console's
BIOS manifest: `tools/biospack.py --arch psc` takes the cores from `psc/cores/latest.json` (KMFD's
`km_<core>_xtreme...` names folded onto RetroBIOS's - `PSC_CORE_ALIASES`), adds the systems only those
cores cover (`PSC_SYSTEMS`: Saturn, Dreamcast, DS, PC-FX, Atari ST, CPC, PSP, DOSBox, NXEngine, xrick) and
keeps `dc/` - 719 files, 302 MB against the Pi's 647/188. `.gitignore`'s `bios/` rule has an exception for
that folder's two files.

**The PC installer exists** (2026-09-20, `apps/installer/`, its own CLAUDE.md): `AutoBleemInstaller.exe`,
a Win32 program like UpdateRoms with the Pi first-boot screen's look (the splash picture on top, the
questions as checkboxes, then two progress bars and the log), shipped as `AutoBleemInstaller-<v>.zip` with
the release's `autobleem-psc-<v>.tar.gz` next to it (`tools/make_installer_bundle.sh <tarball>`; `PACKAGE_KINDS`
"installer" on the site, INDEX_VERSION 21). It picks a removable drive (formats it FAT32/exFAT through
`format.com`, or `fat32format.exe` beside it for FAT32 over 32 GB), unpacks the tarball, fetches the ticked
cover databases (all three by default), RetroArch with cores/libs/apps/libretro bundles (off by default),
the BIOS files by `psc/bios` (needs RetroArch), the samples; run again it **updates** - the package's own
files replaced, everything of the user's kept, `config.ini` too - and an **AutoBleem 1.0 / NG stick is
brought to the new layout first** (the `layout` stage's steps in C++, `legacy_layout.*`; RetroBoot's
playlists dropped for the launcher's scan to rebuild). The engine got `TarArchive` (`.tar.gz` through
miniz, `tests/support/tar_builder.h` writes them for the tests), `PackCatalog`/`PscRetroArchCatalog` for
the `psc/*/latest.json` shapes and `DirEntry::createDirs`; `DirEntry::copyFile` had returned the opposite
of what it did (no caller until now). Verified on the PC over the real site into a folder (covers in 25 s;
RetroArch + all 719 BIOS files + samples, 1.7 GB); **not yet on a real stick or the console**.

**The installer ran on the owner's stick and the result booted** (2026-09-20). Since then it also puts
**UpdateRoms** on the stick (a phase after the unpacking: `releases/unstable.json` / `latest.json`, the
release whose `psc-fs` is this package, its `updateroms` zip into `<stick>/UpdateRoms/`; missing = a
line, not a failure) and **names the stick SONY** (`ensureVolumeLabel`, the status line says so). What is
left is in `TODO.md`.

## Console tools (`apps/`, 2026-09-18) - and one PC tool

`apps/installer/` (2026-09-20) and `apps/updateroms/` (2026-09-19) are the odd ones out: **PC** programs. The
installer is described under "RetroArch for the console" and in its own CLAUDE.md. UpdateRoms is a program, `UpdateRoms.exe`, built on the dev
hosts only (root `CMakeLists.txt` skips it for `arm`/`aarch64`), that scans a console stick or a Pi card
sitting in a card reader - `UpdateRomsJob` (core, tested) over the same `RetroArchScanner`/`CoreInfoTable`/
`OnlineAssets` the launcher's scan uses, writing the target's paths. **A plain Win32 window, no SDL, no
AutoBleem theme** (the owner's call), linked `-static`: one 540 KB exe with no MinGW DLLs.
`tools/make_updateroms_bundle.sh` makes the folder for a stick (a Release build in `build_updateroms/`),
`make_usb.py` stages it into `usb/UpdateRoms/`. Its own CLAUDE.md has the rest.

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
  changed. `targetRomsDir` is what the playlists name (the console's `/media/RetroArch/roms` when a PC writes them -
  the plan's step 5); `""` = `romsDir`. `AutoBleem`, `Applications` and `content_*` are never written.
  With `Options::rdbDir` (2026-09-19) each folder's `ScannedRoms` go through `identify()` first: the
  system's `.rdb` names a zip member by CRC, a loose file by `Crc32::ofFile` (up to `maxCrcBytes`), an
  arcade set (`wholeArchive`) by `rom_name`; the label becomes the record's name and, in the merge, an
  identified entry replaces an existing one for the same ROM whose label differs. **A rescan is cheap**
  (2026-09-21): with `Options::stateFile` a digest per folder (file names + sizes, the playlist's size,
  the rdb's size, target path, core - no mtimes) lets a folder nothing changed in be skipped outright
  (`RetroArchScanResult::systemsSkipped`; its playlist is only read for the counts), and
  `seedCrcsFromPlaylist` gives a loose ROM the CRC its existing playlist entry carries, so `identify()`
  hashes only files new to the playlist. `UpdateRoms` passes no state file yet (the target's state dir
  differs per platform); the digest is portable, so it could.
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

### The platform model (2026-09-20)

One CMake cache string, **`AB_TARGET`** = `psc | rpi | pcusb | win | dev`, says what a build is for
(empty: `psc` when cross-compiling for ARM, `dev` otherwise; the toolchain files force theirs; the old
`AB_TARGET_RPI=ON` is a deprecated alias). Exactly one `AB_PLATFORM_<TARGET>` is defined from it, and
`core/services/environment.h` derives the macros the sources actually test - each named for what it means,
and **the sources never test the CPU or the OS** to tell targets apart (an i386 Linux build used to fall
through `__x86_64__ || _WIN32` and compile as the console):

| macro | targets | meaning |
|---|---|---|
| `AB_DEBUG_HOST` | dev | a development machine: the emulators are not forked (the splash runner), power off is `exit()`, free space is not measured, keyboard-as-pad, a 1280x720 window, the 1/2-arg roots (`make_win.sh`, `tools/win_drive.ps1`) |
| `AB_APPLIANCE` | rpi, pcusb | forks and halts for real, no console tree behind it (the data partition on the command line), no built-in games, an update = `MENU_OPTION_UPDATE` for the session script to re-run the installer |
| `AB_ROOT_RELATIVE_LAYOUT` | all but psc | every path from a root given on the command line (or found by the Windows product) instead of `/media` + `/usr/sony` |
| `AB_HAS_INTERNAL_GAMES` | psc, dev | `/gaadata`'s games can be shown (`GameQueryService::showInternalGames`, the Options row) |

`Env::platformName()` is `psc/rpi/pcusb/win/pc` and names `resources/platform/<name>.ini` - where a
platform's *paths* differ (`PlatformConfig`; since 2026-09-20 also `retroarch_catalog`, the site's
RetroArch listing for the update check - `rpi/retroarch/latest.json`, `pc/retroarch/latest.json` for the
PC stick, empty on Windows; `launch_mode=script|direct` and `pcsx_dir` for the Windows product's direct
launches; `core_extension` - `.dll` on Windows, what `CoreInfoTable` and the default PS1 core file use).
`ABLEEM_EMBEDDED_TARGET` is on for psc/rpi/pcusb. `win` is the Windows product (the NSIS installer, a real
target; `make_win.sh --product`), `pcusb` the 32-bit Debian PC stick - both done (`docs/pc-targets-plan.md`,
the sections above).
The other per-target switches: the console tools (`apps/pscbios`, `apps/abflashkit`) build for psc/dev, the PC
programs (`apps/updateroms`, `apps/installer`) for dev/win, `AB_ONLINE_UPDATE` is off for psc.

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

- **sccache** (2026-09-20) sits in front of every compiler in the image: `ci/build.sh` (and pcsx-ab's)
  configure with `CMAKE_C/CXX_COMPILER_LAUNCHER=sccache`, `docker/run.sh` mounts the cache from the host
  (`~/.cache/autobleem-sccache`, `AB_SCCACHE_DIR`, 10 GB) so it outlives the container, the run ends with
  the stats; `AB_NO_SCCACHE=1` opts out. The console target: 263 s cold, **42 s** with the cache warm
  (95 % hits) - the remaining misses are what includes the generated `core/version.h`. The image's last
  layer holds the binary (a musl release), so a version bump rebuilds nothing else.
- **CI: one Docker image builds every target** (2026-09-19; `docs/ci.md` is the operator's page,
  `docs/ci-plan.md` the plan until the workflows have run). `docker/Dockerfile` -> `autobleem-build`
  (Debian Bookworm, ~3.4 GB, built on the server with `docker/build-image.sh`): the native build with
  clang-format/clang-tidy **22** (apt.llvm.org, the major MSYS2 has), Debian's `crossbuild-essential-armhf`
  / `-arm64` with the multiarch `libsdl2*-dev` packages for the two Pis, `mingw-w64` (posix) with the
  official SDL2 mingw devel packages at `/opt/mingw-sdl2` for Windows, the three cover databases at
  `/opt/autobleem/db`, and **the console toolchain by AutoBleem-NG's recipe** under `/opt/psc`: a Debian
  Stretch armhf sysroot (`mmdebstrap --variant=extract` from archive.debian.org - glibc 2.24 / libstdc++
  6.0.22, the console's own), Stretch's **gcc-6** cross compiler (patchelf'ed RUNPATH to its own
  isl/mpc/mpfr/gmp, its libc linker scripts rewritten to bare names - no host `/usr/arm-linux-gnueabihf`
  hijack, that directory is the Pi cross libc's), and **SDL2 2.0.12 + image 2.6.3 + mixer 2.6.3 + ttf
  2.20.2 built from source** with the console's backend set (Wayland + dummy, GLES via EGL, ALSA, udev, no
  X11), wrapped as `armv8-sony-linux-gnueabihf-*` so `PSCtoolchainV8.cmake` works with
  `-DAB_PSC_TOOLCHAIN=/opt/psc`. The 2019 Sony toolchain (`/opt/toolchain`, `autobleem/PSC-CrossCompile-
  Toolchain`) is **no longer what releases are built with** (the owner's call: outdated). Each stage ends
  with `docker/ab-validate.sh` linking a C++14 + SDL test program and checking the result (the console:
  ARMv8, nothing above GLIBC_2.24 / GLIBCXX_3.4.22, no RPATH, a Wayland SDL2).
  `ci/build.sh native|psc|rpi|rpi64|win|all` (run as `docker/run.sh ci/build.sh <t>`) configures into the
  same `build_*/` dirs the `make_*.sh` scripts use, builds, validates and packages into `dist/<t>/`; for
  `psc`/`rpi`/`rpi64` it **builds pcsx-ab first** from the sibling checkout (`AB_PCSX_DIR` /
  `../pcsx-ab`; pcsx-ab has its own `ci/build.sh` and the same Debian-fallback toolchain files) and the
  package ships that emulator. New scripts: `tools/make_psc_package.sh` (the console zip - the release
  script `make_psc.sh` always assumed; it also **regenerates `libs.tar.gz`** from the image's SDL build,
  keeping iconv/ogg/vorbis) and `tools/make_win_package.sh` (launcher zip with the four SDL DLLs +
  `libwinpthread-1.dll`, and `UpdateRoms-<v>.zip`). The workflows: `.github/workflows/image.yml` (the
  image, self-hosted only - the cover DBs are there - pushed to `ghcr.io/autobleem/autobleem-build`) and
  `ci.yml` (`native` on every push/PR; the cross targets on develop/master/tags/dispatch; a `v*` tag ->
  draft release with the five packages; PRs always on GitHub-hosted runners). The self-hosted runner is
  `docker/runner/compose.yml`. Verified 2026-09-19 on the server: all five targets green (37/37 tests,
  format, tidy), packages inspected; **not yet run through GitHub Actions**. **Run on a console
  2026-09-19**: the psc package's launcher, `libs.tar.gz` and pcsx-ab went onto the owner's stick (the
  previous set kept in `E:\tmp\stick-prev`), and the launcher started with all covers - the first hardware
  run of any console build of this repo. Two bugs the console then showed, both fixed the same day: pcsx-ab
  segfaulted at its first frame (its Wayland branch never set `SDL_SysWMinfo::version`, so SDL 2.0.6+'s
  "Version must be 2.0.6 or newer" check failed on uninitialised stack - a coin the gcc-8 build won and the
  gcc-6 build lost; pcsx-ab2 `70c5dcb`), and no sound anywhere (the Dockerfile's linker-script rewrite had
  truncated the sysroot's `libasound.so`, so SDL2 was built without ALSA; `ab-validate psc` now checks the
  audio backends). `rc/launch.sh` writes `System/Logs/launch.log` + `pcsx.log` since then - that is how
  both were read. A stock-firmware console also needs RetroBoot 1.2's `retroarch` (the vendored bundle);
  a RetroBoot 1.1 tree with a later KMFD build wants GLIBC_2.28 and never starts (see `retroarch/logs/`).
  What the image's compilers turned up: the console's gcc-6 cannot combine an inherited constructor with a
  member initialised from another member (`GuiLauncher` now spells its constructor out - keep it that way
  for every screen), and the test fixture's scratch dirs now carry the pid (`ctest -j` runs suites in
  parallel; same label + counter in two processes deleted each other's trees). Editing the root
  CMakeLists' console branch: a GCC < 8 gets `-march=armv8-a -mfpu=neon-vfpv4` (it used to get armv7ve,
  no NEON - a branch that had never been compiled).
- **PlayStation Classic (real target)**: `make_psc.sh` → `toolchains/psc/PSCtoolchainV8.cmake` → `build_psc/dist/`
  (`autobleem-gui`), built **on the build server over ssh** - the same shape as pcsx-ab's
  `make_psc.sh`, so the two build side by side there. `ssh psc-build` (a `Host` entry in `~/.ssh/config`, in
  both the Windows profile and `C:\msys64\home\<you>` - MSYS2's ssh and Git for Windows' ssh have different
  homes; key `~/.ssh/id_ed25519`). Ubuntu x86_64, 2 cores, Sony's crosstool-NG toolchain at `/opt/toolchain`
  (GCC 8.2.0, sysroot `/opt/toolchain/armv8-sony-linux-gnueabihf/sysroot` with SDL2 2.0.4 + image/mixer/ttf
  `.so`s). The distro CMake is 3.10; `~/opt/cmake` (3.31) is what the script uses. The tree is rsynced to
  `~/autobleem` (minus `usb/`, `db/`, `payload*/`, `!refactor/`, the Pi devkit), built in `~/autobleem/build_psc`
  with Unix Makefiles `-j2`, and the two binaries come back by tar (rsync refuses NTFS modes). **Incremental
  by default** (2026-09-19, the owner's request - every build used to start from nothing): the remote
  build dir is kept and rsync sends only what changed; `--clean` wipes it first (`-k` is the old spelling
  of the default). Invoke from the MSYS2 UCRT64 shell like `make_win.sh`.
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
  the git facts up as `AB_GIT_*` environment variables because the tree goes up without `.git`. This
  Sony-toolchain build has never run on a console; the image's gcc-6 build has (2026-09-19).
- The Pi toolchain files (`toolchains/rpi/RPitoolchain.cmake`, `toolchains/rpi64/RPi64toolchain.cmake`, over
  the shared `toolchains/rpi/common.cmake`) take the SysGCC toolchain when its directory exists
  (`AB_RPI_TOOLCHAIN` / `AB_RPI64_TOOLCHAIN`, the Windows PC) and Debian's multiarch cross compiler
  otherwise (the image) - same thing in pcsx-ab. `toolchains/mingw/MinGWtoolchain.cmake` is the Windows
  cross build from Linux (`-static-libgcc -static-libstdc++`; the tests are built and run only under
  wine, which the image does not have - the native target runs the suites).
- **Raspberry Pi (32-bit Pi OS)**: `make_rpi.sh` → `toolchains/rpi/RPitoolchain.cmake` → `build_rpi/`, then
  `tools/make_rpi_package.sh` for the installable tarball. Incremental since 2026-09-19 (it used to
  `rm -rf` the build dir on every run); `--clean` wipes it, `--debug` builds into `build_rpi_dbg/`. See the
  "Raspberry Pi port" section above. All three build scripts are incremental now; `make_win.sh`'s time is
  mostly `ctest`.
- **Linux/macOS (native)**: `make_sys.sh` - a plain host build into `build_sys/`. (The root's 2019 leftovers
  went on 2026-09-19: the armv7 `MacToolchain.cmake`/`PS1Ctoolchain.cmake`/`PSCtoolchainV7.cmake` and
  `make_mac.sh`/`make_all.sh` - the console build is `make_psc.sh` with `toolchains/psc/` - and
  `make_english.txt.sh` (`tools/lang_tools.py extract`), `.dep.inc`, a stray `coversP.db`, `default.lic`,
  root copies of `default.png`/`pcsx.cfg` (the real ones are in `src/resources`), and `win_drive.ps1`'s
  screenshots, now ignored as `/shot*.png`.)
- **Windows/MinGW (dev + smoke test)**: `make_win.sh` → `build_win/autobleem-gui.exe`. Uses MSYS2 UCRT64
  (`C:\msys64`, installed 2026-09-15) with `mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,SDL2,SDL2_image,SDL2_mixer,SDL2_ttf,pkgconf}`.
  Invoke from PowerShell as `$env:MSYSTEM='UCRT64'; C:\msys64\usr\bin\bash.exe -lc "cd /e/Programming/autobleem-develop && ./make_win.sh"`.
  Run needs `C:\msys64\ucrt64\bin` on PATH (SDL DLLs).
  Windows-only shims: `mkdir` one-arg, `sys/wait.h` guarded, `System::runAndWait` stubbed. A dev build is
  `AB_TARGET=dev` -> `AB_DEBUG_HOST` (see "The platform model" below) - use that, never `__x86_64__` or
  `_WIN32`, to mean "a development machine".
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
  - `tests/support/temp_dir.h` - a scratch tree that deletes itself; `makeSubDir`/`writeFile`/`readFile`. Named
    with the pid and a counter, so suites run in parallel (`ctest -j`, the CI) cannot touch each other.
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
usb/System/Logs/               usb/Games/<game dirs>/   usb/Themes/ <- payload/Themes/*
```
Run `autobleem-gui.exe <usb>` from `usb/Autobleem/bin/autobleem`; stdout/stderr are the log. Expected noise on
Windows: `ALTER TABLE ... duplicate column` (the add-column-if-missing idiom) and a failed `popen` of
`backup_internal.sh`.

**`AB_SHOT=<file%d.bmp>`** (2026-09-20) has the launcher save the frame it presents every 3 s - the way to look
at a Pi's screen over ssh (a systemd drop-in `Environment=AB_SHOT=/tmp/ab%%d.bmp` on `autobleem.service`,
removed afterwards: 8 MB a frame into tmpfs) or at a PC whose screen is in use; pcsx-abnxt has the same as
`PLAT_SDL2_SHOT`. `win_drive.ps1` had posted its keys to the process's *console* window all along (the
`GetWindowText`/`GetClassName` imports were not `CharSet.Unicode`, so every name came back one letter long
and the `SDL_app` window was never matched) - fixed the same day; the resume-slot picker on the PC needs a
clean return from a game (`filename.txt` in the game's `!SaveStates`), which the splash runner simulates when
the file is there.

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
/media/Autobleem/bin/autobleem/   autobleem-gui + absplash + resources (run.sh, lang/, evoimg/, platform/, splash/, ...)
/media/Autobleem/bin/emu/         pcsx-ab + plugins/*.so
/media/Autobleem/bin/emunxt/      pcsx-abnxt, the next emulator, as pcsx-ab + plugins/*.so (Options -> "PS1 Emulator")
/media/Autobleem/bin/db/          covers*.db (regional cover-art DBs; "../db" relative to the binary)
/media/Autobleem/rc/*.sh          boot/launch glue (see payload/Autobleem/rc)
/media/Autobleem/lib/libs.tar.gz  shared libs unpacked to /tmp/lib at boot; lib/apps, lib/retroarch, lib/modules
                                  are the site's libs pack (the Apps' libraries -> /tmp/applib, RetroArch's, xpad.ko)
/media/Games/                     user games, one folder per game, sub-folders allowed; Games/!SaveStates/<folder name>/
                                  (every game's states + its own card, central) and Games/!MemCards/ (the shared cards)
/media/System/Databases/          regional.db (USB games), internal.db (copy of stock DB + extra columns)
/media/System/Logs/               AB_out.txt / AB_err.txt (stdout/stderr of autobleem-gui), autobleem.log (plog,
                                  rolling), launch.log / pcsx.log (the launch scripts' and pcsx-ab's), ui_menu.log
/media/System/lightguns.txt       RetroArch games flagged as light-gun games, one image path per line
/media/System/Bios|Preferences|Region|UI/   rc/backup.sh's copies of the console's own files, made at boot
/media/Themes/<name>/theme.json   UI themes (docs/theme-format.md); /media/Apps/<name>/ launchable apps (app.ini + run.sh)
/media/RetroArch/bin/             RetroArch's own tree (since 2026-09-20; was /media/retroarch): the binary, cores/, info/,
                                  database/rdb/Sony - PlayStation.rdb (game metadata), thumbnails/Sony - PlayStation/
                                  Named_*/ (covers), screenshots/, states/, playlists/, retroarch.cfg
/media/RetroArch/bios/            RetroArch's system directory - the cores' BIOS files (bios/biospack.txt lists them)
/media/RetroArch/roms/            the other systems' games, a folder per system named as RetroArch's databases are
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
or `rc/launch_rb.sh` (RetroArch: file, core - our own script since 2026-09-20, see "RetroArch for the console";
an App's `run.sh` sources `rc/app_env.sh`). `LaunchService::writeSelectionScript()` writes `rc/autobleem_cfg.sh`
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
| `core/services/online_assets.*` | `OnlineAssets` | The scan's online side (2026-09-19): `probe()` (one request per instance), `fetch(url, file)` through the platform's `download_command` (`%u`/`%o`, `std::system`, a `.part` renamed on success), `ensureDatabases(rdbDir)` (the 40 MB `database-rdb.zip` unpacked when there is no `.rdb`), `fetchBoxArt(thumbnailsDir, db, label)` -> Fetched / AlreadyThere / Missing (remembered in `Named_Boxarts/.autobleem-missing.txt` after a re-probe) / Failed (the network went). `boxArtUrl()`/`urlEncode()` spell the libretro-thumbnails URL. `CommandRunner` is the test seam. Made per scan cycle by `ScanService` from what `setOnline()` was given (`App::applyOnlineSetting()`: config.ini `online` + `Env::downloadCommand()`). |
| `core/services/scan_service.*` | `ScanService` | The background scan: one worker thread (lowest OS priority - `System::lowerCurrentThreadPriority()`) does the filesystem work (`GamesFingerprint`, `GameScanner`, its own `CoverDatabase`, and - with RetroArch detected, `romScanEnabled()` - `ableem::RetroArchScanner` over the ROM folders with its own `CoreInfoTable`) and queues `WorkerEvent`s; `poll()`, called once a frame from `GuiLauncher::loop()`, applies every regional.db write on the main thread, has `RetroArchService` reload rewritten playlists, and returns a `ScanUpdate` (added/updated/removed games, `playlistsWritten`, progress, finished with the game and ROM counts). `requestScan()`/`scanning()`/`setWatching()`; `checkForChanges()` is the watcher's debounce over both `games.fingerprint` and `roms.fingerprint`, checked every `ScanWatchInterval` when nothing was requested directly; `fingerprintsMatchDisk()` is the startup check. **A moved game keeps its row** (2026-09-21): the rows whose folder is not where the database says are kept aside at `ScanStarted` (`VanishedGame`: id, folder name, disc names), a verified game at a new path with the same folder name and disc file names claims one (`claimMovedGame` -> `GameDatabase::updateGamePath`, reported in `updatedGames`, so id/history/last_played and the carousel's selection survive a drag into a sub-folder), and the unclaimed are deleted at `Finished`; a *renamed* folder is a new game. The ROM pass gets `<state>/roms.scanstate` (`romScanStateFilePath()`) as the scanner's per-folder state, so a rescan skips every ROM folder nothing changed in. Owned by `App` (`app.scans()`, constructed with `&retroArch_`). |
| `core/main.h` | | The `using` declarations that bring the lib_ableem engine names (`DirEntry`, `sep`, `ImageType`, `GAME_INI`, `trim`/`lcase`, `IniFile`, `GameDatabase`, ...) into the app's global namespace. |
| `core/services/environment.*` | `Env` | `struct Environment : ableem::Environment` + the two app flags, the `AB_DEBUG_HOST` macro, `platformName()` (`"psc"`/`"rpi"`/`"pc"` - the one place the build macros decide a path), `retroArchInstalled()` and `padMappingFiles()` (the `gamecontrollerdb.txt` list `Gui`'s constructor hands `Input::loadMappings()` - the kernel's `/etc/autobleem` one on the console, then the shipped one in the resources dir; **loaded since 2026-09-18** - until then nothing called `loadMappings` and the pscbios wizard's output was never read). All path getters live in the library (`getPathToKernelConfigDir()` is `""` off the console); extend `ableem::Environment` instead of adding new literal paths. |
| `core/services/platform_config.*` | `PlatformConfig` | **What differs per target about where things are, as data**: `resources/platform/<platform>.ini` (`psc.ini`, `rpi.ini`, `pcusb.ini`, `pc.ini`; `win.ini` to come) - `retroarch_dir` (relative to the USB root), `retroarch_core` (the PS1 core the exported playlist names, relative to that dir), `retroarch_binary` (`;`-separated candidates; "RetroArch" in the system menu and Square on a game are offered when one exists), `retroarch_roms_dir` (the other systems' ROM folders the scan writes playlists for, relative to the USB root; 2026-09-18), `download_command` (how the platform fetches a URL to a file, `%u`/`%o`, with its own timeout; empty = never online - the console; 2026-09-19, `Env::downloadCommand()`). `main.cpp` loads and `apply()`s it after the roots are set; a missing file means the console's layout. `retroarch_catalog`, `launch_mode`, `core_extension`, `pcsx_dir` (2026-09-20, see "The platform model"). Add per-platform paths here, never as `#ifdef AB_PLATFORM_*` in the services. **`<platformName>.cores.cfg`** next to it (2026-09-18) is which core plays which RetroArch playlist on that platform (`<database name>=<part of a core display name>`, `#` comments), read by `RetroArchService` ahead of its `.info` mapping - was the one `coreOverride.cfg` for every platform; the Pi's prefers Genesis Plus GX (picodrive's Cyclone core segfaulted on the Pi 400), plain Snes9x and blueMSX. Tested in `tests/core/test_platform_config.cpp`. |
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
`default`, `evolution`), the two console tools under `Apps/`, `RetroArch/`'s skeleton, `Docs/` (the 0.9.0 manuals and release notes - at the root until 2026-09-20). `ab2`'s launcher menu icons (gear, gamepad, memory card,
the save-state frame - which must keep its 68x52 window at (25, 33), where `PsMenu::render` pastes the picture)
and its blue `on.png`/`off.png` switch are drawn by `tools/make_ab2_icons.py` (2026-09-18); the tile sits high in
the 118 slot so it clears the footer bar in the launcher's Games state. Where the resume icon takes the picture is
the theme's `launcher.menuIcons.resumePicture` (`ThemeRect`, unset = the original (25, 33) 68x52); ab2 centres it on
its tile, and `resumeSlotLabel` (`ThemePoint`, 2026-09-20) is where the resume-slot picker writes "Slot n" on its
2.7x copy of the icon - unset = the original spot, so older themes are untouched. ab2's classic font is **Selawik Light** (`selawik-light.ttf`, OFL, Microsoft's open metric-compatible
replacement for Segoe UI) since 2026-09-18 - `sul.ttf` was Segoe UI Light itself, not redistributable and with its
`(` `)` cut out; the console's SST fonts and Typodermic's Zrnic in the other themes are as they always were. `payload_linux/` next to it is the Raspberry Pi installer
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
