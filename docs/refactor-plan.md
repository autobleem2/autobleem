# AutoBleem — model / services / UI refactor

Plan of record for restructuring `src/code` from "folders named after screens" into layers. Written
2026-09-16, after the `App`/`Session` split and the move of config/theme/audio off the `Gui` singleton.

Read `CLAUDE.md` first — it describes the code as it is now. This file describes where it is going and in
what order. Each numbered step below is meant to be one commit that builds and can be smoke tested.

## 1. Why

The two layers below the app are already right and are not touched by this plan:

- **`ableem_engine`** — data access, no SDL: filesystem, strings, ini/cfg, the SQLite databases, the scanner,
  disc images, RetroArch playlist files.
- **`ableem`** — the SDL toolkit: platform, renderer, texture, font, audio, input, `GuiScreen`.

What is missing is the layer between them. `src/code`'s folders (`engine/ gui/ launcher/`) say *which screen*
a file belongs to, not *what layer* it is, so business logic ended up inside the screens that happened to need
it first:

| file | lines | what is actually in it |
|---|---|---|
| `launcher/gui_launcher.cpp` + `gui_launcher_loop.cpp` | 2,125 | screen, input loop, **every game-set query**, history writes, launch decisions |
| `gui/gui.cpp` | 904 | renderer shell, text/rect helpers, and `menuSelection()` — a whole screen inside a method |
| `gui/menus/gui_gameEditorMenu.cpp` | 601 | a menu, plus Game.ini / pcsx.cfg read-modify-write |
| `launcher/ra_integrator.cpp` | 596 | a service already — only misfiled |
| `engine/cardedit.cpp` | 707 | the .mcd model and its icon rendering in one class |

Nine files under `gui/` and `launcher/` call `DirEntry::`, `app.library()`, `ofstream`, `Util::runAndWait` or
`MemcardManager` directly. None of that is testable, because reaching it means constructing a screen, which
means an SDL window.

## 2. Target

Three packages, as real CMake static-lib targets. Targets, not just folders: the linker then enforces the
dependency direction, which is the same thing that made the `lib_ableem` split stick.

```
ab_core       -> ableem_engine                  no SDL, no screens, unit tested
ab_ui         -> ab_core + ableem               shell, dialogs, menu framework, classic UI
ab_evoui      -> ab_ui + ab_core                EvolutionUI: carousel and launcher screens
autobleem-gui -> all three + app.cpp/main.cpp   composition root
starter       -> ab_core                        free proof that core stays SDL-free
ab_core_tests -> ab_core + doctest              host builds only
```

Launching **is** logic: the emulator interceptors are SDL-free and belong in `ab_core`, under
`core/services/launch/`. They are not a separate package.

```
src/code/
  main.cpp  app.cpp/h            composition root: builds the services, owns them, runs the loop
  core/
    model/       game.h  session.h  game_set.h  enums.h  core_info.h
    services/    game_query.*  game_catalog.*  game_settings.*  memcard.*
                 resume_point.*  scan.*  config.*  theme.*  retroarch.*  apps.*
      launch/    launch_service.*  process_runner.h  emu_interceptor.h
                 pcsx_interceptor.*  retroarch_interceptor.*  app_interceptor.*
  ui/
    gui.*  screen.*  text_renderer.*  theme_assets.*  fonts.*  audio.*
    screens/     splash  about  confirm  keyboard  pad_test  scroll_win
                 select_memcard  classic_menu
    menus/       menu_base.h  options*  memcards  game_manager  game_editor
  evoui/
    launcher_screen.*  launcher_input.*  carousel.*  notification_line.*
    elements/    ps_obj  ps_meta  ps_menu  ps_centerlabel  ps_stateselector  ...
    screens/     mc_manager  app_start  btn_guide
tests/
  CMakeLists.txt  third_party/doctest/doctest.h
  support/        env_fixture.h  temp_dir.h  fake_process_runner.h
  fixtures/       regional.db  internal.db  Games/  themes/  playlists/
  core/           test_game_query.cpp  test_game_settings.cpp  ...
docs/refactor-plan.md   (this file)
```

