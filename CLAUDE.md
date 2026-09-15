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

Still to do, in order:

1. `enum class` for the `#define`-int "enums" (`EMU_*`, `SET_*`, `STATE_*`, `SEL_OPTION_*`) - compiler-driven.
2. Centralize the remaining hard-coded paths (`/media/...`, `/tmp/...`, `/usr/sony/share/data`, `/gaadata`) in
   `Environment`.
3. Split `GuiLauncher` (~2,150 lines across `gui_launcher.cpp` + `gui_launcher_loop.cpp`).
4. Set up the Sony ARM toolchain and verify lib_ableem + the app on a console (nothing above has run on real
   hardware yet - only the Windows/MinGW build has been exercised).
5. Features.

## lib_ableem

A portable static library (`lib_ableem/`, namespace `ableem`) that owns every SDL/SDL_image/SDL_mixer/SDL_ttf
call. The app talks to it only through `include/ableem/*.h` - `ableem.h` pulls in the whole API. Never add a
`#include <SDL2/...>` to anything under `src/code/`; if you need new SDL functionality, add it to the library.

- **`Platform`** - owns SDL_Init/window/TTF_Init/Mix_Init (created by `GuiBase`). `isDevHost()` replaces the
  app's old per-call `AB_DEBUG_HOST` checks for cursor grab; `setPowerOffHandler()` is how the app supplies
  what "power off" means (main.cpp wires it once to `gui->drawText(...); Util::powerOff();`) - `Input::poll()`
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
  (still there, moved to `lib_ableem/src/psc_event_filter.c`, wired up by `Input`'s constructor). `Event::Type`
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
- **CMake**: `add_subdirectory(lib_ableem)` from the root file; `ABLEEM_EMBEDDED_TARGET` is forced on for the
  ARM build (no cursor grab, keyboard-as-pad off); `lib_ableem/examples/demo.cpp` (`ableem_demo` target) is a
  from-scratch smoke test of the library alone - texture + font + sound + input, no AutoBleem code involved.

## Build

Two targets in `CMakeLists.txt`: `autobleem-gui` (the app) and `starter` (small PCSX wrapper used by the
stock-UI path). **C++14** (the Sony toolchain is GCC 8+). `sqlite3` is built from `libs/sqlite/sqlite3ab.c`.
Debug builds compile with `-Wall -Wextra` (a few noisy categories off) - keep them warning-free.

- **ARM (real target)**: `make_arm.sh` → `PSCtoolchainV8.cmake` (`armv8-sony-linux-gnueabihf-gcc`, `--static -Os -s`).
  Requires the toolchain at `/opt/toolchain/armv8-sony-linux-gnueabihf`. Not available on this Windows host yet.
- **Mac/Linux**: `make_mac.sh`, `make_sys.sh`.
- **Windows/MinGW (dev + smoke test)**: `make_win.sh` → `build_win/autobleem-gui.exe`. Uses MSYS2 UCRT64
  (`C:\msys64`, installed 2026-09-15) with `mingw-w64-ucrt-x86_64-{gcc,cmake,ninja,SDL2,SDL2_image,SDL2_mixer,SDL2_ttf,pkgconf}`.
  Invoke from PowerShell as `$env:MSYSTEM='UCRT64'; C:\msys64\usr\bin\bash.exe -lc "cd /e/Programming/autobleem-develop && ./make_win.sh"`.
  Run needs `C:\msys64\ucrt64\bin` on PATH (SDL DLLs). Builds with `-DAB_ENABLE_CHD=OFF`; the `starter` target is
  skipped on Windows. Windows-only shims: `mkdir` one-arg, `sys/wait.h` guarded, `Util::execFork` stubbed.
  The x86/Windows/Pi switch is the single macro `AB_DEBUG_HOST` (defined in `environment.h`) — use it, never
  `__x86_64__` directly.
- **`libmamecd`** (`#include <libmamecd/cdrom.h>`, link `mamecd`) is used only by `engine/cdreader.h` for CHD
  images and is NOT in the repo. `AB_ENABLE_CHD=OFF` defines `AB_NO_CHD`, which compiles out `CHDReader`
  (`.chd` games then scan as "no serial").
- External libs: SDL2, SDL2_image, SDL2_mixer, SDL2_ttf, pthreads, mamecd. Vendored: SQLite (`libs/sqlite`),
  nlohmann json + `fifo_map` (`libs/nlohmann`), `unecm.c` (in `src/code`). SDL_FontCache now lives in lib_ableem.
- `PRE_BUILD` step copies `src/resources/` next to the binary; the app expects to run from that dir.
- No tests exist.

### Smoke test layout (Windows)

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

**Keyboard = gamepad on debug hosts** (`PadMapper::translateKeyboardToPad`, `AB_DEBUG_HOST` only):
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
| `main.h` | | Shared enums/constants (`ImageType`, file extension consts) and inline in-place string helpers (`trim`, `lcase`...). |
| `environment.*` | `Env` | All path getters. `private_*` globals are set once in `main()`. Extend this instead of adding new literal paths. |
| `DirEntry.*` | | Filesystem helpers (dir listing, copy/rename/remove, cue parsing, name fixing). Only place using `dirent`. `checkWritable(ofstream, path)` - call it after opening any output file. |
| `util.*`, `util_time.*` | | String/stream helpers, `execUnixCommand` (popen, returns "" on failure), **`runAndWait(exe, args)`** - the only fork/exec in the code base, `toInt(str, def)` (never throws), `powerOff`. |
| `lang.*` | `_()` | gettext-style lookup from `resources/lang/<Language>.txt`. Emoji markers like `\|@X\|` in strings are replaced by button textures by `Gui::renderText`. |
| `engine/scanner.*` | `Scanner` singleton | Walks `/Games`, repairs cue files (comma names, missing cue), runs `unecm`, verifies discs, fills `gamesToAddToDB`, writes `regional.db` + `autobleem.list`. |
| `engine/GetGameDirHierarchy.*` | `GamesHierarchy` | Sub-directory tree of `/Games` → `SUBDIR_ROWS` tables; `autobleem.prev` file detects changes to force rescan. |
| `engine/game.*` | `USBGame`/`Disc` | A game on USB as discovered by the scanner; reads/writes `Game.ini`; `verify()`. |
| `engine/serialscanner.*`, `cdreader.h`, `isodir.*` | | Extract PS1 serial (SLUS-xxxxx) from BIN/PBP/CHD by reading the ISO9660 dir; CHD via libmamecd. |
| `engine/metadata.*`, `coverdb.*` | | Look up title/publisher/year/cover PNG blob in the 3 regional `covers*.db` (U/E/J). |
| `engine/database.*` | `Database` | Thin SQLite wrapper. Every query uses the file-local RAII `Stmt` class (`ok()/bind()/row()/colInt()/colText()/colBlob()`); add new queries the same way. Schema notes in `engine/database_tables.txt`. Same class serves `regional.db`, `internal.db` and cover DBs; the destructor disconnects. |
| `engine/memcard.*`, `cardedit.*` | | Memory-card swap in/out (`!MemCards`) and .mcd block editor. |
| `engine/cfgprocessor.*`, `inifile.*`, `config.*` | | pcsx.cfg / RetroArch cfg rewriting; generic ini map; `config.ini` (keys are lower-cased on load, e.g. `values["theme"]`). |
| `gui/gui.*` | `Gui` singleton | Owns SDL window/renderer, fonts, theme (`theme.ini` merged over `themes/default`), music/sfx, both DB pointers, carousel restore state (`lastSet`, `lastSelIndex`...). `menuSelection()` is the classic-UI main menu event loop. `display()` (re)inits and shows splash or resumes the launcher. |
| `gui/gui_screen.*` | `GuiScreen` | Base for every screen: `init/render/loop` + virtual `doCross_Pressed()`-style handlers; `show()` runs them. Set `menuVisible=false` to exit. |
| `gui/menus/*` | `GuiMenuBase`, `GuiOptionsMenuBase`, ... | Header-only templated list menus (string, two-column, playlist, game dir) and concrete Options / Memory Cards / Game Manager / Game Editor menus. |
| `gui/gui_*` | | Splash, About, Confirm dialog, on-screen Keyboard, pad test, memcard select, scroll window, star FX. |
| `gui_font.*` | `Fonts`, `FontEnum` | Theme/Sony SST font loader built on `ableem::Font` (SDL_FontCache itself is now in lib_ableem). |
| `launcher/gui_launcher.*`, `gui_launcher_loop.cpp` | `GuiLauncher` | EvolutionUI: cover carousel, sets (PS1 all/internal/favorites/history/sub-dir, RetroArch playlists, Apps), settings overlay, resume-state selector, input loop. |
| `launcher/ps_*.{h,cpp}` | `PsObj` and subclasses | Animated sprite/UI elements of the launcher (carousel, meta panel, menu, buttons, labels). |
| `launcher/ps_game.*` | `PsGame` | Game as seen by the UI (from DB or playlist). `PsGamePtr = shared_ptr<PsGame>`. Handles resume-point pictures/slots. |
| `launcher/ra_integrator.*` | `RAIntegrator` singleton | Parses RetroArch `.lpl` playlists and core info, favorites/history playlists, core override (`coreOverride.cfg`). |
| `launcher/*_interceptor.*` | `EmuInterceptor` strategy | `PcsxInterceptor`, `RetroArchInterceptor`, `LaunchInterceptor` (apps): build argv, fork the `rc/*.sh` launcher, manage memcards and save-state resume points. |
| `launcher/gui_mc_manager.*`, `gui_app_start.*`, `gui_btn_guide.*`, `gui_NotificationLine.*` | | Launcher sub-screens. |
| `ver_migration.*` | | One-off migrations between AutoBleem versions (`/media/System/Logs/ver.txt`). |
| `starter.cpp` | separate binary | Wraps `/tmp/pcsx` for the stock SonyUI path; swaps memcard from `Game.ini`. |
| `unecm.c` | | ECM decoder. |

Payload (`payload/`): the release USB tree — `rc/*.sh` scripts, themes (`aergb`, `autobleem`, `default`,
`evolution`), bundled Apps, release notes. `db/` is git-ignored (cover DBs live there).

## Conventions and gotchas

- Singletons via `static shared_ptr<T> getInstance()`: `Gui`, `Scanner`, `Lang`, `RAIntegrator`.
  `Gui::db` / `Gui::internalDB` / `Gui::coverdb` are non-owning pointers; the objects are `unique_ptr`s in
  `runAutobleem()` (main.cpp). `GuiLauncher`'s named `PsObj*` members are non-owning shortcuts into
  `staticElements`/`frontElemets`, which own them (`addStaticElement(new T(...))`).
- Ownership rule: no raw owning pointers. Short-lived helpers and screens are stack objects; anything that
  must outlive a scope goes in a `unique_ptr`. Exceptions are never thrown on purpose; `main()` has a
  last-resort `catch` that logs to `AB_err.txt`.
- `sep` is the path separator (a `Sep` helper in `DirEntry.h` wrapping `separator`, which is `'\\'` under
  `_WIN32`, `'/'` otherwise); paths are built by string concatenation. Several places still hard-code `"/"` or
  match `"/Games"` — a MinGW build should probably force `'/'` (Windows APIs accept it) rather than mix both.
- Ini keys are lower-cased: `cfg.inifile.values["theme"]`, `themeData.values["background"]`.
- Bool-ish config values are the strings `"true"`/`"false"`; ints are parsed with `atoi`.
- Menu/emulator/state selections are plain `int`s with `#define`s (`EMU_PCSX`, `SET_PS1`, `STATE_GAMES`) —
  easy to mix up; converting to `enum class` is on the plan.
- The carousel duplicates games when fewer than 13 exist, so one `PsGamePtr` may appear in several
  `PsCarouselGame`s (see comment in `gui_launcher.h`).
- SDL lifecycle: `TTF_Init`/`Mix_Init` once in `GuiBase`, `SDL_Quit` registered with `atexit` in `main` so it
  runs after the `Gui` singleton is destroyed. Audio is fully closed (`Mix_CloseAudio` loop) before forking PCSX.
- `Gui::menuSelection()` recurses into itself after every sub-screen; screens created there are wrapped in
  explicit `{}` scopes so they are destroyed before the recursion.
- Console `stdout`/`stderr` go to `/media/System/Logs/AB_*.txt`; `cout` is the logging mechanism and is
  unit-buffered so the last lines survive a crash.
- Files are read/written by bare `ifstream`/`ofstream`; use `ios::binary` for anything that is not text
  (PNG blobs, .mcd cards, PBP headers) or the Windows build corrupts it.
- Scripts that edit sources from Python must pass `encoding='utf-8'` (CLAUDE.md got mangled once).
- Shell scripts and cfg/ini files must stay **LF** (enforced by `.gitattributes`). Do not let the Windows
  editor convert them.
- Keep the two `trim` families in mind: in-place `trim()` from `main.h` vs copying `Util::trim()`.
- Match existing style: 4-space indent, `//***` banner comments above functions, `using namespace std;` in .cpp.

## Git

Repo was `git init`ed on 2026-09-15 from the final source snapshot (no upstream history here; the public
history is at github.com/screemerpl/cbleemsync). Commit per logical refactor step. `.gitattributes` forces LF.
