# AutoBleem — developer context

AutoBleem is a game launcher / front-end for the **PlayStation Classic (PSC)**. It replaces the stock SonyUI,
scans a USB stick for PS1 games, keeps metadata + cover art in SQLite, and launches games in PCSX (bundled
`pcsx-ab`) or RetroArch/RetroBoot. Author: screemer (the repo owner). Snapshot version: v0.9.1 (`src/resources/config.ini`).

This file is the primary documentation for the codebase — the source itself is sparsely commented.

## Current work (2026-09)

Refactor for stability, then add features. Priorities, in order:

1. Behavior-neutral safety fixes: RAII instead of raw `new`/`delete`, check `fork()`/`execvp()`/file opens,
   fix functions with missing returns on error paths.
2. Centralize hard-coded paths (`/media/...`, `/tmp/...`, `/usr/sony/share/data`, `/gaadata`) and the
   `#if defined(__x86_64__) || defined(_M_X64) || defined (PI_DEBUG)` PC-vs-PSC switches into `Environment`.
3. `enum class` for the `#define`-int "enums" (`EMU_*`, `SET_*`, `STATE_*`, `SEL_OPTION_*`).
4. Split `GuiLauncher` (~2,150 lines across `gui_launcher.cpp` + `gui_launcher_loop.cpp`).
5. Features.

Also planned: a **MinGW/Windows build** for testing basic functionality on the dev machine (see Build).
The final ARM toolchain will be set up later together with the user.

## Build

Two targets in `CMakeLists.txt`: `autobleem-gui` (the app) and `starter` (small PCSX wrapper used by the
stock-UI path). C++11. `sqlite3` is built from `libs/sqlite/sqlite3ab.c`.

- **ARM (real target)**: `make_arm.sh` → `PSCtoolchainV8.cmake` (`armv8-sony-linux-gnueabihf-gcc`, `--static -Os -s`).
  Requires the toolchain at `/opt/toolchain/armv8-sony-linux-gnueabihf`. Not available on this Windows host yet.
- **Mac/Linux**: `make_mac.sh`, `make_sys.sh`.
- **Windows/MinGW**: not yet supported. Known porting surface (small):
  - `fork/execvp/waitpid` in `util.cpp`, `starter.cpp` and the three interceptors (interceptors already skip the
    fork on x86 via the `__x86_64__` ifdef and show a splash instead).
  - `DirEntry.cpp`: `opendir/readdir/mkdir/stat` (MinGW provides `dirent.h`; `mkdir` takes one arg).
  - `Util::execUnixCommand` uses `popen`; `Util::getAvailableSpace` shells out to `df` (already stubbed on x86).
  - `unistd.h`/`usleep` in ~12 files (MinGW has both).
  - `Env::getWorkingPath` uses `getcwd`/`PATH_MAX`.
  - **`libmamecd`** (`#include <libmamecd/cdrom.h>`, link `mamecd`) is used only by `engine/cdreader.h` for CHD
    images. It is NOT in the repo. For a Windows build either provide it, substitute libchdr, or compile out CHD.
- External libs: SDL2, SDL2_image, SDL2_mixer, SDL2_ttf, pthreads, mamecd. Vendored: SQLite (`libs/sqlite`),
  nlohmann json + `fifo_map` (`libs/nlohmann`), `SDL_FontCache` and `unecm.c` (in `src/code`).
