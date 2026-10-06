# AutoBleem - developer context (short map)

AutoBleem is a game launcher / front-end for the PlayStation Classic (PSC), the Raspberry Pi, a PC USB stick and
Windows. This file is the **short map**; the full reference is **`docs/developer-guide.md`** - read only the
section you need (its headings are listed below). The project-wide knowledge (platforms, site, releases, CI,
decisions) is in `autobleem2/autobleem-main` (`CLAUDE.md`, `docs/`); history in its `docs/history/`.

## Where the code lives

- `autobleem-core/` (submodule, `autobleem2/autobleem-core`): `lib_ableem` (engine + SDL ui library), `ab_core`
  (SDL-free model + services), `ab_classic` (game-agnostic classic screens). Its own short map is
  `autobleem-core/CLAUDE.md`. **A change to shared code = a commit in autobleem-core, then a submodule bump here.**
- Here: `ab_ui` (`src/code/app.*`, the game-aware classic screens in `src/code/gui/`), `ab_evoui`
  (`src/code/evoui/`: the carousel launcher), the executable (`main.cpp`, `autobleem.*`), `apps/abpad/`,
  `payload*/`, `src/resources/` (lang files, platform ini), `tools/`, `ci/`, `docker/run.sh`.
- Themes are the `autobleem-themes/` submodule. Clone with `--recurse-submodules`.

## Build (details: developer guide "Build")

- One image builds every target: `docker/run.sh ci/build.sh native|psc|rpi|rpi64|pcusb|win` (the laptop is where
  teams build). `native` runs the test suites. `AB_TARGET` = `psc|rpi|pcusb|win|dev`; the sources test the
  derived macros (`AB_DEBUG_HOST`, `AB_APPLIANCE`, `AB_ROOT_RELATIVE_LAYOUT`, `AB_HAS_INTERNAL_GAMES`), never the
  CPU or OS.
- Windows dev build: `make_win.sh` (MSYS2 UCRT64), `--no-tests` to skip ctest.
- **C++14; the console's gcc-6** cannot combine an inherited constructor with a member initialised from another
  member - screens spell their constructors out.
- **clang-format** (`tools/format.sh`, `--check` fails the build) and **clang-tidy** (`tools/lint.sh`) must stay
  clean. Debug builds are `-Wall -Wextra` warning-free.

## Rules that bite (details: developer guide "Conventions and gotchas", "The quiet stick")

- **Never `#include <SDL2/...>` under `src/code/`** - new SDL functionality goes into lib_ableem.
- **Every on-screen string is `_()`** and lands in all 16 `src/resources/lang/*.txt` in the same commit
  (`tools/lang_tools.py`); no `=` in a key.
- **A game launch gives the display up**: never keep an `ableem::Texture`/`Font` in an object that outlives a
  launch (only `ThemeAssets`).
- **The quiet stick**: anything that runs per boot/scan/launch writes through `DirEntry::writeFileIfChanged` or to
  the runtime dir; the guard is `autobleem-core/tests/core/test_quiet_stick.cpp`. The Packages index
  (`PackageService`) is RAM only: a scan never writes or creates anything; the one write is an App's `LastPackage=`.
- **Packages** (game data an engine with `Uses=` plays; developer guide "Packages"): a Packages-row entry is never
  launched (`PsGame::package`); the engine gets `AB_PKG_*` and the `{package}` placeholders, replaced per argument.
- **Every version a user sees is `Env::productVersion()`**, never `Version::VERSION`/`FULL_VERSION`.
- Files: `ios::binary` for non-text; **never `readsome()`**; shell scripts and cfg/ini stay **LF**.
- Logging is `PLOG_*` (no `cout`); `PLOG_*` inside an unbraced `if` wants braces.
- No raw owning pointers; screens are stack objects; `override`/`explicit`/`static_cast`/`make_unique`.
- The PSC's own storage (eMMC, `/data`) is never written - only the stick (`/media`) and `/tmp`.

## Testing the UI

`tools/ab_drive.py` (the DebugDriver: `AB_DEBUG_PORT`, `AB_NO_SPLASH=1`) - read `docs/testing.md` first.
`AB_SHOT=<file%d.bmp>` saves the presented frame every 3 s. `tools/make_usb.py usb` builds the smoke-test tree.

## The developer guide's sections (`docs/developer-guide.md`)

Current work - The rest of the project - The virtual gamepad for Apps - Multi-platform Apps - Extensions (ABI:
`AB_SDK_ABI`) - Scanner processors - Where the code lives - Build (the platform model, sccache, CI, PSC/Pi/PC/
Windows builds, libchdr, code style, tests) - Smoke test layout (Windows) - Running on PC - Runtime layout on the
console (boot chain, `/tmp`, SDL2 on the console, power off) - Source map (`src/code/`) - The quiet stick -
Conventions and gotchas - Licence - Git - User manuals.
