# AutoBleem 2

AutoBleem is a game launcher for the **PlayStation Classic**: it replaces the stock SonyUI, scans a USB stick
for PS1 games, keeps their metadata and cover art, and starts them in the bundled `pcsx-ab` or in
RetroArch/RetroBoot. Version 2 is a rebuilt AutoBleem - the same product, the same USB layout and the same
themes, on a codebase that was taken apart and put back together during 2026 - and it now also runs as an
appliance on a **Raspberry Pi**.

This repository continues [screemerpl/cbleemsync](https://github.com/screemerpl/cbleemsync) (AutoBleem 0.x
to 0.9.1) and folds in everything the community fork
[AutoBleem-NG/autobleem](https://github.com/AutoBleem-NG/autobleem) added after the original went quiet.
The repository is private while the console build is being brought up; the history starts at the 0.9.1
source snapshot (`a033670`).

> **Status (September 2026):** the Windows development build and the Raspberry Pi build run - the Pi 400
> test box boots into the launcher, scans, and plays PS1 games in pcsx-ab and other systems in RetroArch.
> The PlayStation Classic binaries build and pass the link gates but **have not yet been run on a console**.

## What changed since 0.9.1

### The code

The 2019-2021 GUI was one process-wide `Gui` object with SDL calls, SQLite, the scanner and the screens all
reaching into each other. It is now six libraries, each linking only the one below it:

| Target | What it is |
|---|---|
| `lib_ableem/` `ableem_engine` | Portable, SDL-free engine: filesystem, ini/cfg files, the SQLite game database, cover dbs, disc images (bin/cue, PBP, CHD via libchdr, ECM), the scanner, RetroArch playlists, `.rdb` metadata, thumbnails, memory-card images, themes as `theme.json`, zip read/write, MD5, logging. |
| `lib_ableem/` `ableem` | Every SDL2 call: window, renderer (with perspective "cover flow" and output scaling), textures, fonts, audio, input (pads, keyboard-as-pad, raw joysticks for the mapping wizard). |
| `src/code/core/` `ab_core` | AutoBleem's model and services, no SDL: game queries, catalog, settings, memory cards, resume points, launching, RetroArch integration, the background scan, system info, platform layout. Unit tested. |
| `src/code/gui/` `ab_classic` | The theme-aware classic UI shared with the console tools: `AppBase`, `Gui`, text rendering, theme assets, splash/confirm/keyboard/about/hardware-info screens, the list-menu framework. |
| `src/code/gui/` `ab_ui` | The game-aware classic screens: Options, game editors, Game Manager, memory cards, playlists. |
| `src/code/evoui/` `ab_evoui` | EvolutionUI: the carousel and the launcher. |

Every SDL/SQLite/JSON/miniz/libchdr dependency lives inside `lib_ableem/`; the application includes no SDL
header. There are no raw owning pointers, no uncaught exceptions, one `fork/exec` helper, RAII around every
SQLite statement, and a doctest suite (34 suites, run by every Windows build) for the engine and the core
services. `CLAUDE.md` is the developer documentation - the source map, the conventions and the history of
every step, kept current with the code.

### Ported back from AutoBleem-NG

The fork's 122 commits past our snapshot were reviewed and their functionality re-implemented on the new
structure (each with tests), so AutoBleem 2 has everything the maintained public line has:

- **Metadata from RetroArch's `Sony - PlayStation.rdb`** (title, publisher, year, players, region) with the
  covers databases kept as the fallback, and **covers from the libretro thumbnail sets**
  (`retroarch/thumbnails/`) - no `default.png` copies next to games any more.
- **Multi-disc folders merged** (`Game (Disc 1)`/`(Disc 2)` -> one game) and `.m3u` generation for RetroArch.
- **Light-gun games** as a set of their own, flagged per game.
- **CHD with zstd** (upstream libchdr, vendored) in the launcher and in pcsx-ab.
- Fitted, wrapped and elided text everywhere; Options paging and user font selection; the Game Manager's
  preview pane.
- **Chinese (Simplified)** with a bundled CJK font, on top of the 16 languages the launcher already had;
  translations as `Key=Value` files kept in step by `tools/lang_tools.py`.
- Logging through plog (`System/Logs/autobleem.log`), a compile-time version from git, link gates that
  check the console binary's GLIBC/GLIBCXX ceiling, UPX-packed binaries, clang-format/clang-tidy.
- The Phase-0 bug fixes: `play_us_ra`, locked games keeping their serial, CHD exported as `.chd.cue`, the
  year on the meta panel, music not restarting on theme browse, the Favorites fallback, rc script guards.

Left out on purpose: the fork's Docker/CI pipeline, gtest (doctest does the job) and the RetroBoot 1.2.1
Apps payload.

### New in AutoBleem 2

- **Straight into EvolutionUI.** The classic boot menu is gone; the splash goes directly to the carousel
  and the **scan runs in the background** on a lowest-priority thread while you browse - a game folder
  dropped on the stick appears without a reboot, a pulled one disappears. The old menu's items (Re-Scan,
  RetroArch, Memory Cards, Game Manager, Hardware Information, Options, About, Power Off) live in the
  **L2+R2 system menu**.