`audio.*` stays in `ui/`, not `core/`: `AppAudio` holds `ableem::Sound`/`Music`, so it needs SDL. `App` still
owns it and screens still reach it as `app.audio()` — only the file location follows the no-SDL rule.

## 3. What moves out of the UI

| lifted from | into |
|---|---|
| `GuiLauncher::getGames_SET_SUBDIR/FAVORITE/HISTORY/RETROARCH/APPS`, `getAllPS1Games` | `GameQueryService` |
| `GuiLauncher::addGameToPS1GameHistoryAsLatestGamePlayed`, favorite toggles, `GuiManager`'s game delete and the `nftw` cover flush | `GameCatalogService` |
| `GuiEditor`'s Game.ini and pcsx.cfg read/write (about 400 of its 601 lines) | `GameSettingsService` |
| `PsGame::findResumePicture/storeResumePicture/isResumeSlotActive/removeResumePoint/isCleanExit`, interceptor `prepareResumePoint/saveResumePoint` | `ResumePointService` |
| `PsGame::setMemCard`, the file work in `GuiMemcards`/`GuiSelectMemcard`, interceptor `memcardIn/memcardOut` | `MemcardService` |
| `App::launchGame`, `App::writeSelectionScript`, the four interceptors | `LaunchService` |
| `RAIntegrator` | `RetroArchService` (rename, and drop what `ableem::RetroArchPlaylist` already does) |
| `Gui::menuSelection()` | `ClassicMenuScreen : GuiScreen` |
| `Gui`'s text/token/rect helpers (about 450 lines) | `TextRenderer`; theme textures and fonts to `ThemeAssets` |

`PsGame` then becomes what its name says: a data record (`ableem::GameRecord` plus the launcher-only fields),
with no filesystem in it.

## 4. Unit testing

The point of the no-SDL rule in `ab_core` is that everything above can be tested without a window. There are
no tests today, so the harness lands **before** the first service is extracted, and every service arrives with
its tests in the same commit. A service with no test is not done.

**Framework: doctest**, vendored as a single header at `tests/third_party/doctest/doctest.h`. Chosen over
Catch2 v2 for compile time (it is the header every test TU includes) and over GoogleTest because that needs a
build of its own. It needs only C++11, so the C++14 / GCC 8 constraint is fine. This is the one piece of
vendored code outside `lib_ableem/third_party` — it ships in no binary, so it does not belong in the library.

Wiring: `enable_testing()` in the root `CMakeLists.txt`, `add_subdirectory(tests)` guarded by
`AB_BUILD_TESTS` (ON for host builds, forced OFF for the ARM toolchain file), one `add_test` per doctest
suite, run with `ctest --test-dir build_win --output-on-failure`. `make_win.sh` runs `ctest` after a
successful build.

Two support pieces are needed before anything can be tested, and both are small:

- **`EnvFixture`** — `ableem::Environment`'s setters are static, i.e. global. A scoped fixture that points
  `Env` at a temp tree in its constructor and restores the previous values in its destructor is what makes
  the tests independent of each other and of run order.
- **`ProcessRunner`** — `LaunchService` must not fork in a test. Give it a `ProcessRunner&` (the production
  one calls `Util::runAndWait`); tests pass a recording fake and assert on the argv that *would* have been
  used. This is the only interface this plan introduces purely for testability, and it is worth it: argv
  construction for `launch.sh`/`launch_rb.sh` is exactly the kind of thing that silently breaks.

What each service's first tests cover:

