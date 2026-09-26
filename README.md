# AutoBleem 2

A game launcher and front-end for the **PlayStation Classic** (and the **Raspberry Pi**). AutoBleem replaces the stock SonyUI, scans a USB stick or SD card for PS1 games, keeps their metadata and cover art in SQLite, and launches games in `pcsx-ab`, `pcsx-abnxt` (the next emulator), or RetroArch.

## Platforms

- **PlayStation Classic** - the primary target
- **Raspberry Pi** (32-bit and 64-bit) - as an appliance on the SD card
- **PC / Linux** - development and testing builds
- **Windows** - development build with MSYS2; a standalone Windows product is assembled by [autobleem2/autobleem-appliance](https://github.com/autobleem2/autobleem-appliance)

## Getting started

Ready-to-use packages for every platform are assembled and released by [autobleem2/autobleem-appliance](https://github.com/autobleem2/autobleem-appliance). Extract a release package to your USB stick or SD card (FAT32 or exFAT), drop PS1 game folders into `Games/`, plug it in, and the launcher starts automatically.

### Stick layout

```
/media (console) or data partition (Pi):
Autobleem/bin/autobleem/   launcher binary, libraries, resources
Autobleem/bin/emu/         pcsx-ab (PS1 emulator)
Autobleem/bin/emunxt/      pcsx-abnxt (PS1 emulator, next version)
Autobleem/bin/db/          regional cover art databases
Autobleem/rc/              boot and launch scripts
Games/                     your PS1 game folders (one folder per game)
  Resident Evil.bin, .cue  (or .chd, .pbp)
  Silent Hill (Disc 1)/    (merged into one game on first scan)
RetroArch/                 RetroArch installation and ROMs for other systems
System/Databases/          game metadata (regional.db, internal.db)
Themes/                    UI themes (folders or .zip)
```

## Building

| Target | Command |
|--------|---------|
| **Windows (dev)** | `make_win.sh` - builds, runs tests, validates translations. Requires MSYS2 UCRT64 with SDL2 packages. |
| **Raspberry Pi (32-bit)** | `make_rpi.sh` then `tools/make_rpi_package.sh` for the installer. |
| **Linux / macOS** | `make_sys.sh` for a native build. |
| **PlayStation Classic** | `make_psc.sh` to cross-compile on a build server, or `docker/run.sh ci/build.sh psc` in the Docker image for all targets. |

See `CLAUDE.md` for detailed build information, platform-specific macros, and coding conventions.

## Architecture

The launcher is built from six linked libraries, each with no dependencies on the ones above it:

- **`ableem_engine`** - SDL-free portable engine: filesystem, ini/cfg files, SQLite game database, metadata from RetroArch's `.rdb`, cover art from thumbnails, disc images (bin/cue, PBP, CHD via libchdr), game scanner, RetroArch playlists, themes as JSON, zip read/write
- **`ableem`** - SDL2 UI layer: window, renderer with perspective cover-flow, textures, fonts, audio, input (pads, keyboard-as-pad)
- **`ab_core`** - AutoBleem's model and services (no SDL): game library, queries, settings, memory cards, resume points, launching, the background scan
- **`ab_classic`** - Theme-aware classic UI: launcher singleton, text rendering, theme assets, splash/confirm/keyboard screens
- **`ab_ui`** - Game-aware classic screens: Options, game editors, Game Manager, memory cards
- **`ab_evoui`** - EvolutionUI: the carousel and the launcher (the primary screen)

## Features

- **EvolutionUI** with background scanning - games appear on the stick without rebooting
- **Cover flow** - a perspective shelf of PS1 games in jewel cases, other systems in big boxes
- **Multiple PS1 emulators** - choose between `pcsx-ab` and `pcsx-abnxt` in Options
- **theme.json themes** - typed theme format with partial-over-default merging; old `theme.ini` folders are converted in place; themes can be dropped as `.zip` files
- **Metadata and cover art** from RetroArch's database and thumbnail sets
- **Multi-disc game folders** merged automatically
- **RetroArch integration** - play other systems (Mega Drive, SNES, NES, etc.)
- **Light-gun games** as a separate set
- **Memory card management** - create, rename, and switch between cards per game
- **Hardware Information** screen (CPU, RAM, storage, network)
- **17 languages** with Key=Value translation files

## Code structure

```
lib_ableem/             SDL-free engine + SDL UI library (vendored: sqlite, json, miniz, plog, libchdr)
src/code/
  core/                 model + services (ab_core library)
  gui/                  classic UI (ab_classic + ab_ui libraries)
  evoui/                launcher UI (ab_evoui library)
  app.*                 app model + main entry point
  autobleem.*           application class
  main.cpp
src/resources/          bundled themes, languages, fonts, platform configs
apps/abpad/             virtual gamepad mapper for third-party Apps
payload/                console USB tree: boot scripts, themes, rc glue
payload_linux/          Raspberry Pi installer package
```

**Shared code**: This repo uses [autobleem-core](https://github.com/autobleem2/autobleem-core) as a submodule (library headers, cmake files, core tests). Clone with `git clone --recurse-submodules`.

**Console tools**: PSC-Bios (WiFi, timezone, gamepad wizard) and ABFlashKit (kernel flasher) are in [autobleem-console-tools](https://github.com/autobleem2/autobleem-console-tools). UpdateRoms and the installers are in [autobleem-pc-tools](https://github.com/autobleem2/autobleem-pc-tools).

## Runtime

- **PlayStation Classic**: The launcher runs at 720p on the console's HDMI output. Boot chain is the exploit payload → AutoBleem's `rc/boot.sh` → launcher.
- **Raspberry Pi**: Runs as a systemd service on a 32-bit or 64-bit Pi OS install. Games are on an exFAT partition of the same SD card.
- **Development (Windows)**: The launcher reads from a fake USB root in one-arg mode (`autobleem-gui <root>`) or two-arg debug mode (`autobleem-gui <regional.db> <Games dir>`).

## Licence

**GNU General Public License, version 3 or later** - see `LICENSE` and `TRADEMARKS.md`.

- Source code and binaries: GPL-3.0-or-later, copyright 2018-2026 screemer and the AutoBleem contributors
- Third-party components: see `THIRD_PARTY_NOTICES.md` (SQLite, nlohmann json, miniz, plog, libchdr with lzma/zlib/zstd, SDL2 family, fonts under SIL OFL, etc.)
- Name, logo, and theme artwork are excluded from the licence grant - see `TRADEMARKS.md`

## Development

- **CLAUDE.md** - the canonical source of developer knowledge: architecture, conventions, build details, runtime layout, history
- **`tools/format.sh`** - auto-format sources with clang-format
- **`tools/lint.sh`** - check with clang-tidy
- **Tests** - `tests/` with doctest; every extracted service ships with tests in the same commit
- **Manual** - `manuals/` - user manual in Markdown, built to HTML and PDF

## Credits

AutoBleem 2 is by screemer. AutoBleem-NG (Axanar, cornelk, and contributors) for the features ported back. PCSX-ReARMed (notaz) and RetroArch communities for the emulators.

**This tool is made to be used with legally owned games only. It does not alter any file on the console's internal storage.**