- `PRE_BUILD` step copies `src/resources/` next to the binary; the app expects to run from that dir.
- No tests exist.

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
/media/themes/<name>/theme.ini    UI themes; /media/retroarch/ RetroBoot; /media/Apps/ launchable apps
/gaadata/<id>/                    stock internal games (read-only console storage)
```

Boot chain: `rc/autobleem.sh` → unpack libs → `bin/autobleem/run.sh` → `autobleem-gui /media`.
Game launch: `rc/launch.sh` (PCSX, args: ssFolder, cdfile, lang, region, gameFolder, resume, aspect, filter, pad)
or `rc/launch_rb.sh` (RetroArch: file, core). `Gui::saveSelection()` writes `rc/autobleem_cfg.sh`
(`AB_SELECTION=...`) which the shell scripts read after the GUI exits.

## Source map (`src/code/`)

| Area | Files | Notes |
|---|---|---|
| Entry | `main.cpp` | Parses argv → `Environment`; opens both DBs; adds columns to internal.db; restores memcards; decides `forceScan`; outer loop `menuSelection()` → `MENU_OPTION_START` → picks an `EmuInterceptor` → `memcardIn/prepareResumePoint/execute/memcardOut` → `gui->display(resume=true)`. |
| `main.h` | | Shared enums/constants (`ImageType`, file extension consts) and inline string helpers (`trim`, `lcase`...). Uses `std::ptr_fun`/`not1` (gone in C++17). |
| `environment.*` | `Env` | All path getters. `private_*` globals are set once in `main()`. Extend this instead of adding new literal paths. |
| `DirEntry.*` | | Filesystem helpers (dir listing, copy/rename/remove, cue parsing, name fixing). Only place using `dirent`. |
| `util.*`, `util_time.*` | | String/stream helpers, `execUnixCommand` (popen), `execFork`, `powerOff`. |
| `lang.*` | `_()` | gettext-style lookup from `resources/lang/<Language>.txt`. Emoji markers like `\|@X\|` in strings are replaced by button textures by `Gui::renderText`. |
| `engine/scanner.*` | `Scanner` singleton | Walks `/Games`, repairs cue files (comma names, missing cue), runs `unecm`, verifies discs, fills `gamesToAddToDB`, writes `regional.db` + `autobleem.list`. |
| `engine/GetGameDirHierarchy.*` | `GamesHierarchy` | Sub-directory tree of `/Games` → `SUBDIR_ROWS` tables; `autobleem.prev` file detects changes to force rescan. |
| `engine/game.*` | `USBGame`/`Disc` | A game on USB as discovered by the scanner; reads/writes `Game.ini`; `verify()`. |
| `engine/serialscanner.*`, `cdreader.h`, `isodir.*` | | Extract PS1 serial (SLUS-xxxxx) from BIN/PBP/CHD by reading the ISO9660 dir; CHD via libmamecd. |
| `engine/metadata.*`, `coverdb.*` | | Look up title/publisher/year/cover PNG blob in the 3 regional `covers*.db` (U/E/J). |
| `engine/database.*` | `Database` | Thin SQLite wrapper, prepared statements only. Schema notes in `engine/database_tables.txt`. Same class serves `regional.db`, `internal.db` and cover DBs. |
| `engine/memcard.*`, `cardedit.*` | | Memory-card swap in/out (`!MemCards`) and .mcd block editor. |
| `engine/cfgprocessor.*`, `inifile.*`, `config.*` | | pcsx.cfg / RetroArch cfg rewriting; generic ini map; `config.ini` (keys are lower-cased on load, e.g. `values["theme"]`). |
| `engine/padmapper.*` | | SDL GameController mapping, hot-plug, power button. |
| `gui/gui.*` | `Gui` singleton | Owns SDL window/renderer, fonts, theme (`theme.ini` merged over `themes/default`), music/sfx, both DB pointers, carousel restore state (`lastSet`, `lastSelIndex`...). `menuSelection()` is the classic-UI main menu event loop. `display()` (re)inits and shows splash or resumes the launcher. |
| `gui/gui_screen.*` | `GuiScreen` | Base for every screen: `init/render/loop` + virtual `doCross_Pressed()`-style handlers; `show()` runs them. Set `menuVisible=false` to exit. |
| `gui/menus/*` | `GuiMenuBase`, `GuiOptionsMenuBase`, ... | Header-only templated list menus (string, two-column, playlist, game dir) and concrete Options / Memory Cards / Game Manager / Game Editor menus. |
| `gui/gui_*` | | Splash, About, Confirm dialog, on-screen Keyboard, pad test, memcard select, scroll window, star FX. |
| `gui/gui_sdl_wrapper.h` | `SDL_Shared<T>` | RAII shared handle for SDL_Window/Renderer/Texture/Surface. Use it for any new SDL resource. |
| `gui/SDL_FontCache.*`, `gui_font.*` | | Vendored font cache + `Fonts` loader (theme fonts and Sony SST fonts). |
| `launcher/gui_launcher.*`, `gui_launcher_loop.cpp` | `GuiLauncher` | EvolutionUI: cover carousel, sets (PS1 all/internal/favorites/history/sub-dir, RetroArch playlists, Apps), settings overlay, resume-state selector, input loop. |
| `launcher/ps_*.{h,cpp}` | `PsObj` and subclasses | Animated sprite/UI elements of the launcher (carousel, meta panel, menu, buttons, labels). |
| `launcher/ps_game.*` | `PsGame` | Game as seen by the UI (from DB or playlist). `PsGamePtr = shared_ptr<PsGame>`. Handles resume-point pictures/slots. |
| `launcher/ra_integrator.*` | `RAIntegrator` singleton | Parses RetroArch `.lpl` playlists and core info, favorites/history playlists, core override (`coreOverride.cfg`). |
| `launcher/*_interceptor.*` | `EmuInterceptor` strategy | `PcsxInterceptor`, `RetroArchInterceptor`, `LaunchInterceptor` (apps): build argv, fork the `rc/*.sh` launcher, manage memcards and save-state resume points. |
| `launcher/gui_mc_manager.*`, `gui_app_start.*`, `gui_btn_guide.*`, `gui_NotificationLine.*` | | Launcher sub-screens. |
| `ver_migration.*` | | One-off migrations between AutoBleem versions (`/media/System/Logs/ver.txt`). |
| `starter.cpp` | separate binary | Wraps `/tmp/pcsx` for the stock SonyUI path; swaps memcard from `Game.ini`. |
| `gui/abl.c` | | C helper (splash from C code). `unecm.c`: ECM decoder. |

Payload (`payload/`): the release USB tree — `rc/*.sh` scripts, themes (`aergb`, `autobleem`, `default`,
`evolution`), bundled Apps, release notes. `db/` is git-ignored (cover DBs live there).

## Conventions and gotchas

- Singletons via `static shared_ptr<T> getInstance()`: `Gui`, `Scanner`, `Lang`, `RAIntegrator`.
  `Gui::db` / `Gui::internalDB` / `Gui::coverdb` are raw pointers owned by `main()`.
- `sep` is the path separator constant (from `DirEntry.h`); paths are built by string concatenation.
- Ini keys are lower-cased: `cfg.inifile.values["theme"]`, `themeData.values["background"]`.
- Bool-ish config values are the strings `"true"`/`"false"`; ints are parsed with `atoi`.
- Menu/emulator/state selections are plain `int`s with `#define`s (`EMU_PCSX`, `SET_PS1`, `STATE_GAMES`) —
  easy to mix up; converting to `enum class` is on the plan.
- The carousel duplicates games when fewer than 13 exist, so one `PsGamePtr` may appear in several
  `PsCarouselGame`s (see comment in `gui_launcher.h`).
- `Gui::display()` calls `TTF_Init()`/`Mix_Init()` every time it is re-entered after a game; `GuiBase::~GuiBase()`
  calls `SDL_Quit()` and `main()` calls it again. Audio is fully closed (`Mix_CloseAudio` loop) before forking PCSX.
- After `execvp` in the child there is no `_exit()` — if exec fails the child keeps running GUI code. Fix when
  touching the interceptors.
- Console `stdout`/`stderr` go to `/media/System/Logs/AB_*.txt`; `cout` is the logging mechanism.
- Shell scripts and cfg/ini files must stay **LF** (enforced by `.gitattributes`). Do not let the Windows
  editor convert them.
- Keep the two `trim` families in mind: in-place `trim()` from `main.h` vs copying `Util::trim()`.
- Match existing style: 4-space indent, `//***` banner comments above functions, `using namespace std;` in .cpp.

## Git

Repo was `git init`ed on 2026-09-15 from the final source snapshot (no upstream history here; the public
history is at github.com/screemerpl/cbleemsync). Commit per logical refactor step; only commit when asked.