| service | tested with |
|---|---|
| `Config`, `Theme` | ini defaults, the merge of a partial theme over `themes/default`, the fallback to the Sony dir when a theme sub-dir is missing |
| `GameQueryService` | fixture `regional.db` + `internal.db`: all games, favorites, history, sub-dir rows, `origames` on and off |
| `GameCatalogService` | history ordering after N plays, favorite toggle round-trip, delete removes the game dir (temp tree) |
| `GameSettingsService` | Game.ini round-trip, pcsx.cfg line replacement, a file with CRLF and a file with missing keys |
| `ResumePointService` | slot naming, picture paths, `isResumeSlotActive` against a temp `!SaveStates` tree |
| `MemcardService` | create/rename/swap-in/swap-out/restore over a temp `!MemCards` tree |
| `LaunchService` | argv for PCSX, RetroArch and Apps, with and without a resume point, via the fake runner |
| `RetroArchService` | parsing both playlist formats from fixture `.lpl` files, core override resolution |

Fixtures: a trimmed `regional.db`/`internal.db` (a handful of rows) plus a small `Games/`, `themes/` and
`playlists/` tree under `tests/fixtures/`. The `.db` files are binary per `.gitattributes`. Keep them small
enough to read in a diff-less review — a dozen rows is plenty.

## 5. Order of work

Each item is one commit. Moves are kept content-free and separate from edits, so `git log --follow` survives.

**Phase A — model, packaging and the test harness**

1. ~~`enum class` for `SET_*`, `STATE_*`, `EMU_*`, `SEL_OPTION_*`~~ **done 2026-09-16.** `EMU_*` turned out
   to be `EmuMode` already. The rest became `GameSet` + `Ps1SelectState` (`session.h`),
   `LauncherScreenState` (`gui_launcher.h`) and a file-local `LauncherMenuOption`
   (`gui_launcher_loop.cpp`) — not a shared `core/model/enums.h`, because `core/model/` did not exist yet
   and three of the four are used in exactly one screen. Fold them in when that file appears.
   `PsMenu::selOption` stayed an `int`: it is a generic index that `PsMenu` animates by `++`/`--`.
   The compiler earned its keep twice — a dead `launcher.set < 0` guard in `Gui::menuSelection()`, and five
   identical `showSetName()` branches that collapse to one.
2. ~~Create the three targets and `git mv` the files that are already clean~~ **partly done 2026-09-16.**
   The file list above was written without checking, and most of it was not clean:

   | file | what stopped it |
   |---|---|
   | `scanner.*` | 6× `Gui::splash` |
   | `pcsx_`/`retboot_`/`launch_interceptor.*` | `Gui::splash` + `Gui::getInstance()` + `App::get()` |
   | `theme.*`, `util_time.*` | `App::get().config()` |
   | `ps_game.*` | `App::get().library()` in `setMemCard` |
   | `session.h`, `ra_integrator.*`, `emu_interceptor.*` | clean, but include `ps_game.h` |

   So `ab_core` was created with what genuinely had no `Gui` and no `App::get()`: `main.h`,
   `environment.*`, `util.*`, `lang.*`, `DebugTimer.*`, `services/config.*`, plus `model/timing.h` for the
   `TicksPerSecond` / showing-timeout defines that had been sitting in `gui_NotificationLine.h` where
   `Config` could not reach them. `starter` links `ab_core` alone, which is the standing proof it stays
   SDL-free. The stale `ver_migration.*` row is gone from `CLAUDE.md`.

   **`ab_ui` and `ab_evoui` were not created** then: `Gui::menuSelection()` constructed `GuiLauncher`
   while ~20 launcher files use `Gui`, and as targets the two would have been a link cycle. **Created on
   2026-09-16 after step 14** (see there). Each parked file above joined `ab_core` in the phase B step
   that gave it a seam, except `theme.*`, `util_time.*` and `scanner.*`, which are in `ab_ui`.
3. ~~**Attempt an ARM build here**~~ **deferred** — the toolchain is still not on this host. Phases A–D are
   host-verified only, as section 7 anticipated. Do this before any of it ships.
