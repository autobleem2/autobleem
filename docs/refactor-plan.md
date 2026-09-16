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

   **`ab_ui` and `ab_evoui` were not created.** `Gui::menuSelection()` constructs `GuiLauncher` while ~20
   launcher files use `Gui`: as targets the two would be a link cycle, which enforces nothing and is worse
   than one target. They wait on step 14, which is what breaks the cycle. Each parked file above joins
   `ab_core` in the phase B step that gives it a seam.
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

6. `GameQueryService`
7. `GameCatalogService`
8. `ResumePointService` and `MemcardService`
9. `GameSettingsService`
10. `LaunchService` (introduces `ProcessRunner`)
11. `RetroArchService`

**Phase C — split `Gui`**

12. `TextRenderer` out of `gui.cpp`.
13. `ThemeAssets` out of `gui.cpp`.
14. `menuSelection()` becomes `ClassicMenuScreen`. After this `Gui` is window + renderer + assets, and
    `App::run()` shows screens instead of calling a method on the Gui.

**Phase D — split `GuiLauncher`** (the existing todo #3). Deliberately last: phases B and C take roughly 600
lines out of it first, so what is left is genuinely carousel and input.

15. `carousel.*` (positions, the duplicated-games rule, cover loading).
16. `launcher_input.*` / `launcher_screen.*` along the existing file seam.

## 6. Invariants to enforce

- `grep -rl "ableem/ui\|SDL2/" src/code/core` returns nothing. Same check style as the existing SDL grep.
- Nothing under `core/` includes anything from `ui/` or `evoui/`.
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
