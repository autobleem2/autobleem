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
autobleem-main's `docs/archive/legacy-1x-analysis.md`) - `slotIsActive`/`pictureForSlot`/`lastPicture`/`storePictureForSlot`/`removeSlot`/
`exitedCleanly`, plus `prepareForLaunch`/`saveAfterLaunch` that the PCSX interceptor used to hold. Its
header documents the file layout. Two naming quirks callers depend on: slot 0's picture has no number in
its name, and `lastPicture()` uses slot 0's picture name whichever slot it finds.

`GameSettingsService` (`core/services/game_settings.*`) is what the game editor edits: `open(game)` gives a
`GameSettings` (the game's Game.ini as an `IniFile` - filled in from the record for an internal game, which
has no file - plus the nine `pcsx.cfg` values), and one setter per editor option writes it back with the
encoding PCSX expects (0/1 flags decimal, levels hex, every `!SaveStates` copy via `ConfigFileEditor`).
The favorite/play-using-RA toggles live here (Game.ini for USB, internal.db for internal). `GuiEditor` is
now only the screen: callers set `gameData` and `show()`.
**A game's PCSX config has one source** (2026-09-24, `PcsxConfig`, `core/services/pcsx_config.*`): the
launcher's pcsx.cfg, or - once an emulator's menu saved "Save settings for this game" - the game's own
`!SaveStates/<folder>/.pcsx/pcsx.custom.cfg`, which both emulators load over pcsx.cfg and are the only
writers of. `PcsxConfig::value()` is what the launch and the editor read (the custom line first);
`GameSettings::custom` makes every setter a no-op and the editor greys its Video/Emulator rows behind an
"Unlock the settings" row (`GameSettingsService::unlock` deletes the file). A legacy `autobleem.cfg` /
`cfg/<label>-<id>.cfg` (the retired "Save AutoBleem config") becomes the custom file on open or launch
(`migrateLegacy`, the newest wins), and edits no longer touch `cfg/*.cfg`.

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
   to rewrite per install), is gone: `LaunchService::selectionScriptFile()` is `<runtime>/autobleem_cfg.sh`
   (was `<rc>/` until the quiet-stick work of 2026-09-24), and
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

## The rest of the project (autobleem-main)

The project-wide knowledge - the platforms, the download site, the release channels, the build and
CI across the repositories, the owner's standing rules - lives in **`autobleem2/autobleem-main`**
(`CLAUDE.md`, `docs/`). These sections moved there on 2026-09-23, to `docs/history/`:

- Raspberry Pi port (2026-09-17) -> `docs/history/raspberry-pi.md`
- The PC USB stick (2026-09-20, `pcusb`) -> `docs/history/pc-usb-stick.md`
- The Windows product (2026-09-20, `win`) -> `docs/history/windows-product.md`
- The online update (2026-09-20, a Pi and the dev hosts - never the console) -> `docs/history/online-update.md`
- pcsx-abnxt - the next emulator (`github.com/autobleem/pcsx-abnxt`, started 2026-09-20) -> `docs/history/pcsx-abnxt.md`
- RetroArch for the console (`github.com/autobleem/retroarch-psc`, 2026-09-19/20) -> `docs/history/retroarch-for-the-console.md`
- Console tools (`apps/`, 2026-09-18) - and one PC tool - moved out on 2026-09-23 (see above) -> `docs/history/console-tools-in-the-launcher.md`

This file keeps what is the launcher's own: its code, its build, the console runtime its payload
drives, and its conventions.

## The virtual gamepad for Apps (`apps/abpad/`, 2026-09-22)

The third-party Apps on a stick were compiled by other people against other pads, so they take the
console's badly or not at all: the PSC pad's `b0` is Triangle and `b2` is Cross, its d-pad is on two
axes, and it has no hat and no sticks, while nearly every Linux port was written for the wired Xbox
360 pad (`a:b0`, d-pad on hat 0, triggers on axes 2 and 5). RetroBoot papered over this by injecting
a pad configuration into the app's process; this is that, done deliberately and from our own
`gamecontrollerdb.txt`. The design and the reasoning are in `docs/virtual-gamepad-plan.md`.

**`abpadd`** is the one process that reads the pads. It runs SDL2 and reads every pad through the
**GameController API** with our database, so a pad resolves in an App exactly as it resolves in the
launcher and in pcsx - the same code reading the same file - and publishes each player's state in
shared memory (`/tmp/abpad.state`, a seqlock: the reader is inside somebody else's game loop and must
never block or hold a lock). It watches SDL's device events with a slot per player, so player two
unplugging does not shuffle player three into their place, and a pad no database knows is guessed at
and handed to `SDL_GameControllerAddMapping` rather than left invisible - SDL only offers a *mapped*
pad as a GameController. `--probe` says what SDL makes of the pads and through which driver, `--watch`
says what is being published; between them they answer "is it the daemon or the app?" without a
debugger.

**It must run as root.** SDL reaches a modern pad through hidapi, which needs `/dev/hidraw*`, and
those are root-only; without them it falls back to evdev, where the same physical pad has a different
GUID (SDL stamps the driver into it - a trailing `h` is hidapi), a different button layout, and may
match a quite different database line. The launcher runs as root, so anything less makes the daemon
resolve the pad differently from the launcher, which is the one thing it exists not to do. It says so
if it cannot get at hidraw. (This is also the explanation for the Pi 400 pad "re-enumerating" noted
under the Pi port: the same pad seen through two drivers.)

**`libabpad.so`** is preloaded into each App by `rc/app_env.sh` and answers its SDL with a pad it
understands - the wired X360 pad by default, or the PSC's own (`virtual=psc`). It has **no SDL of its
own**: `nm -D -u` finds not one undefined SDL symbol, so it can go in front of an SDL 1.2 app, an SDL2
app or anything else, and every SDL function it calls is found at run time in whatever the app
brought. One library serves both ABIs, which it tells apart by asking the *loaded* library - `dlsym(
RTLD_NEXT, "SDL_SetVideoMode")`, which exists only in SDL 1.2. **Asking the other way round would be
wrong** wherever `libSDL-1.2.so.0` is **sdl12-compat** (SDL 1.2's API on top of SDL2, which is what
Debian ships today): both libraries are then in the process, an SDL2 symbol is found, and an SDL 1.2
app would be answered in SDL2's event structures - unrelated layouts, so corruption rather than a
misread. It covers both the joystick and the `SDL_GameController*` families, ~64 entry points, because
an app using the controller API never calls the joystick ones (SDL2 reaches its own through the dynapi
`_REAL` symbols, which a preload cannot intercept) and would otherwise be the one app with a different
pad. **Every function taking a handle has to be there** or the app hands our pointer to the real SDL;
the two that return a struct by value (`GetBindForButton`/`ForAxis`) are the only ones left out, and
say why.