4. ~~The test harness: doctest, `tests/`, `EnvFixture`, `TempDir`, ctest wiring, and the first tests against
   `Config`~~ **done 2026-09-16.** doctest 2.4.11, `tests/{support,core,third_party}`, `AB_BUILD_TESTS`
   (forced OFF by `PSCtoolchainV8.cmake`), `make_win.sh` runs `ctest` after every build. 10 test cases /
   44 assertions over `Config` and over the fixtures themselves — `EnvFixture` is load-bearing enough that
   trusting it untested would be the wrong kind of economy. Mutation-checked: flipping the `ui` default
   fails `test_config`, so the suite is not passing vacuously. `Theme` joins when it reaches `ab_core`.
5. ~~`GameSetSelection` in `core/model/`, replacing the six loose ints shared between `GuiLauncher` and
   `Session::LauncherState`~~ **done 2026-09-16.** `core/model/game_set.h` now holds `GameSet`,
   `Ps1SelectState` and `GameSetSelection`; `Session::LauncherState` is gone and `GuiLauncher`'s six
   `current*` members are one `selection`. The identical five-line save block that appeared at three call
   sites is `GuiLauncher::rememberSelection()`.

   One quirk was preserved rather than quietly fixed: the PS1 sub-set is not carried across while another
   set is showing (`loadAssets()` does not restore it, `rememberSelection()` does not write it back), so
   launching from RetroArch or Apps and then pressing Select back to PS1 lands on All Games rather than
   where you left it. Both halves have always behaved this way. Copying the struct whole would change it -
   arguably for the better, but that is a behaviour decision, not a structural one.

**Phase B — extract the services.** One per commit, each with its tests, in this order — each extraction
shrinks the input to the next:

6. ~~`GameQueryService`~~ **done 2026-09-16**, in two commits: `PsGame` had to reach `ab_core` first
   (`PsGame::setMemCard` split so the Game.ini half is core and the regional.db half stays at the two
   interceptor call sites until step 8). `GameQueryService::gamesFor(selection)` now answers the whole
   question `switchSet` used to work out inline - the origames fallback, the five PS1 sub-sets, RetroArch,
   Apps, and the sorting - and `switchSet` is carousel work only.

   RetroArch is reached through a two-method `RetroArchGames` interface that `RAIntegrator` implements, so
   core does not know the launcher singleton exists; step 11 replaces the implementation, not the seam.

   Three things the tests turned up that no amount of reading would have: a game with no `DISC` row is
   invisible to `loadUsbGames` (it is a join), a **USB** game's favorite flag lives in its `Game.ini` and
   not in `regional.db` (only internal games use the `FAVORITE` column), and a brand-new `internal.db`
   needs `createSchema()` before `openInternalGames()`'s column ALTERs mean anything. All three are now
   encoded in `tests/support/game_library_fixture.h`. `ps1GamesInSubDirRow` also had an out-of-range read
   before its bounds check; it returns empty now.
7. ~~`GameCatalogService`~~ **done 2026-09-16**, with one deliberate omission. History renumbering, the
   game delete and the cover flush all moved. **The favorite toggles did not**: in `GuiEditor` the favorite
   is one of six identical `gameIni` read-modify-save blocks sharing one long-lived `IniFile` member, so
   pulling one out would leave the editor writing the same file two ways. They move with the rest of the
   editor at step 9, which is where that ini handling belongs anyway.

   The cover flush no longer uses `nftw()`. Walking with `DirEntry` instead keeps `<ftw.h>` - which is not
   portable and is awkward on the Windows dev build - out of `ab_core`, and makes the flush assertable
   against a temp tree.

   `recordGamePlayed` now wraps its writes in one transaction per database. It rewrites the whole ranking
   on every launch, and each loose `UPDATE` was its own disk sync on the console; the suite that exercises
   it went from 37s to 9s, which is the same effect measured from the other side.