- **Cover flow**: a perspective shelf of covers receding into the distance, PS1 games in jewel cases,
  RetroArch games and Apps in big boxes at the art's own aspect. The row is bounded (no wrap-around), with
  translucent empty boxes past the ends. 60 fps on a Pi at 1080p with 1.5x output scaling.
- **`theme.json`** themes: a typed theme format (`docs/theme-format.md`) with partial-over-default merging;
  old `theme.ini` folders are converted in place, a theme can be dropped in as a `.zip`. The stock SonyUI is
  no longer re-skinned. The shipped **`ab2`** theme has redrawn launcher icons and Selawik Light.
- **Hardware Information** screen (`autobleem-gui <root> --sysinfo` prints the same on the command line).
- **Console tools in the tree**: **PSC-Bios** (WiFi, timezone, the gamepad mapping wizard) and
  **ABFlashKit** (kernel flasher) - the 2020 forks under `apps/`, rebuilt on the same libraries, drawn with
  the launcher's theme, translated into all 17 languages, with fakes so they run on a PC for visual testing.
  The launcher loads the wizard's `gamecontrollerdb.txt`.
- **Raspberry Pi port** (`payload_rpi/`): 32-bit Raspberry Pi OS on a Pi 2/3/4/400/Zero 2 W, games on an
  exFAT partition of the SD card, RetroArch built from source with every libretro core, the BIOS pack
  fetched from RetroBIOS, a plymouth boot splash, pcsx-ab for PS1. See `payload_rpi/README.md`.
- The `Cfg=` absolute path in `config.ini` is gone; every path derives from the USB root.

## Building

| Target | How |
|---|---|
| PlayStation Classic | `./make_psc.sh` - cross-compiles on a build server with Sony's GCC 8.2 toolchain, gates the binaries (GLIBC/GLIBCXX ceiling, no RPATH), packs them and drops them into `payload/`. |
| Raspberry Pi (32-bit) | `./make_rpi.sh` with the SysGCC toolchain, then `tools/make_rpi_package.sh` for the installable tarball. |
| Windows (development) | `./make_win.sh` from an MSYS2 UCRT64 shell: builds, runs the tests, validates the language files, checks formatting. `python tools/make_usb.py usb` stages a fake USB root; `tools/win_drive.ps1` drives the exe from the keyboard for smoke tests. |
| Mac / Linux | `make_mac.sh`, `make_sys.sh`. |

C++14, CMake >= 3.12, SDL2 + SDL2_image + SDL2_mixer + SDL2_ttf. Everything else is vendored.
`CLAUDE.md` has the details for each target and the coding conventions (`tools/format.sh`, `tools/lint.sh`).

## Using it on a PlayStation Classic

The USB layout is unchanged from 0.9: extract a release to the root of a FAT32 (or ext4) stick named
`SONY`, drop game folders into `Games/`, plug it in. One folder per game with the image inside:

```
Games/
     Resident Evil 2 Leon/
           Resident Evil.bin, .cue
     Silent Hill (Disc 1)/         <- merged with (Disc 2) into "Silent Hill" on the first scan
     Tomb Raider II/
           Tomb Raider II.chd
     Revolt/
             Revolt.PBP
```

Cover databases go in `Autobleem/bin/db/` as before; a `retroarch/` tree (RetroBoot) with its
`database/rdb` and `thumbnails` gives better metadata and art. Save states and memory cards are kept next
to each game (`!SaveStates`, `!MemCards`). Themes go in `themes/`, as folders or `.zip`s.

## Credits

AutoBleem is by screemer, with the AutoBleem team and the community on Discord. AutoBleem-NG (Axanar,
cornelk and contributors) for the features ported back. RetroBoot by genderbent, cores by KMDFManic.
BleemSync (ModMyClassic) for parts of the early boot scripts. Vendored: SQLite (public domain), nlohmann
json (MIT), miniz (MIT), plog (MIT), libchdr with lzma/zlib/zstd, doctest (MIT), SDL_FontCache (MIT),
unecm (Neill Corlett, GPLv2), Noto Sans SC and Selawik (SIL OFL).

This tool is made to be used with legally owned games only. It does not alter any file on the console's
internal storage. License: see `LICENSE`.
