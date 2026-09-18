# Scanning RetroArch's `roms/` from AutoBleem - a plan, not started

Written 2026-09-18, after the Raspberry Pi got its BIOS pack and one `RetroArch/roms/<system>/` folder per
system with a core. The owner's call: **leave it until real ROMs have been run on the Pi through RetroArch's
own scanner** (`Import Content -> Scan Directory` / `Manual Scan`, see `payload_rpi/README.md`, "Games for
the other systems"). This note is so the analysis does not have to be redone.

## What RetroArch's scanner does

- *Manual Scan*: list files by extension in a folder, label = file name, one system name and one core for
  all of them, write `playlists/<system>.lpl`. Trivial.
- *Scan Directory*: CRC32 of each file - for a `.zip`, each entry's CRC from the central directory, then
  the archive's own CRC (which is why arcade sets must match the core's ROM set exactly) - looked up in
  the `.rdb` databases; label = the database's `name`; CD images are identified by serial instead. The
  expensive part for RetroArch is not knowing which database to try. **We do not have that problem: our
  `roms/` folders are named by system (RetroArch's rdb names, `install.sh`'s `RA_ROM_SYSTEMS`), so the
  folder names the one `.rdb` to consult.**

## What already exists

- `ableem::RetroArchPlaylist` (`engine/retroarch_playlist.h`) - `.lpl` load/save, JSON and six-line.
- `RetroArchService` (`core/services/retroarch.cpp`) - parses every `info/*.info` (`corename`,
  `supported_extensions`, `database`), and already picks "the core for a database name" with
  `resources/platform/<platform>.cores.cfg` on top (`<database name>=<part of a core's display name>`). That is the
  default-core-per-system table.
- `ableem::RdbReader` - the rmsgpack parser, indexed by serial and name; a CRC index is a few lines
  (records carry `crc`, `size`, `rom_name`).
- `ableem::ZipArchive` (miniz) - `list()` can expose each entry's CRC for free; `mz_crc32` hashes plain files.
- `ableem::ThumbnailLookup` - box art by name with the fuzzy fallback; works on a database name or a file name.
- `ScanService` - the background worker at idle priority, `GamesFingerprint` (path + size, no mtime), the
  watcher's debounce, `poll()` -> `ScanUpdate` -> `GuiLauncher::reloadGames()`.

## The work, two commits, each with tests

1. **`RetroArchScanner`, the manual-scan equivalent** (~1 day). Engine class: for each `roms/<system>/`,
   list the files whose extension the system's core supports, build entries (`path`, label from the file
   name, explicit `core_path`/`core_name`, `db_name = <system>.lpl`, crc `DETECT`), merge into the existing
   playlist - keep entries not under `roms/`, keep the user's core choice on an entry we wrote before, add
   the new, drop the vanished - and save (write + rename, RetroArch rewrites these files itself). Hook it
   into `ScanService`'s worker after the PS1 pass, extend `GamesFingerprint` to `roms/` so a dropped ROM
   triggers it, add `RetroArchService::reloadPlaylists()` (today `ensureLoaded()` reads once per run) so the
   RetroArch set refreshes through the same `reloadGames()` path. Result: copy a ROM in, the carousel
   updates, no RetroArch menu needed. RetroArch reads the same playlists.
2. **Database identification** (~1 day more). `RdbReader` CRC index; per system folder open that system's
   `.rdb` (5-30 MB, one at a time - fine on a Pi), CRC the file or the zip entries/archive, take the rdb's
   `name` as the label (proper titles, exact thumbnail names, arcade sets identified instead of "1942"),
   fall back to the file name when nothing matches. CD systems: file name or the `.m3u` - serial formats
   differ per system, not worth it yet.

## Caveats

- Merge rules against playlists RetroArch also rewrites; never fight it, never touch Favorites/History.
- Archive paths: RetroArch's `file.zip#entry` form only when the core cannot read zips itself (the `.info`
  says: `supported_extensions` includes `zip`, `block_extract`).
- `ScanService` applies PS1 results on the main thread through `regional.db`; playlist files would be
  written by the worker directly - a different contract, keep it explicit.
- Where the `.rdb`s are: `<retroarch>/database/rdb/` on both targets (the installer downloads
  `database-rdb.zip`; RetroBoot ships them on the console).