8. ~~`ResumePointService` and `MemcardService`~~ **done 2026-09-16**, in two commits. `MemcardService`
   owns which set a game plays with and the swap around a launch, which the PCSX and RetroArch interceptors
   each had their own copy of; `memcardIn`/`memcardOut` are one line each now. `PsGame::setMemCard` is one
   call again as `setCardForGame()`, undoing the split step 6 needed. The memory-card screens and the
   editor go through it too, so nothing outside the service constructs a `MemcardManager`.

   **Known bug found and pinned, then fixed on 2026-09-16 in its own commit** (the guard is gone and the
   test asserts the fallback): `swapInForLaunch`'s "the set is gone, fall back to SONY" branch was unreachable. `MemcardManager::swapIn()` returns false only when the set directory is missing,
   and the `DirEntry::exists` guard immediately above the call has already excluded that. A game pointing
   at a deleted set therefore keeps pointing at it. Deleting the guard is the fix; it is a behaviour change,
   so today's behaviour is asserted in `tests/core/test_memcard.cpp` and commented at both ends, and fixing
   it should be a deliberate commit that changes that test.

   `ResumePointService` took `PsGame`'s five resume methods and the PCSX interceptor's
   `prepare`/`saveResumePoint`; the interceptor's two are one line each now. The "read the second line of
   the filename file to get the state's base name" dance appeared seven times across those seven methods
   and is one helper. Two naming quirks are preserved and commented because callers depend on them: slot 0's
   picture has no number in its name, and `lastPicture()` looks for slot 0's picture name whichever slot it
   finds.

   With that, **`PsGame` is what its name says** - `ableem::GameRecord` plus the launcher-only fields, 29
   lines, no filesystem at all. The last of it, the Game.ini memcard write, went into `MemcardService`
   alongside the database write it belongs with, which is what section 3 of this plan asked for.
9. ~~`GameSettingsService`~~ **done 2026-09-16.** `GuiEditor` is 320 lines instead of 600, and every one
   of them is rendering or input. The service is `open()` (the Game.ini, or for an internal game an ini
   filled in from the record; then the nine pcsx.cfg reads) plus one setter per editor option, each
   preserving the file encoding it had - 0/1 flags in decimal, the levels in hex, `frameskip3` written in
   hex but read in decimal (0..3, so it never mattered). The **favorite toggles deferred from step 7** are
   here, not in `GameCatalogService`: in the editor they were one of six identical read-modify-save blocks
   over one `IniFile`, and that ini is what this service is.

   Both callers used to seed the editor's ini by hand, the same five lines twice; they set `gameData` now
   and `open()` does the rest. `GuiEditor::gameFolder` and `internal` were always `game->folder` and
   `game->internal` at both call sites, so they are gone.

   Two things pinned rather than fixed: `setLocked` only flips Automation from its opposite value (an ini
   with no key is left alone - the scanner always writes one), and `setMemcard` writes the ini but not
   regional.db's MEMCARD column, which `MemcardService::setCardForGame` does; a launch reads the ini so the
   stale column has no effect. Both are asserted in `tests/core/test_game_settings.cpp` under a comment.

   Smoke testing this found a dev-host bug in lib_ableem, fixed in its own commit: with the keyboard as
   the pad, `Input::padEventPending()` never saw a key release, so every classic screen's d-pad repeat loop
   spun forever on the first press. The console was never affected.