Other things it does, each because an App needed it: the **d-pad and the left stick feed each other**
(`movement=`, default `both`) - Chocolate Doom is configured with `joystick_x_axis`/`y_axis` and never
looks at a hat, others read only a hat, and no choice of layout fixes that because the layout decides
what the pad *has*, not what the game reads; **no mouse cursor** (`cursor=`), since these machines have
no mouse and it is the app that asks for one; and a **way out** - holding Start+Select escalates
through one gesture, the shim asking the app to quit at ~1.5 s, the daemon (which knows the app's pid)
sending SIGTERM at 3 s and SIGKILL at 5 s. It must be *held*: Start and Select are live buttons in most
games.

Layout: `apps/abpad/src/core/` is `abpad_core`, which links **nothing** - not even `ab_core` - because
it ends up inside a shared object mapped into someone else's process; `src/daemon/` is `abpadd`;
`src/shim/` is the preload (`sdl_abi.h` spells out both SDLs' event structures rather than including a
header, since the shim is loaded into a process whose SDL we did not build); `src/padtest/` is
**`padtest`**, the instrument - a live panel drawn with terminal escapes, `--gc` for the controller
API, and **`--map`**, which asks for one control at a time and writes the gamecontrollerdb line itself
(the pscbios wizard's job, in the one place a Pi or a PC has no wizard). Tests in
`tests/apps/test_abpad_core.cpp`.

**`rc/app_env.sh` on both platforms** now starts the daemon (`--watch-pid`, so it goes when the App
goes), preloads the shim, and points an App at `rc/pad.default.ini` plus its own `Apps/<name>/pad.ini`.
It also gives the App **a home on the stick**: `$HOME` is `<root>/Home` with the XDG variables under
it, because an App left alone writes to `/root/.local/share/...` - the machine's own storage, which is
not ours to write to and does not travel with the stick. Note that exFAT and FAT cannot hold a symlink,
so a program wanting one under `$HOME` (PulseAudio tries) logs a warning; harmless so far.

**Proven on a Pi 400** with five Apps built for it (not in this repository - see below): SDLPoP
(GameController API), OpenTyrian and OpenJazz (raw joystick), OpenJazz again built with `LEGACY_SDL=ON`
(the SDL 1.2 ABI, through sdl12-compat), and Chocolate Doom (both APIs plus a GUID lookup). **Not yet
run on a console.** Two lessons from getting there worth keeping: an App on an appliance must **ship
its configuration**, because there is no setup tool reachable from a launcher (Chocolate Doom's pad was
simply switched off in its own config, and Doom needs `joystick_index`/`joystick_guid` as well as
`use_joystick 1`); and when the shim seems to do nothing, **`LD_DEBUG=bindings`** names the object each
symbol bound to in one line, which is the first diagnostic and not the last.

The test Apps are deliberately **not** in this repository: Freedoom may be redistributed and the Jazz
Jackrabbit shareware may not, and neither question is settled by us building them. A fetch-and-build
recipe is the clean route if they are ever to ship.

## Multi-platform Apps (2026-09-24, `docs/app-format-plan.md`)

**One App folder, a binary per platform.**
- `Apps/<name>/` keeps its shared files once (icon, data, `pad.ini`) and one binary per platform key in
  `bin/<key>/`.
- `app.ini` says which binary is which: `Exec.<key>=`, or one `Exec=bin/{key}/<name>` pattern.

**Platform keys.**
- `Env::appPlatformKeys()` is the ordered list this build accepts:
  - the target's own key first: `psc`, `rpi`, `rpi64`, `pcusb`, `win`, `dev`;
  - then its generic `linux-<arch>` / `windows-x86_64` key;
  - then the platform ini's `app_platform_keys=`.
- The console takes `psc` only, because nothing built against a current distribution loads there.

**Resolving an App: one rule, `AppManifest` (`core/services/app_manifest.*`).**
- The first key whose binary exists wins. `Args`/`Lib` resolve for that key. `Env=` is one
  `NAME=value;...` line with `Env.<key>=` on top: `IniFile` lower-cases keys, so there is no key per
  variable.
- `GameQueryService::apps()` leaves out an App with nothing to run here.
- `LaunchService::planApp` starts the App:
  - through `rc/app_run.sh`, or its own `Startup=` script, with `AB_APP_DIR/EXEC/ARGS/LIB/KEY`,
    `AB_PLATFORM(_KEYS)`, `AB_ROOT` and the ini's `Env` in the environment (`LaunchPlan::env`,
    `System::runAndWait`'s `env`);
  - directly on Windows (the resolved exe, `Args` split, `Lib` on `PATH`).
- An ini with only `Startup=` is an App of the old kind and is started exactly as before.
- `VirtualPad=true|false` (absent = true) says whether the App runs with the virtual pad mapper.
  `AB_APP_VIRTUAL_PAD` carries it, and `app_env.sh` skips abpadd and the preload when it is off.
- An App's source repository is named `app_<name>`, an extension's `ext_<name>` (the owner's rule).

**The scripts.**
- `rc/app_env.sh` is one file for every Linux target: the console's libs pack only where
  `Autobleem/lib/apps` exists, `AB_APP_LIB` first on the library path, home on the stick, the virtual
  gamepad.
- A `run.sh` started by hand resolves the ini itself through `rc/app_resolve.sh`, the rule in `sh`+`awk`.
  It reads the keys from `System/platform_keys`, which the launcher writes at start-up.
- `tests/rc/test_app_resolve.cpp` holds the shell copy to `AppManifest`'s answers. It also keeps the
  three scripts identical in `payload/` and `payload_linux/`. **autobleem-appliance's `payload_linux/`
  carries the same three files**: change all three copies together.

## Extensions (2026-09-24, `docs/extensions-plan.md`)

**What an extension is.** A plugin, `Extensions/<name>/` with an `extension.ini`:
- `Name`, `Description`, `Author`, `Version`, `Icon`;
- `Plugin=bin/{key}/<name>`, resolved by `AppManifest`, with `.so`/`.dll` added;
- `Background=true` to be polled every frame;
- `Network=required|optional|none` - `required` is refused offline.

It is installed by hand, run from the System menu's **Extensions** item (`GuiExtensions`), and never
bundled with a release. Its source repository is named `ext_<name>`.

**How it binds to the launcher.**
- It links against the launcher's own copy of the SDK. `autobleem-gui` is built with `ENABLE_EXPORTS`
  (`--export-all-symbols` on MinGW). On Windows a plugin imports from **`autobleem-gui.exe` by name**, so
  never rename the executable; `tools/ab_drive.py` runs `drive/autobleem-gui.exe` for that reason.
- A plugin never links the SDK's static libraries: `ab_add_extension()`
  (`autobleem-core/cmake/ab_extension.cmake`) gives it the headers and, on Windows, the import library.
- It logs through plog instance 1 (`PLOG_DEFAULT_INSTANCE_ID=1`), chained by `AB_EXTENSION` into the
  launcher's log with an `[<name>]` tag. Chaining instance 0 recursed on Linux.
- A plugin is built with **hidden visibility** (`ab_add_extension`); only the two `AB_EXTENSION` entry points
  are exported. With default visibility the Linux loader merges what two plugins both define (a static in
  an inline or template function is a GNU "unique" symbol): the plugins shared one plog instance-1 logger,
  and every line was logged once per extension, under each one's tag. `nm -D --defined-only` on a plugin
  should list `ab_extension_abi`/`ab_extension_create` and no `u` symbols.
- **ABI**: `AB_SDK_STAMP` in `gui/extension.h`, a macro on purpose. Bump `AB_SDK_ABI` whenever the layout
  of a class, or the signature of a function, an extension may use changes.

**Where the code is.**
- Core: `ExtensionCatalog` and `PluginLoader`.
- ab_classic: `gui/extension.h`, `ExtensionRuntime` (the crash guard: `System/Extensions/.active`, and
  `disabled.txt`) and `ExtensionHostBase`.
- The launcher: `App` (its `LauncherExtensionHost`, `takeExtensionRequests()`),
  `AutoBleem::run`/`runOutside` (start, crash guard, suspend/resume, shutdown) and `GuiLauncher`
  (the poll and its `extensionBubble`).
- `extensions/hello/` is the sample and smoke test, staged in `build_win/extensions/` and put on the dev
  stick by `make_usb.py`. It is built on a dev host only (`AB_BUILD_SAMPLE_EXTENSION`, off for every device
  target): it never goes into a package or onto a device (the owner's call, 2026-09-24).

## Where the code lives (2026-09-23) - read this before the sections below

The launcher takes **`lib_ableem`, `ab_core`, `ab_classic` and `ab_installer` from the `autobleem-core`
submodule** (`github.com/autobleem2/autobleem-core`, checked out at `autobleem-core/`): the one copy the
launcher and the tool repos share. Everything the sections below place at `lib_ableem/...`,
`src/code/core/...`, `src/code/app_base.*` or the classic `src/code/gui/...` files (Gui, ThemeAssets,
TextRenderer, Fonts, AppAudio, the splash/confirm/keyboard/about/hardware-info screens, the list-menu
framework) is under `autobleem-core/` now, at the same relative path; the version script and `version.h.in`
too (it stamps *this* repository's git describe - `AB_VERSION_SOURCE_DIR`). What stays here: `ab_ui` (App
and the game-aware classic screens - `src/code/app.*`, `gui/game_detail_pane.*`, the editors, Game Manager,
memory cards, playlists, Options), `ab_evoui`, the executable, `src/tools/`, `apps/abpad/`, `payload*/`,
`src/resources/`, the tools and scripts. A launcher source includes core as `"core/..."`, `"gui/..."` from
`src/code` (`ab_ui` has `src/code` as its include root). **A change to shared code is a commit in
autobleem-core, then a submodule bump here** (and in autobleem-console-tools / autobleem-pc-tools). The tests:
core's suites build from `autobleem-core/tests/` through the same `AB_BUILD_TESTS`, this repo's own are
abpad's on core's harness. Clone with `--recurse-submodules` (the workflows check out `submodules:
recursive`).

**The console tools and the PC programs are not built here any more**: PSC-Bios (an **extension** since
2026-09-24, `Extensions/pscbios/` - the launcher runs it in-process from Hardware Information) and ABFlashKit (an App) are
`autobleem2/autobleem-console-tools`, UpdateRoms, AutoBleemInstaller and AutoBleemWinSetup
`autobleem2/autobleem-pc-tools` (the installer's job, `InstallerJob`/`WindowsInstallJob`, is core's
`ab_installer`), each with its own CI and release; autobleem-appliance assembles them into the stick and the
Windows product. Their history in this tree is autobleem-main's `docs/history/console-tools-in-the-launcher.md`; their current docs are those repos'
CLAUDE.md files.

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
  `UsbGame::verify()` reasons are plain English: the scanner keeps them in `failedGames`, regional.db's
  `FAILED_GAMES` holds them, and the Game Manager lists and translates them (`gamesThatFailedVerifyCheck.txt`
  is gone - "The quiet stick").
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
  SDL 2.0.4 (`SDL_RenderGeometry` is 2.0.18, the console has 2.0.14).
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
target; `make_win.sh --product`), `pcusb` the 32-bit Debian PC stick - both done (autobleem-main's `docs/pc-targets-plan.md`,
the sections above).
The other per-target switches: the console tools and the PC programs are built in their own repositories
now (autobleem-console-tools, autobleem-pc-tools); `AB_ONLINE_UPDATE` is on for every target since
2026-09-23 - the console checks only with a network (autobleem-main's `docs/history/online-update.md`).

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
- **CI: one Docker image builds every target** (2026-09-19; autobleem-main's `docs/ci.md` is the operator's page,
  autobleem-main's `docs/archive/ci-plan.md` the plan until the workflows have run). `docker/Dockerfile` -> `autobleem-build`
  (Debian Bookworm, ~3.4 GB, built on the server with `docker/build-image.sh`): the native build with
  clang-format/clang-tidy **22** (apt.llvm.org, the major MSYS2 has), Debian's `crossbuild-essential-armhf`
  / `-arm64` with the multiarch `libsdl2*-dev` packages for the two Pis, `mingw-w64` (posix) with the
  official SDL2 mingw devel packages at `/opt/mingw-sdl2` for Windows, the three cover databases at
  `/opt/autobleem/db`, and **the console toolchain by AutoBleem-NG's recipe** under `/opt/psc`: a Debian
  Stretch armhf sysroot (`mmdebstrap --variant=extract` from archive.debian.org - glibc 2.24 / libstdc++
  6.0.22, the console's own), Stretch's **gcc-6** cross compiler (patchelf'ed RUNPATH to its own
  isl/mpc/mpfr/gmp, its libc linker scripts rewritten to bare names - no host `/usr/arm-linux-gnueabihf`
  hijack, that directory is the Pi cross libc's), and **SDL2 2.0.14 + image 2.6.3 + mixer 2.6.3 + ttf
  2.20.2 built from source** with the console's backend set (Wayland + dummy, GLES via EGL, ALSA, udev, no
  X11, no OSS - see "SDL2 on the console" below), wrapped as `armv8-sony-linux-gnueabihf-*` so `PSCtoolchainV8.cmake` works with
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
  `libwinpthread-1.dll`, and `UpdateRoms-<v>.zip`). **The workflows today** (2026-09-23, the compile-once
  model - autobleem-main's `docs/ci-org-migration-plan.md`): the image is built and pushed by
  **`autobleem2/autobleem-build`**'s own `image.yml`, from develop and master pushes (that repo is the
  Dockerfile's one source; this tree's `docker/` is a stale copy without llvm-mingw); here, **`test.yml`**
  is the test gate (`ci/build.sh native` on a hosted runner, every push and pull request) and
  **`publish-launcher.yml`** builds `launcher-<platform>-<v>.tar.gz` for psc/rpi/rpi64/pcusb/win on develop
  pushes and `v*` tags, and keeps the rolling `nightly` release current; **autobleem2/autobleem-appliance**
  assembles the packages and images from it and the other components' releases, and publishes them.
  The monolith's `ci.yml` (everything built here, the packages published from here) and `site-refresh.yml`
  (the site's RetroArch and cores - autobleem-build's `retroarch.yml` does that now) were deleted on
  2026-09-23. All gated by `AB_CI_ENABLED`. Verified 2026-09-19 on the server: all five targets green
  (37/37 tests, format, tidy), packages inspected. **Run on a console
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
  Run needs `C:\msys64\ucrt64\bin` on PATH (SDL DLLs). **`--no-tests`** skips ctest (37 s of every build; the
  language and format checks stay). **sccache** (`mingw-w64-ucrt-x86_64-sccache`, installed 2026-09-21) is put in
  front of gcc when present (`AB_NO_SCCACHE=1` opts out; not in `compile_commands.json`, so clang-tidy is
  unaffected): a clean rebuild is 38 s instead of 3 min - it does nothing for a sequential edit, that is
  ninja's job. What *did* cost 25 s on every no-change build was `version.h`'s `BUILD_TIMESTAMP` changing
  every run (14 objects + 5 links); it is now kept while tag/hash/branch/dirty are unchanged (see the
  Version row of the source map).
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

**The DebugDriver** (2026-09-21, `lib_ableem/include/ableem/ui/debug_driver.h`) is how the UI is tested: a dev
build started with `AB_DEBUG_PORT=<port>` (and `AB_NO_SPLASH=1`) - `AppBase` starts it, so the console tools
take it too (`ab_drive.py start --tool abflashkit`; PSC-Bios is an extension, reached through the launcher) - takes pad/keyboard input and hands frames back
over a socket - `press x`, `down l2`, `key escape`, `text abc`, `shot a.png`, `screen` (the class name of the
screen showing, from `GuiScreen::show`'s stack), `window hide|show`. `tools/ab_drive.py start|run|sheet|stop` is
the client (`run "menu 5; wait_screen GuiOptions; shot a.png"`); a whole walk through the screens takes seconds,
with the window hidden. `win_drive.ps1` is the old way, kept for a keyboard-only smoke test.

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
/media/System/Logs/               crash-<n>/ (a crash's logs, the last 3), saved-<n>/ (Hardware Information's Save
                                  logs), standby.log (failures only), update.log, installer.log; everything else -
                                  AB_out/AB_err, autobleem.log, launch/pcsx/retroarch.log - only with the `keep`
                                  marker there (see "The quiet stick")
/tmp/autobleem/                   the runtime dir (RAM): logs/, autobleem_cfg.sh (the selection), ra-append.cfg,
                                  ra-core-options.cfg, exit/ (the emulator's resume point), extensions.active
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
2.0.14 + SDL2_image/mixer 2.6.3 + SDL2_ttf 2.20.2 from the image, SDL2 with the **Wayland** video and **ALSA**
audio backends - the console has no X and no OSS; see "SDL2 on the console") → `bin/autobleem/run.sh` → `autobleem-gui /media`.
`/autobleem` existing on the console means the AutoBleem kernel is installed (`Env::autobleemKernel`: a real
clock, so "Last played" is shown).
Game launch: `rc/launch.sh` (PCSX, args: ssFolder, cdfile, lang, region, gameFolder, resume, aspect, filter, pad)
or `rc/launch_rb.sh` (RetroArch: file, core - our own script since 2026-09-20, see "RetroArch for the console";
an App's `run.sh` sources `rc/app_env.sh`). `LaunchService::writeSelectionScript()` writes `<runtime>/autobleem_cfg.sh`
(`AB_SELECTION=...`) which `rc/selection.sh` reads after `AutoBleem::run()`'s loop actually exits the process -
`MENU_OPTION_RETRO` (the L2+R2 system menu's RetroArch/EmulationStation item), `MENU_OPTION_UPDATE` (the
online update the launcher downloaded: `abupdate` from tmpfs lays it over the stick - autobleem-main's
`docs/history/online-update.md`; the launcher and `abupdate` fetch with **`abfetch`** (`src/tools/abfetch`,
our own HTTP/1.1 client over the vendored mbedTLS 3.6 in `third_party/mbedtls`, TLS 1.2, `cacert.pem` next to
it, certificate dates deliberately not checked - no battery clock), never with the kernel payload's curl) or `MENU_OPTION_POWEROFF` (see "The console's power off" below); starting a game and returning from one both loop back into the launcher
in-process and never reach it. `boot.sh` loops `autobleem.sh` -> `selection.sh` since 2026-09-22, so both
come back to the launcher without a reboot; `selection.sh` reboots for anything else (a crash, a missing
`autobleem_cfg.sh` - the file is deleted once read), which brings AutoBleem back up. The stock
SonyUI exit - `starter` mounted over `/usr/sony/bin/pcsx`, USB games linked into `/gaadata` with a `.lic`
each (`link.sh`/`overmount.sh`/`startsony.sh`) - is gone with it (2026-09-18, as in AutoBleem-NG), and so is
`.lic` handling in the scanner. RetroBoot's own update hook went the same day: `autobleem.sh` no longer
runs `retroboot/bin/init.sh` at boot, and the `/tmp/.rbpatching` guards, `rb_patch_background.sh` and
`rb_monitor.sh` are deleted - an RB_Patch dropped on the stick is not applied by AutoBleem any more.

### SDL2 on the console (2026-09-23)

The launcher, absplash, the console tools, pcsx-ab and pcsx-abnxt all run on the SDL2 family in
`Autobleem/lib/libs.tar.gz` (`/tmp/lib`, inherited through `LD_LIBRARY_PATH`): **SDL2 2.0.14**, SDL2_image and
SDL2_mixer 2.6.3, SDL2_ttf 2.20.2, built by the `autobleem2/autobleem-build` image (`/opt/psc/sdl2`, its
`docker/Dockerfile`'s psc stage) with the **Wayland** video and **ALSA** audio backends only - the console has
no X and no OSS; `ab-validate psc` fails an image whose SDL2 has x11 or oss, lacks wayland or alsa, or is not
2.0.12/2.0.14. `tools/make_psc_package.sh` and the `publish-launcher` workflow put the image's SDL2 family
into the archive at package time (the checked-in `payload/Autobleem/lib/libs.tar.gz` is the same set, for
builds without the image). **2.0.14 is the ceiling** (verified on a console the same day, and was 2.0.12 before):
the console's compositor is Sony's Weston 1.11, which offers `wl_shell` and no xdg shell, and 2.0.14 is the
last SDL with a `wl_shell` window - 2.0.16 removed it together with `zxdg_shell_v6`, and 2.0.20+ also need
libwayland >= 1.18 (the console has 1.12; 2.0.22's configure refuses it). The Wayland protocol code is
generated by a wayland-scanner 1.12 the image builds (a newer one emits `wl_proxy_marshal_flags()`, which
1.12 lacks). What a newer SDL would take, for later: a patch bringing `wl_shell` back as a fallback (what
retroarch-psc's `wl_shell_fallback.patch` did for RetroArch 1.22), with the 1.18/1.20 libwayland symbols made
optional - the route to 2.0.22 or 2.30 and `SDL_RenderGeometry` (2.0.18), which the carousel's turned covers
already use when the headers have it. SDL2's ABI is backward compatible, so a newer libSDL2 in the archive
never needs a rebuild of the programs.

### The console's power off (2026-09-22) - and how the exploit chain really runs

What the boot really is, from `tools/psc_mount_debug.sh`'s dumps (the two rounds are in the git log of this
entry): the console is **systemd** (`halt`/`reboot`/`shutdown` are `systemctl`); `powermanage.service`
(`/usr/bin/start_pman`) does its housekeeping and then **`echo mem > /sys/power/state` - the "1st
suspend"**, the standby every boot goes through before the power button; `usbwatch.service`
(`/usr/bin/usb_watch`) polls `blkid` every 2 s for a `SONY*`-labelled `sd[ab]1`, mounts it rw on `/media`,
finds `/media/028c18a9-.../` (one of Sony's eight update ids), gpg-"verifies" `LUPDATA.BIN` into
`/tmp/diag/028c.../start` and runs it - `red_led 14 0.2` (the 5.6 s blink) then `cd /media/Autobleem; source
./start.sh` - so **our whole chain is sourced into a shell whose script lives on tmpfs**; the only thing of
Sony's that ever holds the stick is that shell's cwd, which our `cd` moved there. The mount lands at ~6 s,
the suspend during the blink: the stick is mounted rw all through the boot standby (its dirty flag set),
and nothing of ours runs before it - that part is untouchable. Sony's own "power off" is `power_manage`
(`/data/power/*` = `/dev/shm/power`, `touch prepare_suspend`, `echo mem`), a suspend as well; the board has
no real halt, `shutdown -h now` (systemd, which unmounts `/media` cleanly first) reboots. **The USB bus is
reset by every resume** - hub, pad and stick re-enumerate within 2 s (the stick keeps its name, usually) -
and `power_manage` does not notice a suspend it did not start (`resume_count`/`usbreset_count` stay put).

So the launcher's **Power Off is Sony's power off with the stick unmounted** (verified on the console the
same day with `tools/psc_sleep_test.sh` before it was built): `App::requestPowerOff()` (the system menu's
item and the power button - `AutoBleem`'s constructor re-wires `Platform::setPowerOffHandler` on
`AB_PLATFORM_PSC`; every other build keeps `System::powerOff()`) sets `MENU_OPTION_POWEROFF` (7) and
`Input::requestQuit()` - poll() returns Quit on every call from then on, every screen's loop closes on
Quit, so the stack of screens unwinds and `AutoBleem::run()` leaves cleanly (databases closed, the scan
joined), "POWERING OFF... PLEASE WAIT" on the screen. `rc/boot.sh` is a loop now (`cd $RC; ./autobleem.sh;
cd /tmp; sh /tmp/selection.sh`), the udev rules file bind-mounted from `/tmp`, so nothing of ours is on the
stick while `selection.sh` (a copy on tmpfs) runs its `standby()`: `rm System/.session`, `umount /media`
(five tries; busy -> the holders into `System/Logs/standby.log` and a reboot), `abfatflag clean` when the
flag is ours (below), **green off, red on**, `echo mem`, and after the power button: green on, 3 s for
the bus, up to 30 s of `blkid` for the `SONY` partition, `mount` as usb_watch mounts it, `touch
System/.session`, exit 0 -> the launcher again - under the AutoBleem picture (`absplash` and
`splash/autobleem.jpg` copied to `/tmp` by boot.sh, shown from the resume until the launcher's
`display(false)` unlinks `/tmp/.abload`, as after RetroArch; the ten seconds were black and looked like a
console that did not start). No stick after 30 s -> reboot. The red LED alone is
"AutoBleem's standby" (the manual says so: the sign it works as intended). RetroArch (`AB_SELECTION=4`)
comes back through the same loop - `retroarch.sh` no longer re-runs `start.sh` nested.
**On the AutoBleem kernel** (2026-09-23, a tester's report: Power Off just restarted AutoBleem - then, with
the first fix, hung on a black screen with the green LED). The overlay's `/etc/autobleem/rndis` brings up a
USB network gadget (RNDIS) on the power port at every boot; `standby()` turns it off
(`/sys/class/android_usb/android0/enable`) and back on through the overlay's own `rndis restart` - **in the
background**, because its `start()` ends in `tcpsvd` (the FTP server), which stays in the foreground and
never returns. The gadget was not what refused the suspend, though: the kernel's own log (retests with the
tester, 2026-09-23) said `musb_bus_suspend: trying to suspend as a_host while active` / `Device usb1 failed
to suspend async: error -16` - the tester's stick sat on a hub in the **micro-USB (power) port**, which the
AutoBleem kernel runs as an OTG host (the stock kernel has no host mode there), and that host refuses
suspend-to-RAM while it serves a device. `shutdown -h now` (what 1.x did) only runs the drivers' shutdown
hooks, so nothing can refuse it. So: the write's result is checked, a refusal retried twice (logged to
`System/Logs/standby.log` with the wakelocks and the kernel's reason lines), and after the third
`poweroff_instead()` mounts the stick again (it never went away), appends the log, sets the red LED and
runs `shutdown -h now` - POWER is then a cold boot, not a quick wake. Confirmed by the tester the same
day, from the OTG hub and from a front port. **Never read `/sys/power/wakeup_count` in these scripts**: it
blocks while a wakeup event is in progress, which hung a diagnostic build on the red LED.

**The dirty flag** (`ableem::FatDirtyFlag`, `lib_ableem/engine/fat_dirty_flag.*`, tested; the CLI
`abfatflag DEVICE [clean|dirty]` in `src/tools/`, shipped next to `absplash`): the boot sector byte at
0x41 (FAT32) / 0x25 (FAT12/16) bit 0 - what Linux's fat driver sets on an rw mount and clears on umount,
what Windows' "scan and fix" keys on - plus FAT[1]'s ClnShutBit on a clear, and exFAT's `VolumeFlags`
bit 1 (excluded from the boot checksum). **The kernel never clears a flag it found set at mount time**
(`fat_set_state`'s `sbi->dirty` gate; it says "Volume was not properly unmounted" and leaves it), so a
stick pulled once during the boot standby would stay dirty for ever. `rc/checkstick.sh` (boot.sh, before
the launcher, nothing open for writing yet): copies the tool to `/tmp`, and when `System/.session` is
absent - the previous session ended through the standby - does `remount,ro` (the kernel clears a flag
it owns right there), `abfatflag clean` if it is still dirty (then it is ours: `/tmp/ab_stick_owned`, and
`abfatflag dirty` after the `remount,rw` to keep "mounted rw = dirty" true on disk), and `touch
System/.session`. A session that ended any other way - the stick pulled while the launcher ran, a crash
- leaves the marker, the flag stays, and Windows gets to repair real damage. `standby()` clears an owned
flag after its umount and forgets the ownership after the fresh mount (the kernel owns it again).
Nothing in any of this writes to the console's own storage (the owner's rule: `/data` included).

## Source map (`src/code/`)

`src/code/core/` is the `ab_core` static library (no SDL, no screens - see "Current work"): `main.h`, `model/` and `services/`, nothing else at its top level; `gui/` and
`app.*` are `ab_ui`; `evoui/` is `ab_evoui`; `main.cpp` and `autobleem.*`
are the executable (`AutoBleem::run()` shows `GuiLauncher` directly - see "Current work", 2026-09-17). `core/model/timing.h` holds `TicksPerSecond` and the showing-timeout
defaults, which both the services and the screens need.

| Area | Files | Notes |
|---|---|---|
| Version | `core/version.h` (generated) | `Version::VERSION` (the last git tag, else `AB_VERSION_FALLBACK` in CMakeLists - was `config.ini`'s `Version=` key, dropped on load now), `GIT_HASH`, `GIT_BRANCH`, `GIT_DIRTY`, `BUILD_TIMESTAMP`, `FULL_VERSION` (`v2.0.0-pre0 (master@a83777b*)`). Written by `cmake/generate_version.cmake` into `<build>/generated/core/` on every build (`ab_version` target; the header only changes when the facts do - `BUILD_TIMESTAMP` is kept from the existing header while tag, hash, branch and dirty flag are the same (2026-09-21), so it is when *this version* was first built, and a no-change ninja run is a no-op). The splash, About and the log's first line use it. Include as `"core/version.h"`. |
| Entry | `main.cpp` | Strips `--sysinfo`, has `EnvironmentSetup::fromArguments()` configure `ableem::Environment`, registers `SDL_Quit`, then constructs the one `AutoBleem` and calls `run()`. |
| `core/services/environment_setup.*` | `EnvironmentSetup` | The layouts a program can be started with (2026-09-18, was `main.cpp`'s `setupEnvironment()`): `fromRoot(root)` (everything under one root - the console's `/media`, the Pi's data partition, the 1-arg debug mode: `Games/`, `System/Databases/`, `Autobleem/bin/autobleem` as the resources dir, `Autobleem/bin/db`, `themes/`; the Sony data tree is the console's own or `<resources>/sony` under `AB_ROOT_RELATIVE_LAYOUT`), `fromDbAndGames()` (the 2-arg debug layout), `fromArguments()` (autobleem-gui's command line) and `forTool(argc, argv, name)` for a console tool in `Apps/<tool>` (optional root, `/media` by default on the console; pins `Env::getAppDir()` - the tool's own folder, `getPathToAppLangDir()` its `lang/` - before anything can chdir). Every one applies `PlatformConfig`. The only place besides `Env::platformName()` that spells `/media` or `/usr/sony`. Tested in `tests/core/test_environment_setup.cpp`. |
| `autobleem.*` | `AutoBleem : App` | The program: `run()` opens the DBs, restores memcards, requests a scan up front when `games.fingerprint` doesn't match (or is missing, or there are loose game files, or `gamelist.xml` is gone), starts `scans()` and shows the splash, then loops `GuiLauncher` directly - `MENU_OPTION_START` → `launchGame()` (watching paused around it) → back to the launcher; `MENU_OPTION_RETRO` and `MENU_OPTION_POWEROFF` (the console's standby, `App::requestPowerOff()` - see "The console's power off") exit the loop. Chooses the `ProcessRunner` the launch service forks with (a splash on the dev host). In the executable, above both UI libraries. |
| `app_base.*` | `AppBase` | The model of any program drawn with the classic UI: `Config`, `Lang`, `Theme`, `Clock`, the `Gui` singleton (whose window title it sets - `Gui::setWindowTitle` before the first `getInstance()`) and `AppAudio`. Top of `ab_classic`; every `GuiScreen`'s `app` member is one. `AppBase::get()` for the non-screens (Gui, Theme, AppAudio, Fonts). |
| `app.*` | `App : AppBase` | AutoBleem's model on top of it: the `GameLibrary`, the `Session`, every service (including `ScanService`, `app.scans()`). Top of `ab_ui`. `App::get()` is a `static_cast` of `AppBase::get()`; a game-aware screen declares its own `App &app = App::get();` over `GuiScreen`'s `AppBase &app` (the seven that do: the two game editors, Game Manager, memory cards, playlists, select-memcard, `GuiLauncher`). |
| `core/model/session.h` | `Session` | Where we are across one run: `menuOption` (`MENU_OPTION_IDLE`/`RETRO`/`START`/`UPDATE`/`POWEROFF` - the classic-UI values are gone), the game being started (`runningGame`, `EmuMode`, `resumePoint`), and `launcher`, the carousel's `GameSetSelection`. |
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
| `gui/screens/gui_*` | | The rest of the classic screens, shown from the launcher's L2+R2 system menu or its sub-screens: About (`credits` settable by the caller, AutoBleem's by default - a tool shows its own), Confirm dialog, on-screen Keyboard (`GuiKeyboard`, rebuilt 2026-09-24 as ABI 3: pages of letters, symbols - `/ \ : ? & = % @ #` and the rest a URL, path or password needs - and two of accented letters, a function row with Shift/caps lock, the page key, Space, Backspace and Done; L1 Shift, R1 the next page, L2/R2 move the cursor; a USB keyboard types alongside the pad, Esc cancels, and while it shows a dev host's keyboard-as-pad is off; UTF-8 by whole characters; keys and field drawn as plain text, never parsed for `|@X|` markers), memcard select, `GuiTextPage` (a titled page of static `lines`, Circle back - a tool's instructions). `gui/starfx.*` is the star field the About screen draws. (`GuiScrollWin`/`GuiPadTest` were deleted on 2026-09-18 - nothing had shown them since the classic menu went.) |
| `gui/screens/gui_facts_page.*`, `gui_action_menu.*` | `GuiFactsPage`, `GuiActionMenu` | Two reusable classic screens (2026-09-21, for the console tools): a facts page - sections with a heading band and label/value rows, scrolling, re-read every `refreshInterval`, a subclass gives `title()`/`collect()` and takes its own buttons through `onButton()`/`extraHints()` (Hardware Information and PSC-Bios's opening screen) - and an action menu in the system menu's look (rows of a name over a description, Cross picks into `result`, Circle leaves; ABFlashKit's screen). |
| `gui/screens/gui_hardware_info.*` | `GuiHardwareInfo` | The Hardware Information screen (2026-09-18) for a machine without PSC-Bios, a `GuiFactsPage` of `SystemInfoService`'s sections plus a "Display and input" one only the running program can fill (render driver + MSAA, video driver, display mode, canvas/scale, audio driver, SDL version, the pads by name - `Platform::linkedVersion/videoDriverName/displayModeString`, `Renderer::driverName`, `Input::pads`). Classic layout, rows paged like Options, re-read every second, Up/Down a row, L1/R1/Left/Right a page, Circle back. `autobleem-gui <root> --sysinfo` prints the same sections (minus the display/input one) to stdout and exits - for bug reports and for checking the Linux branch over ssh; verified on the Pi 400 (64-bit kernel: no `model name` in cpuinfo, the core comes from `armCoreName()`'s part-id table). The system menu's item runs the PSC-Bios extension (`app.extensions().run("pscbios")`, `Extensions/pscbios/`, the console's only - 2026-09-24; it was `Apps/pscbios/run.sh` before) and shows this screen wherever that does not run: a Pi, a PC, a console without it, one of another `AB_SDK_ABI`, one the crash guard disabled; a PSC-Bios that failed mid-run is a notification line instead. |
| `core/services/system_info.*` | `SystemInfoService` | What that screen shows, SDL-free: `collect()` = `system()` (os-release/uname, hostname, uptime, load; the registry on Windows), `hardware()` (device-tree model, cpuinfo, cpufreq, thermal_zone0, meminfo), `storage()` (the data root first, then every block filesystem in `/proc/mounts` - or the fixed/removable drives - with `statvfs`/`GetDiskFreeSpaceEx`), `network()` (IPv4 per interface, `getifaddrs`/`GetAdaptersAddresses` - ab_core links `iphlpapi ws2_32` on Windows), `software()` (version, build, platform, roots, RetroArch). The parsers and formatters are static and tested (`tests/core/test_system_info.cpp`). |
| `gui/gui_font.*` | `Fonts`, `FontEnum` | Theme/Sony SST font loader built on `ableem::Font` (SDL_FontCache itself is now in lib_ableem). |
| `evoui/screens/evoui_launcher.h`, `evoui_launcher_screen.cpp`, `evoui_launcher_input.cpp`, `evoui_launcher_actions.cpp` | `GuiLauncher` | EvolutionUI, the only screen `AutoBleem::run()` shows, in three files: the screen (assets, the sets - PS1 all/internal/favorites/history/sub-dir, RetroArch playlists, Apps - the metadata panel, state transitions, `render()`), the input (the event loop - polls `app.scans()` once a frame via `applyScanUpdate()`, before `render()` - and per-button handlers, L2+R2 among them), and the actions (what Cross does per state and menu icon, and L2+R2's system menu). Holds the `Carousel` as `carousel`. A black overlay fades out over `LauncherFadeInDuration` every time the screen is shown (`fadeAlpha`/`fadeStart`). `scanStatusLine` (bottom of the screen) shows the scan's progress or its "Scan complete" summary; `reloadGames()` re-runs the current set's query and re-selects the same game by id whenever the roster changed and no scroll animation is running; a highlighted game that vanished (folder pulled, or merged by the scan) falls back to the set's first game, closes a resume-slot picker that was showing its slots, and keeps the cover raised while the game menu is open (`Carousel::snapMainCover`). |
| `evoui/screens/evoui_set_picker.*` | `GuiSetPicker` | What Select opens (2026-09-21; Select used to cycle the sets and L2+Select open a folder or playlist picker): a panel with three icon tabs - PlayStation, RetroArch, Apps - L1/R1 between them, and the groups of the tab as rows (all/internal/the folders/favorites/history/light-gun games; a playlist each; the one Apps group) with their game counts, Up/Down and L2/R2 a page, Cross picks. It fills a `GameSetSelection`; `GuiLauncher::loop_chooseSet()` applies it. **L2/R2 page on every list since the same day** (the menu base, the memcard picker, Hardware Information, the text page); L1/R1 go to the first/last row. |
| `evoui/screens/evoui_system_menu.*` | `GuiSystemMenu` | The L2+R2 overlay (either order - `powerOffShift` + `r2Held`, 2026-09-18): Re-Scan Games, RetroArch/EmulationStation, Memory Cards, Game Manager, Hardware Information (PSC-Bios on the console, `GuiHardwareInfo` elsewhere), Options, About, Power Off - everything the classic main menu used to offer, Power Off included (no more direct L2+R2 shutdown). A dumb picker: a panel over the launcher's dimmed background (`background` texture handed in by the launcher), launcher fonts and theme colours, the panel as tall as its rows need and scrolling with edge markers when more than fit, the launcher's X/O hint icons in its footer; Up/Down + wrap, Cross/Circle - it returns a `SystemMenuAction` and `GuiLauncher::loop_openSystemMenu()` runs it. |
| `evoui/carousel.*`, `carousel_game.*` | `Carousel`, `PsCarouselGame` | **Two kinds of box** (2026-09-18): a PS1 game is the art in the theme's jewel case (`cdJewel`, thin - `JewelCaseThickness` 8%); a RetroArch game or an App is a **big box** - the art at its own aspect (tall NES, wide SNES) with `evoimg/bigbox.png` laid over it as a 9-slice (`drawNineSlice`, 7 px border; `tools/make_bigbox_frame.py` draws the file, replace it with real artwork any time) and `BigBoxThickness` 22% deep. `PsCarouselGame::content` is where the box is in the 226x226 texture and `thickness` its depth; `renderTurnedCover` turns the box about *that* rect and puts the spine on its edge, so a tall box no longer has its spine floating in the transparent part of the texture. The row of covers: `games` (exactly the set's games, a bounded row - see "Conventions"), `selected`, the 13 screen positions, the scroll/moveMainCover animations, texture load/free on visibility, `render()`. **Cover flow** since 2026-09-18: `PsScreenpoint::angle` (degrees about the vertical axis, negative = left of the middle, facing in) is interpolated like x/y/scale; `PsCarousel::createCoverPoint(distance, side)` lays out the `PsCarousel::SideCovers` (14 - enough that the outermost slot is off a 1280-wide screen, so a cover scrolls in from the edge rather than popping up) slots a side as a shelf receding from the middle: the nearest at half size 190 px out, each further one 3.5% smaller, 15 shades darker, a step (50 px, scaled with the cover) further out and turned more (40..72°) - the shrinking is what makes an inner cover drawn over an outer one read as being in front of it; `render()` draws far-to-near, the selected cover as a plain copy and every turned one via `renderTurnedCover()` - front face through `Renderer::copyTrapezoid`, plus a spine (`CoverThickness` = 8% of the width, textured with a strip from the cover's near edge, darker) and a Lambert-ish darkening with the turn. `ViewerDistance` (600 px) is the perspective strength. |
| `evoui/controls/evoui_*.{h,cpp}` | `PsObj` and subclasses | The EvolutionUI controls: the animated elements the launcher is built from (`PsObj` base, meta panel, menu, buttons, labels, the state selector). Class names keep their `Ps` prefix. `PsMeta` shows a RetroArch game the database knows as title / "publisher, year" / core / "n Players" (2026-09-19); one it does not know as title / core, as before. |
| `core/model/ps_game.*` | `PsGame : ableem::GameRecord` | Game as seen by the UI (from DB via `PsGame::fromRecords`, or playlist). `PsGamePtr = shared_ptr<PsGame>`. Adds the RetroArch/App fields. A plain data record - the resume points are `ResumePointService`'s, the memcard `MemcardService`'s. |
| `core/services/game_catalog.*` | `GameCatalogService` | The writes: play history ranking, game delete, cover flush. Owned by `App` (`app.gameCatalog()`). |
| `core/services/resume_point.*` | `ResumePointService` | The save-state slots in a game's `!SaveStates` folder, and the prepare/save around a PCSX launch. Owned by `App` (`app.resumePoints()`); non-screens reach it via `App::get()`. |
| `core/services/memcard.*` | `MemcardService` | The `!MemCards` sets and a game's chosen card; the swap in/out around a launch. Owned by `App` (`app.memcards()`). |
| `core/services/game_settings.*` | `GameSettingsService` | The game editor's model: a game's Game.ini flags and pcsx.cfg values, read with `open()` and written one setter per option. A game with its own config (`PcsxConfig`) is read-only until `unlock()`. Owned by `App` (`app.gameSettings()`). |
| `core/services/game_query.*` | `GameQueryService` | Which games a set shows and in what order - `gamesFor(selection)` is the whole of the old `switchSet` query. Owned by `App` (`app.gameQuery()`); RetroArch arrives through the `RetroArchGames` interface. |
| `core/services/retroarch.*` | `RetroArchService` | RetroArch's playlists as sets of foreign `PsGame`s: `.lpl` parsing (both formats via `ableem::RetroArchPlaylist`), the core for an entry from its `ableem::CoreInfoTable` (`info/*.info` + `platform/<platform>.cores.cfg`, `coresCfgPath()`), Favorites/History, `reloadPlaylists()` after the scan rewrote them, and `ensureMetadata()` - publisher/year/players from `<rdb dir>/<playlist>.rdb` by label, read once per playlist on first use and dropped again (Favorites/History copy from the source playlist). Implements `RetroArchGames`. Owned by `App` (`app.retroArch()`). |
| `core/services/launch.*`, `process_runner.*` | `LaunchService`, `ProcessRunner` | A game launch start to finish: argv for `rc/launch.sh` (PCSX) / `rc/launch_rb.sh` (RetroArch) / an App's `startup`, the memcard and resume-point work around it, the RetroArch config transfer, `writeSelectionScript()`. Runs through a `ProcessRunner`. Owned by `App` (`app.launcher()`). |
| `evoui/screens/evoui_mc_manager.*`, `evoui_app_start.*`, `evoui_btn_guide.*` | | Launcher sub-screens. |
| `evoui/controls/evoui_notification_line.*`, `evoui_notification_bubble.*` | `NotificationLines`, `NotificationBubble` | The launcher's notifications, all in one look (2026-09-21): `NotificationBubble` is a PanelStyle sheet at the right edge that slides in and fades out (the scan's progress with a bar, 440 wide); each `NotificationLine` (0: "Showing: ..." for the set, 1: messages and the jump letter) is one too, fitted to its text, and `GuiLauncher::render` stacks them under the scan's bubble at the top-right corner. `show()` takes the time from the platform, so a line set before the first frame keeps its hold. |

| `apps/abpad/` | `abpad_core`, `abpadd`, `libabpad.so`, `padtest` | The virtual gamepad for third-party Apps - see its own section above and `docs/virtual-gamepad-plan.md`. Built for every target but `win`; the packages ship it as `Autobleem/bin/abpad/`. |
| (autobleem-console-tools) | `PscBios`, `AbFlashKit` | The console tools live in their own repository since 2026-09-23: PSC-Bios an extension (`Extensions/pscbios/`), ABFlashKit an App. |

Payload (`payload/`): the release USB tree — `rc/*.sh` scripts, themes (`ab2`, `aergb`, `autobleem`,
`default`, `evolution`), the two console tools under `Apps/`, `RetroArch/`'s skeleton, `Docs/` (the 0.9.0 manuals and release notes - at the root until 2026-09-20). `ab2`'s launcher menu icons (gear, gamepad, memory card,
the save-state frame - which must keep its 68x52 window at (25, 33), where `PsMenu::render` pastes the picture)
and its blue `on.png`/`off.png` switch are drawn by `tools/make_ab2_icons.py` (2026-09-18); the tile sits high in
the 118 slot so it clears the footer bar in the launcher's Games state. Where the resume icon takes the picture is
the theme's `launcher.menuIcons.resumePicture` (`ThemeRect`, unset = the original (25, 33) 68x52); ab2 centres it on
its tile, and `resumeSlotLabel` (`ThemePoint`, 2026-09-20) is where the resume-slot picker writes "Slot n" on its
2.7x copy of the icon - unset = the original spot, so older themes are untouched. `launcher.colors.selection`
(2026-09-21) is the picker's colour for the selected slot: a halo in it around the tile (the tile's own alpha, drawn
larger twice with additive blending, so it follows any tile's shape) and the other tiles dimmed; unset = the original
red tint of the selected tile, which only ever showed on a white tile (ab2's cyan tile made it invisible). **Open: the
pad is dead for 1-3 s after every game (and at boot) on the Pi 400** - its multi-mode "PS4/PC/PS3/Android" pad
re-enumerates right after the launcher opens it (the log: disconnect, back as an "Xbox 360" pad a second later, back
as itself three seconds after that), which is SDL's hidapi driver probing the pad's HID reports.
`SDL_HINT_JOYSTICK_HIDAPI=0` (evdev instead) stopped the re-enumeration but the same pad then came up with another
GUID and a mapping with Triangle/Square swapped - reverted the same day; a fix has to keep hidapi's mapping (a
gamecontrollerdb line for the evdev GUID, or not closing the pad around a game at all). ab2's classic font is **Selawik Light** (`selawik-light.ttf`, OFL, Microsoft's open metric-compatible
replacement for Segoe UI) since 2026-09-18 - `sul.ttf` was Segoe UI Light itself, not redistributable and with its
`(` `)` cut out; the console's SST fonts and Typodermic's Zrnic in the other themes are as they always were. `payload_linux/` next to it is the Raspberry Pi installer
package, not part of the USB tree (see "Raspberry Pi port"). `db/` is git-ignored (cover DBs live there).

## UI styling standards (2026-09-21, the `feature/ui-fixes` pass)

Every screen but the launcher's own carousel frame draws in **one look**, and new screens must too:

- **`PanelStyle`** (`gui/panel_style.*`) is the look: the screen behind dimmed (`dim`, black 110), a sheet
  (black 200) with a 1 px edge in the launcher theme's *secondary* colour, a **header** (`header`: the title in
  `FONT_28_BOLD` at `RowInset` (24) + 18 from the top, a rule 8 px above the header's 74 px end), rows,
  and a **footer** band (`FooterHeight` 54). Colours come from `launcher.colors` (`text`, `secondary`,
  `hint`) - never hard-coded. `Gui::panelStyle()` resolves it for the current theme.
- **Two panel shapes.** A *full* panel (the classic screens: Options, the editors, Game Manager, Memory
  Cards, Hardware Information, the keyboard, pages): `Gui::renderTextBar()` + `renderHeader(title)` +
  rows + `renderStatus(hints)`; its rect is the theme's `classic.menuPanel` down to the status line
  (`Gui::classicPanel()`), rows live in `classicContent()`, the footer in `classicFooter()`. A *compact*
  panel centred on the screen (the system menu, the set picker, the update prompt, Confirm): 800 wide,
  as tall as its rows, `PanelStyle::Margin` (40) from the edges, the launcher's captured frame under it
  (`renderer.captureNextFrame(); render(); background = renderer.lastCapture()`). A dialog with one
  question is compact, never full.
- **Rows.** Text at `RowInset + 8` (32 px) from the panel's edge - the header's text x. The classic
  screens' rows use the theme's classic font at its own line height, one under the other, **as many as
  fit** (`Gui::classicRowsThatFit(font)`), scrolling a row at a time with **markers**
  (`Gui::renderScrollMarkers` - triangles at the content's right edge). The selected row is
  `renderSelectionBox`: a band in the text colour at alpha 38 with a 5 px bar at the panel's left edge
  (`PanelStyle::selection`); a heading between rows is `renderLabelBox` (a faint band). A row that cannot be changed is drawn, then greyed over
  with `renderDisabledBox` (`PanelStyle::disabled`, black at alpha 150) - still selectable, so the cursor
  can pass it. Compact panels
  use `PanelStyle::RowHeight` (60: `FONT_22_MED` title + `FONT_15_BOLD` description) or 44 for a
  single-line row.
- **Values right-aligned.** An option row is its label at the left and its value at the row's right
  edge: a boolean's switch (`renderTextLineOptions`, the theme's on/off image with its transparent margin
  measured so the art meets the edge) or text (`renderRowValue`). A screen with a pane on the right
  passes the pane's `rowsRight` as the edge.
- **The detail pane** (`gui/game_detail_pane.*`, 360 wide) is the right side of any screen about one
  game: the cover on a plate, a screenshot when there is one, then facts as `FONT_15_BOLD` label over
  `FONT_20_BOLD` value, a rule to its left.
- **Footers are structured** and drawn by `PanelStyle::footer` from the `"|@X| Label  |@O| Label"`
  protocol (`parseHints`): the hints **sorted** Cross, Circle, Triangle, Square, Start, Select, L1/R1,
  L2/R2, keyboard keys; icons 30 px (the launcher's hint images for X/O/T, the theme's buttons for the
  rest); labels in the largest launcher font that fits; a counter ("Game 3/21") at the right edge in the
  secondary colour. **Labels**: Circle is "Back" wherever leaving loses nothing, "Cancel" only where
  Cross commits; Cross names its action; sentence case ("Delete game"). **Paging is L2/R2 everywhere**,
  L1/R1 go to the first/last row (or switch tabs where there are tabs).
- **Fonts.** Titles/labels: the launcher pair (`themeFonts[FONT_28_BOLD/22_MED/20_BOLD/15_BOLD]`, Open
  Sans); classic rows: the theme's classic font (`assets().themeFont`, Saira / Selawik); never a
  hard-coded ttf path - the shipped ones are `Env::getPathToFontsDir()`'s.
- **Waiting.** A long job on the main thread runs inside `Gui::beginBusy(message, redraw)` /
  `endBusy()` with `Gui::tickBusy()` in its loops (the spinner over the dimmed screen); a blocking call
  with no loop goes through `Gui::drawText(message)` (background, logo, spinner). Background work
  reports in the launcher's `NotificationBubble` (top-right, slides in and out), never in a status line.
- **Every string on screen is `_()`** and lands in all 16 language files in the same commit
  (`tools/lang_tools.py extract`/`update`, then translate); no `=` in a key.
- **Testing a screen** is `tools/ab_drive.py` (`start --show`, `run "menu 5; wait_screen GuiOptions; shot
  a.png"`, `sheet`, `stop`); every screen's class name is what `wait_screen` takes.

## The quiet stick (2026-09-24, `docs/quiet-stick-plan.md`)

**The data root is written only when the user's state changes** - a save, a card, a kept resume slot, a
setting the player changed, a game added or removed. Everything else is in RAM or not written at all.
Branch `feature/quiet-stick` in this repo, core, pcsx-abnxt, pcsx-ab and autobleem-appliance (merged into develop 2026-09-24; the
work happened in `E:\Programming\_work-quiet`). Nothing of it has run on a console or a Pi yet -
`tools/stick_writes.sh start|stop` measures a scenario there.

- **Write only what changed.** `DirEntry::writeFileIfChanged(path, contents)` (compare, then `.tmp` +
  replace) is under `IniFile::save`, `ConfigFileEditor` (`replaceProperties`: one write per batch, an empty
  line removes a key), the playlists, fingerprints, `.m3u`, EmulationStation's gamelist, the cue repair.
  regional.db: `UPDATE GAME` matches no row when nothing differs, `replaceDiscs` and the sub-dir tables
  compare first, the history writes only the ranks that move. Config saves once.
- **The runtime dir** (`Env::getPathToRuntimeDir()`, platform ini `runtime_dir`, `$AB_RUNTIME_DIR` from the
  scripts): `/tmp/autobleem` on the console, `/run/autobleem` (systemd `RuntimeDirectory`) on the Linux
  targets, `System/Runtime` elsewhere. It holds the logs (`logs/`), the selection hand-over, RetroArch's
  append file and core-options copy, the emulator's exit dir, the extensions' crash guard.
- **Logs**: `Env::getPathToLogsDir()` = `<runtime>/logs`, or `System/Logs` when kept -
  `Env::keepLogsRequested()`: the `System/Logs/keep` marker, config.ini `keeplogs` (Options -> Diagnostics,
  which makes/removes the marker), `$AB_KEEP_LOGS=1`. `rc/ab_log.sh` (identical in `payload/` and
  `payload_linux/`, checked by `test_app_resolve`) is the scripts' side; `ab_persist_logs REASON` copies a
  crash's logs to `System/Logs/crash-<n>/` (last 3, a `.new` marker the launcher announces once -
  `Env::takeNewCrashLogs()`). Hardware Information: a Logs section, Square = `Env::copyLogsToStick()`.
- **The selection** (`autobleem_cfg.sh`) is in the runtime dir and written only when the launcher leaves;
  `selection.sh` takes anything but 4/6/7 for a crash (persists the logs before its reboot).
- **RetroArch** is no longer set up by rewriting its files: `LaunchService::prepareRaAppend` writes
  `<runtime>/ra-append.cfg` (`config_save_on_exit` from config.ini `rapersist` - Options "Persist RetroArch
  config" - plus the game's settings; core options in a RAM copy named by `core_options_path`), the scripts
  pass `--appendconfig`. RetroArch 1.22 saves appended values into `retroarch.cfg` when it saves, so
  `restoreAppended()` puts each of ours back afterwards; what the player changed stays.
- **The emulator** says what it takes in an `abfeatures` file next to its binary (pcsx-abnxt: `exitdir`,
  `memcarddir`, `loadstate`; the classic pcsx-ab the same, `frontend/ab_env.h`): `AB_EXIT_DIR` (the resume point of the way
  out in `<runtime>/exit`, `ResumePointService::setExitDir` reads it there and copies only a kept slot),
  `AB_MEMCARD_DIR` (the set played in place, `MemcardService::setDirForLaunch` - no swap),
  `AB_LOAD_STATE` (the kept slot read where it is). Without the file everything works as before.
- **Refused games** are regional.db's `FAILED_GAMES` (`GameDatabase::replaceFailedGames`), listed in the
  Game Manager after the games ("Not added", the reason in the pane, Square deletes the folder).
- **Guard**: `autobleem-core/tests/core/test_quiet_stick.cpp` over `tests/support/tree_snapshot.*` (size +
  mtime of every file) - a second start, an unchanged save, a no-change rescan (a refused game, a two-disc
  game, a RetroArch tree included) must leave the tree alone. Keep it green: **anything that runs per boot,
  per scan or per launch writes through `writeFileIfChanged` or to the runtime dir.**

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
  `autobleem.log` in the logs dir (`Env::getPathToLogsDir()`: `<runtime>/logs` in RAM, 256 KB x 2 - or
  `System/Logs`, 1 MB x 3, with "Keep logs on the stick"), as `HH:MM:SS LEVEL [function:line] message`; the file's
  first line is the build (`Version::FULL_VERSION`). **There is no `cout` anywhere** - the screens, the ui
  library and the demo log the same way (diagnostic chatter is `PLOG_DEBUG`, off in release builds);
  `tools/theme_convert`'s `cout` is that CLI's output, not a log. Console `stdout`/`stderr` still go to
  `AB_*.txt` in the logs dir and are unit-buffered so the last lines survive a crash. A `PLOG_*` inside an
  unbraced `if` wants braces (the macro is itself an if/else; `-Wdangling-else` says so).
- Files are read/written by bare `ifstream`/`ofstream`; use `ios::binary` for anything that is not text
  (PNG blobs, .mcd cards, PBP headers) or the Windows build corrupts it. **Never `readsome()`** to read a
  file: it returns only what is already buffered, and libc++ (llvm-mingw, what the Windows programs are
  built with) never reports anything on a fresh stream - `DirEntry::copy` wrote every file empty and called
  it a success until 2026-09-23 (a 0-byte `UpdateRoms.exe` on every stick the installer touched); GCC builds
  never showed it. `read()` + `gcount()`, as `DirEntry::copy` does now.
- **Every version a user sees is the package's** (`Env::productVersion()` - `$AB_VERSION`, else a `VERSION`
  file at the data root / next to the program / a folder up, else `Version::DESCRIBE`); never show
  `Version::VERSION` or `FULL_VERSION` on screen. The launcher exports `AB_VERSION` for what it starts.
  autobleem-main's `docs/versioning.md` §4 has the rule.
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

## Licence

GPL-3.0-or-later since 2026-09-21 (`LICENSE`; autobleem-main's `docs/licensing-plan.md` has the analysis - GPLv2 is not
possible, the 0.9.1 and AutoBleem-NG code is GPLv3). `THIRD_PARTY_NOTICES.md` is **generated** by
`tools/make_third_party_notices.py` from the vendored licence files - rerun it when anything under
`lib_ableem/third_party`, `tests/third_party`, the fonts or `libs.tar.gz` changes; the package scripts copy
it and `LICENSE` next to the launcher. `TRADEMARKS.md` keeps the name, logo and theme artwork out of the
grant. Nothing proprietary may go under `payload/` or `src/resources/`: the themes' fonts are OFL, their UI
sounds, launcher images and the default music are generated (`tools/make_theme_{sounds,images,music}.py`);
`ab2/ab.ogg`, `aergb/mel.ogg`, every `play_text.png` and the themes' backgrounds are the owner's / the
theme authors' own work.

## Git

Repo was `git init`ed on 2026-09-15 from the final source snapshot (no upstream history here; the public
history is at github.com/screemerpl/cbleemsync). Commit per logical refactor step. `.gitattributes` forces LF.

## User manuals (`manuals/`, 2026-09-21)

`manuals/<lang>/autobleem-user-manual.md` (English and Polish for now) is the user manual in a small Markdown
subset (headings, lists, tables, `![caption](path)` figures, `> ` notes, `<!-- pagebreak -->`), built by
**`tools/build_manuals.py`** into `build_manuals/<lang>/*-<lang>.html` and `*.pdf` (headless Chrome/Edge prints the
PDF; `--html` skips it; `manuals/style.css` is the look, the site's palette). **`tools/repo_publish.sh manuals
build_manuals/*/*.pdf`** puts the PDFs on the site (`manuals/`, a "User manual" panel under "Every platform" -
`index_manuals`, `MANUAL_LANGUAGES`); rebuild and republish after any manual change. Facts to keep right: a stock
console reads **FAT32 only** (exFAT needs the AutoBleem kernel), and the About screen's easter egg is not mentioned. The screenshots are
`manuals/images/<lang>/*.jpg`, taken by **`tools/manual_shots.py`** through the DebugDriver on the Windows
dev build (`--lang Polski --show`; needs `make_win.sh` and `tools/make_usb.py usb`): it walks the launcher,
PSC-Bios and ABFlashKit through their screens with the stick's `config.ini` switched to each language, and
captures the UpdateRoms/installer windows by title when they are open. A changed screen means rerunning the
shots for every language; a new language is a folder under `manuals/` plus its `LANGS` entry. `build_manuals/`
is git-ignored; the PDFs are not checked in.
