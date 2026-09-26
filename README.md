# AutoBleem 2

A game launcher and front-end for the **PlayStation Classic** (and the **Raspberry Pi**). AutoBleem replaces the stock SonyUI, scans a USB stick or SD card for PS1 games, keeps their metadata and cover art in SQLite, and launches games in `pcsx-ab`, `pcsx-abnxt` (the next emulator), or RetroArch.

## Platforms

**Products**: PlayStation Classic (`psc`), Raspberry Pi 32-bit and 64-bit (`rpi`, `rpi64` - appliances on the SD card), 32-bit Debian PC USB stick (`pcusb`), Windows (`win` - NSIS installer). Development builds on Linux/macOS (`make_sys.sh`) and Windows MSYS2 (`make_win.sh`).

## Getting started

Ready-to-use packages for every platform are assembled and released by [autobleem2/autobleem-appliance](https://github.com/autobleem2/autobleem-appliance). Extract a release package to your USB stick (FAT32 on a stock PlayStation Classic; exFAT requires the AutoBleem kernel) or SD card on a Raspberry Pi, drop PS1 game folders into `Games/`, and plug it in. On the console, the exploit payload in the stick's `/media/028c18a9-ec4b-4632-b2cf-d4e20f252e8f/` folder boots AutoBleem automatically.

### Stick layout

```
/media (console) or data partition (Pi):
Autobleem/bin/autobleem/   launcher binary, libraries, resources
Autobleem/bin/emu/         pcsx-ab (PS1 emulator)
Autobleem/bin/emunxt/      pcsx-abnxt (PS1 emulator, next version)
Autobleem/bin/db/          regional cover art databases
Autobleem/rc/              boot and launch scripts
Autobleem/lib/libs.tar.gz  shared SDL2 libraries (unpacked to /tmp/lib at boot)
Games/                     your PS1 game folders (one folder per game)
  !SaveStates/             save-state slots per game
  !MemCards/               shared memory cards
Apps/                      third-party apps (ABFlashKit, etc.)
Extensions/                launcher extensions (PSC-Bios, Store, etc.)
RetroArch/bin/             RetroArch binary, cores, configuration
RetroArch/bios/            BIOS files for cores
RetroArch/roms/            other systems' games
System/Databases/          game metadata (regional.db, internal.db)
System/Logs/               launcher and system logs
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

The launcher is built from libraries that link only the one below them:

- **`ableem_engine`** - SDL-free portable engine: filesystem, ini/cfg files, SQLite game database, metadata from RetroArch's `.rdb`, cover art from thumbnails, disc images (bin/cue, PBP, CHD via libchdr), game scanner, RetroArch playlists, themes as JSON, zip read/write (in `autobleem-core`)
- **`ableem`** - SDL2 UI layer: window, renderer, textures, fonts, audio, input; links `ableem_engine` (in `autobleem-core`)
- **`ab_core`** - AutoBleem's model and services (no SDL): game queries, settings, memory cards, launching; links `ableem_engine` (in `autobleem-core`)
- **`ab_classic`** - Theme-aware classic UI: `Gui` singleton, text rendering, theme assets, splash/confirm/keyboard screens; links `ab_core` + `ableem` (in `autobleem-core`)
- **`ab_ui`** - Game-aware classic screens (Options, editors, Game Manager); links `ab_classic` (in this repo)
- **`ab_evoui`** - EvolutionUI: carousel and launcher; links `ab_ui` (in this repo)



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

**In autobleem-core submodule**:
- `lib_ableem/` - SDL-free engine + SDL UI library (vendored: sqlite, json, miniz, plog, libchdr)
- `src/code/core/` - model + services (ab_core library)
- `src/code/app_base.*` and `src/code/gui/` (partial) - classic UI framework (ab_classic library)

**In this repository**:
- `src/code/main.cpp`, `autobleem.*` - the executable; `src/code/app.*` - the app model (ab_ui)
- `src/code/gui/` (game-aware screens) and `src/code/evoui/` - launcher UI (ab_ui and ab_evoui libraries)
- `src/resources/` - themes, languages, fonts, platform configs
- `apps/abpad/` - virtual gamepad mapper for third-party Apps
- `payload/` and `payload_linux/` - console USB tree and Pi installer package

Clone with `git clone --recurse-submodules`.

**Separate repositories**:
- [autobleem-console-tools](https://github.com/autobleem2/autobleem-console-tools): PSC-Bios, ABFlashKit
- [autobleem-pc-tools](https://github.com/autobleem2/autobleem-pc-tools): UpdateRoms, installers
- [autobleem-appliance](https://github.com/autobleem2/autobleem-appliance): assembles packages from all repos

## Runtime

- **PlayStation Classic**: The launcher runs at 720p on the console's HDMI output. Boot chain is the exploit payload → AutoBleem's `rc/boot.sh` → launcher.
- **Raspberry Pi**: Runs as a systemd service on a 32-bit or 64-bit Pi OS install. Games are on a data partition of the same SD card.
- **Development (Windows)**: The launcher reads from a fake USB root in one-arg mode (`autobleem-gui <root>`) or two-arg debug mode (`autobleem-gui <regional.db> <Games dir>`).

## Licence

**GNU General Public License, version 3 or later** - see `LICENSE` and `TRADEMARKS.md`.

- Source code and binaries: GPL-3.0-or-later (`LICENSE`)
- Third-party components: see `THIRD_PARTY_NOTICES.md` (SQLite, nlohmann json, miniz, plog, libchdr with lzma/zlib/zstd, SDL2 family, fonts under SIL OFL, etc.)
- Name, logo, and theme artwork are excluded from the licence grant - see `TRADEMARKS.md`

## Development

- **CLAUDE.md** - the canonical source of developer knowledge: architecture, conventions, build details, runtime layout, history
- **`tools/format.sh`** - auto-format sources with clang-format
- **`tools/lint.sh`** - check with clang-tidy
- **Tests** - `tests/` with doctest; every extracted service ships with tests in the same commit
- **Manual** - `manuals/` - user manual in Markdown, built to HTML and PDF

## Credits

AutoBleem 2 is by screemer. The AutoBleem-NG contributors, whose fork's features were ported back. PCSX-ReARMed (notaz) and RetroArch communities for the emulators.

**This tool is made to be used with legally owned games only. It does not alter any file on the console's internal storage.**