10. ~~`LaunchService` (introduces `ProcessRunner`)~~ **done 2026-09-16.** `App::launchGame`'s interceptor
    choice and the three interceptors are one service; `launch(game, mode, resumePoint)` is the whole
    sequence (cards in, resume point prepared, argv built, script run, cards out) and `App::launchGame` is
    the Gui work around it. `ProcessRunner` is the one seam this plan introduced for testability, and it
    also absorbed the `#ifdef AB_DEBUG_HOST` each interceptor had: the composition root installs a splash
    runner on the dev host and the fork runner on the console. The launcher-script and RetroArch paths
    come from `Env` (`getPathToRCDir()`, `getPathToRetroarchDir()`) - identical on the console, and what
    lets the tests point them at a temp tree - which is most of the old todo #2. `session.h` moved to
    `core/model/` with it, and `GuiLauncher`'s one direct `PcsxInterceptor` use (saving a resume point) is
    the `ResumePointService` call it always was.

    **Known bug found and pinned, then fixed on 2026-09-16 in its own commit** (`ConfigFileEditor` matches
    the whole key now): `transferRaConfig`'s scanline overlay. `ConfigFileEditor`
    matches a property as a *line prefix*, so replacing `input_overlay` also rewrites the
    `input_overlay_enable` and `input_overlay_opacity` lines, and the two replacements meant for them then
    find nothing; RetroArch is left with three `input_overlay = ...` lines and its defaults for the other
    two. The fix is to replace the longer keys first, or teach `ConfigFileEditor` to match `key =`. It is
    a behaviour change, so today's output is asserted in `tests/core/test_launch.cpp` with a comment.
    Also pinned, and fixed the same day: a RetroArch launch never recorded a "last played" time - only
    PCSX and Apps did. It does now, for the library's own games only: a playlist entry's `gameId` is its
    index in the playlist and would have named an unrelated regional.db row.

    Two non-behaviour changes folded in: the selection script was written twice per launch (once by
    `App::launchGame`, once by each interceptor's `execute`) and is written once now, and the 3 ms
    `usleep` after the run happens on the dev host too, where the interceptors used to skip it.

    The tests (15 cases) build the argv for PCSX with and without a resume point, for RetroArch with each
    core choice and a foreign playlist entry, and for an App; and, through the fake runner's `whileRunning`
    hook, look at the tree *during* the run - which memory card is in play, what RetroArch's `.srm` and its
    config say - and after it. 23 mutations, every one caught.
11. ~~`RetroArchService`~~ **done 2026-09-16.** `RAIntegrator` moved into core as an `App`-owned service
    and lost its singleton; the launcher, the playlists menu and the carousel reach it as
    `app.retroArch()`. Its public surface shrank to what was actually called from outside - the playlist
    names, a playlist's games and count, the post-run favorites/history reload, `escapeName()` - and the
    core tables and detection are private. The "drop what `ableem::RetroArchPlaylist` already does" part:
    the two per-format parsers were one function each over the engine's loaders but had drifted (only the
    JSON one mapped `/media` onto the USB root for the dev host; the six-line one `continue`d where the
    JSON one let `isGameValid` drop the entry a few lines later) and are one function now over
    `RetroArchPlaylist::load()`. The `/media` mapping is unconditional: on the console the USB root *is*
    `/media`, so it is the identity there, and it is what lets the tests (and the fake USB tree) hold
    playlists RetroArch itself wrote. Favorites and history, which had a duplicated reload each, share
    `reloadSpecialPlaylist()`.

    The tests (12 cases) build a RetroArch install in a temp tree - `.info` files, core stubs, ROMs - and
    cover both playlist formats, the exclusions (AutoBleem's own export, the Apps list, empty files,
    non-.lpl), core resolution in each of its three ways, the dropped-entry rules including archives,
    Favorites/History placement and fill-in, and the post-run reload. One thing the mutation check taught:
    NTFS returns directory entries already sorted case-insensitively, so a sort can only be tested with a
    lower-case name that byte order puts last.

    With that **phase B is complete**: every service the plan listed exists, is owned by `App`, is in
    `ab_core`, and has tests. `session.h` went with step 10. Left in the app target and still core-shaped:
    `theme.*`, `util_time.*` (both `App::get().config()`) and `scanner.*` (`Gui::splash`).

**Phase C — split `Gui`**

12. ~~`TextRenderer` out of `gui.cpp`~~ **done 2026-09-16.** 400 lines out of `gui.cpp` (899 -> 504),
    the header down from 178 to 95. `TextRenderer` holds references to the renderer, the theme, and `Gui`'s
    theme font and button-texture map (both of which change when a theme loads), so nothing was copied.
    The token structs stopped reaching for `Gui::getInstance()` and take the renderer they belong to.
    Every screen's `gui->renderTextLine(...)` is `gui->text().renderTextLine(...)` - ~100 mechanical
    edits in 20 files, no signature changed. `renderBackground/Logo/Status/TextBar/FreeSpace/drawText` stay
    on `Gui` because they draw its textures; they go with step 13. No ctest coverage is possible here;
    the editor and the button guide screenshots are pixel-identical to the ones before.
13. ~~`ThemeAssets` out of `gui.cpp`~~ **done 2026-09-16.** The fonts, the seven named textures and the
    button-marker map, `loadThemeTexture()` and the body of `loadAssets()` are `gui/theme_assets.*`;
    `Gui::loadAssets()` is `assets_.load()` plus the theme's music, which is `AppAudio`'s and not a
    texture. `ThemeAssets` takes the renderer, the `Theme` and the `Config` by reference, so the
    `App::get()` calls that were in `loadAssets()` are gone from it. `TextRenderer`'s references now point
    into `assets_`, which is declared first. Screens' `gui->themeFont` / `gui->backgroundImg` / ... are
    `gui->assets().themeFont` etc. - 33 edits in 8 files. `gui.cpp` is 441 lines (from 899 at the start
    of phase C), the header 85. Verified by cycling the theme in the Options menu: background, logo, font
    and button textures all switch, and cancelling switches them back.
14. ~~`menuSelection()` becomes `ClassicMenuScreen`.~~ **done 2026-09-16.** `gui/gui_classic_menu.*`;
    `App::run()` shows it where it called `gui_->menuSelection()`. `Gui` is window + renderer + assets +
    text + the four drawing helpers, 182 lines of .cpp. The recursion is gone: where `menuSelection()`
    called itself after a sub-screen and let the outer frame fall through, the screen calls `restart()`
    (init + render) and breaks out of the event drain, so the loop's next iteration picks up
    `startingGame`/`resumingGui` the way the recursive call's first lines did. The one observable
    difference is that the stack no longer grows a frame per sub-screen visit. The power-off block
    (`shutdown -h now; sync(); exit(1)`) was kept as it was rather than routed through `Util::powerOff()`,
    which exited 0 and did not sync; unified the same day, at the user's call: `Util::powerOff()` syncs
    and exits 0, and all three power-off paths (both L2+R2 menus and the power button) use it.
    Verified with the About, Options, Advanced-row, Start -> launcher -> launch -> back sequence.

    **And then the targets.** With the classic menu out of `Gui`, the only file in `gui/` that named the
    launcher was the menu itself. The remaining coupling was `App`: every screen holds it as `app` and it
    also held `run()`, the runner choice and the outer loop, so `ab_ui` would have depended on the
    executable. `App` is the model now, at the top of `ab_ui`, taking its `ProcessRunner` from whoever
    constructs it; `AutoBleem : App` (`src/code/autobleem.*`) in the executable adds `run()`,
    `openLibrary/rescan/launchGame` and `makeProcessRunner()`. `ab_ui` = `gui/` + `engine/` + `app.*` +
    `util_time.*`, `ab_evoui` = `launcher/`, and the executable is `main.cpp`, `autobleem.*` and
    `gui_classic_menu.*` - the one screen that shows both the classic sub-screens and the launcher lives
    above both libraries. Each target links only the one below it, so the linker now enforces what
    section 6 asked for. One stale include turned up: `app_audio.cpp` took `TicksPerSecond` from a
    launcher header; it comes from `core/model/timing.h`.

**Phase D — split `GuiLauncher`** (the existing todo #3). Deliberately last: phases B and C take roughly 600
lines out of it first, so what is left is genuinely carousel and input.

15. ~~`carousel.*` (positions, the duplicated-games rule, cover loading).~~ **done 2026-09-16.**
    `launcher/carousel.*`: `games` and `selected`, the `PsCarousel` position table, `scrolling`, the
    duplicated-games rule (`setGames`), `setInitialPositions`, both scrolls, `moveMainCover`,
    `updatePositions`/`updateVisibility` and the cover-drawing part of `render()`. `GuiLauncher` holds it
    as `carousel` and its ~200 references became `carousel.games` / `carousel.selected` /
    `carousel.selectedIsValid()` by search-and-replace; `gui_launcher.cpp` is 520 lines from 834.
    `Carousel` takes a `GuiBase&` for the renderer and the clock. Two small things folded in:
    `setInitialPositions`'s twelve unrolled blocks are two six-step loops with the same result, and
    `PsCarouselGame` takes its `PsGamePtr` by const reference. The `GuiLauncher` `gui` shadow section 7
    warned about is gone - the new member is initialised from the base's `gui`, which the shadow would have
    hidden behind a null pointer. Verified: scroll both ways with key repeat, the cover moving up for the
    game menu and back, a set switch onto a two-game playlist filling all thirteen positions.
16. ~~`launcher_input.*` / `launcher_screen.*` along the existing file seam.~~ **done 2026-09-16.**
    `gui_launcher.cpp` -> `launcher_screen.cpp` (521 lines: assets, sets, metadata panel, state
    transitions, render) and `gui_launcher_loop.cpp` -> `launcher_input.cpp` (595: the event loop and the
    per-button handlers), both `git mv` so the history follows. A third seam was plain once the carousel
    was out: the eight handlers that *do* something - start the game, open the settings / game editor /
    memory-card manager / resume selector, the L2+Select sub-directory and playlist choosers - and then
    reconcile the carousel afterwards, are `launcher_actions.cpp` (484). The class is unchanged;
    `LauncherMenuOption` moved to the header because two files use it now. No behaviour change, and
    nothing to test but the app: scroll, game menu, editor, button guide, set switch and a RetroArch launch
    all as before.

**With that the plan is complete**: phases A to D, every step, plus the three bugs the extractions pinned
and the power-off unification. What it did not do, by the user's choice, is step 3: nothing here has run on
a console. That is the next thing.

## 6. Invariants to enforce

- `grep -rl "ableem/ui\|SDL2/" src/code/core` returns nothing. Same check style as the existing SDL grep.
- Nothing under `core/` includes anything from `ui/` or `evoui/`. Nothing in `ab_ui` (`gui/`, `engine/`,
  `app.*`) includes anything from `launcher/`; nothing in `launcher/` includes `autobleem.h` or the classic
  menu. (Both hold as of 2026-09-16, and the link order would break if they stopped.)
- Services take their dependencies by reference in the constructor. `App::get()` stays the composition root
  only — give `GuiScreen` a `Services &svc` next to its `app`, so screens stop using it as a back door to
  globals.
- Every new service ships with tests in the same commit.
- Debug builds stay `-Wall -Wextra` clean, and `ctest` stays green.

## 7. Risks

- **ARM is unverified.** Nothing in this repo has run on a console yet. Step 3 exists for that reason; if the
  toolchain is still unavailable when phase A lands, note it in the commit and treat phases B–D as
  host-verified only.
- **~6,000 lines of churn on code whose only end-to-end check is `tools/win_drive.ps1` screenshots.** That is
  why the harness comes before the services rather than after.
- **Env is global.** Until `ableem::Environment`'s statics become an instance, `EnvFixture` is load-bearing:
  a test that forgets it will pass alone and fail in a suite.
- **`GuiLauncher` shadows `gui`** (`shared_ptr<Gui> gui` redeclared over `GuiScreen`'s, assigned the same
  value in `init()`), as do `GuiScrollWin` and `GuiKeyboard`. Harmless today, a real trap during phase C/D.
  Remove the three shadows early, in the header clean-up that is already in progress.
