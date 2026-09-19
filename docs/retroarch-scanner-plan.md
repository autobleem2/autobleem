# Scanning RetroArch's ROMs from AutoBleem - the plan (complete, 2026-09-18/19)

Written 2026-09-18 morning after the Raspberry Pi got its BIOS pack, parked until real ROMs had been run on
the Pi through RetroArch's own scanner; revised the same evening once they had (846 ROMs over nine systems,
Sega and ColecoVision confirmed running) and after the day's launcher work. **All five steps are in**
(that evening and the next night, see the "Done" notes under each). What is left is noted at the end.

**RetroArch is optional on every platform** (the owner, 2026-09-18): none of this runs unless RetroArch is
detected - `ScanService::romScanEnabled()` = `Env::retroArchInstalled()` (the binary the platform ini
names exists) and the ROM folders exist. A stick or a Pi without RetroArch never sees a ROM pass, a
`roms.fingerprint` or a playlist write.

## What it is for

Today a game for another system reaches the carousel in one of two ways: RetroArch's own *Import Content*
(a menu the user has to find, one system at a time), or a playlist written by hand. The scan that already
watches `Games/` for PS1 games should do the same for the ROM folders: copy a ROM in, the carousel updates.
And it has to do that on **all three targets**:

| | PlayStation Classic | Raspberry Pi | PC (dev) |
|---|---|---|---|
| RetroArch tree | RetroBoot's, `/media/retroarch/` | `RetroArch/` on the data partition | `usb/retroarch/` (console layout) |
| ROMs | `/media/roms/<system>/` (RetroBoot's, what the old playlists point at) | `RetroArch/roms/<system>/` (a folder per rdb name, made by `install.sh`) | `usb/roms/<system>/` |
| `.rdb` databases | shipped by RetroBoot (`database/rdb/`) | downloaded by the installer | whatever `make_usb.py` copies |
| thumbnails | none unless the user adds them | PS1 mirrored by the installer, others only if fetched | none |
| network | **maybe** - the AutoBleem kernel and a USB adapter, or nothing at all; else `UpdateRoms.exe` on a PC (step 5) | usually | yes |
| SDL / tools | 2.0.12, busybox, no `wget` to rely on | 2.32, `wget` and `curl` | `curl.exe` |

So the scanner's core must work **fully offline from the file names alone**, with identification by
database and box-art download as extras that switch on when their inputs exist - and the "where things are"
and "how to fetch" differences stay **data** in `resources/platform/<platform>.ini`, not `#ifdef`s
(the rule `PlatformConfig` already enforces for `retroarch_dir`/`retroarch_core`/`retroarch_binary`).

## What already exists (and what changed today)

- `ableem::RetroArchPlaylist` (`engine/retroarch_playlist.h`) - `.lpl` load/save, JSON (1.0 six fields)
  and six-line. **RetroArch 1.22 on the Pi writes version 1.5** with a header (`default_core_path`,
  `scan_content_dir`, `sort_mode`, ...) - the loader must keep unknown header fields when it rewrites a
  file, or write 1.0 and let RetroArch upgrade it (it does, harmlessly). Check which it does today.
- `RetroArchService` (`core/services/retroarch.cpp`): parses every `info/*.info` (`corename`,
  `supported_extensions`, `database`, `block_extract`), picks the core for a database name with
  `resources/platform/<platform>.cores.cfg` on top, and `mapPlaylistPath()` turns the console's `/media/...`
  into the Pi's mount. `ensureLoaded()` reads the playlists once per run - the scanner needs a
  `reloadPlaylists()`.
- `ScanService`: the idle-priority worker, `GamesFingerprint` (path + size, **no mtime** - no RTC), the
  watcher's debounce, `poll()` -> `ScanUpdate` -> `GuiLauncher::reloadGames()`. The PS1 path applies its
  results on the main thread through `regional.db`; playlists will be files the worker writes itself - a
  different contract, kept explicit in `WorkerEvent`.
- `ableem::RdbReader` - rmsgpack, indexed by serial and name; a CRC index is a few lines (records carry
  `crc`, `size`, `rom_name`). `ableem::ZipArchive` - `list()` can give each entry's CRC from the central
  directory for free; `mz_crc32` hashes plain files.
- `ableem::ThumbnailLookup` - box art by name with the fuzzy fallback, cached listings.
- **The reference playlists**: the Pi's, made by RetroArch's scanner for 4 games a system and by a
  one-off script for the other 800 (this session): entries are `<roms>/<system>/<file>.zip#<entry>`
  with the entry's CRC (RetroArch does the same for a single-entry zip), `label` = the rdb `name` when
  RetroArch identified it, else the file's stem. Arcade sets sit in `roms/Arcade/` with `FBNeo - Arcade
  Games.lpl`. Whatever we write must round-trip with those.
- The carousel: a RetroArch game is a big box now (`PsCarouselGame`), art from `ThumbnailLookup`; the
  RetroArch set reloads through `reloadGames()` like the PS1 sets.

## The work

Five steps, each its own commit with tests (`tests/core/`), in this order. 1 alone is the feature; 2-4 make
it good; 5 is the console's way to 2-3 without a network of its own.

### 1. `ableem::RetroArchScanner` - the offline scan

An engine class (no SDL, no `App`), given: the ROMs root, the playlists directory, a *system table* (per
folder name: the core to write, the extensions to accept, whether the core reads archives itself), and a
`ScanProgressListener`. For each `roms/<system>/` folder:

- list the files whose extension the system accepts; **one entry per game**, not per file: a `.cue` hides
  the `.bin`s it names, an `.m3u` hides its `.cue`s/`.chd`s, a `.zip` is one entry (`file.zip#entry` when
  it holds one ROM and the core does not `block_extract`, plain `file.zip` for an arcade set);
- label = the file's stem (tags kept - `Adventures of Lolo (USA)` is what the thumbnails are named after);
- merge into the existing `<system>.lpl`: keep every entry not under our `roms/` folder (the user's own
  additions), keep an entry we or RetroArch wrote before when its file is still there (its label and
  CRC may be better than ours), add the new, drop the vanished; write to a temp file and rename. Never
  touch `AutoBleem.lpl` (the PS1 export), Favorites or History.

Hooked into `ScanService`'s worker after the PS1 pass: `GamesFingerprint` extended to the ROM folders (850
zips is nothing), a `WorkerEvent::Kind::PlaylistsWritten` the main thread turns into
`RetroArchService::reloadPlaylists()` + `ScanUpdate::retroArchChanged`, and `GuiLauncher` reloads the set
when it is the one showing. The system table comes from `RetroArchService` (its `.info` map and
`cores.cfg`) on the Pi/PC; on the console the same code runs against RetroBoot's `info/`.

Platform data: `resources/platform/<platform>.ini` gains `retroarch_roms_dir` (`roms` on the console and
PC, `RetroArch/roms` on the Pi) - `Environment::getPathToRetroarchRomsDir()`.

Tests: a temp tree with a few systems (a zip with one entry, a cue+bin pair, an m3u, an arcade zip, a
stray `.txt`), an existing playlist with a foreign entry and a stale one, and the merged result checked
field by field.

**Done 2026-09-18** (`tests/core/test_retroarch_scanner.cpp`, plus two cases in `test_scan_service.cpp`).
What differs from the text above:

- The `.info` parsing and the database->core mapping left `RetroArchService` for the engine as
  `ableem::CoreInfoTable` (`engine/retroarch_cores.h`, `block_extract` read too); the service keeps one, the
  scan worker builds its own (the service belongs to the main thread). `RetroArchScanner::systemsFrom()`
  turns it into the system table.
- `RetroArchScanner::Options{romsDir, playlistsDir, targetRomsDir}` - `targetRomsDir` is the step-5 knob
  (the prefix the playlists name), `""` = `romsDir`; an existing entry counts as "ours" under either.
- Archives are always candidates, whatever the core's extension list says: a `.zip` for a core that does
  not read archives itself is opened (`ZipArchive::listEntries`, CRCs from the central directory - so step
  1 already writes the ROM's CRC for zipped games, and it is the CRC RetroArch's scanner records: Adventures
  of Lolo came out `D9C4CBF7` both ways), one accepted ROM inside = `zip#rom` named after the zip, several =
  one entry each named after the ROM, none = skipped; a `.7z` goes in whole (miniz cannot look inside).
  **"Reads archives itself" is `zip` among the core's `supported_extensions`** (`RetroArchSystem::
  readsArchives()`): no `.info` in the current libretro bundle says `block_extract` at all - fbneo is
  `zip|7z|cue|ccd` - so that flag is only honoured in addition. `.ccd` hides its `.img`/`.sub` next to the
  cue/m3u rules.
- **Folder aliases**: the Pi's `roms/Arcade/` (and `SNK - Neo Geo/`) feed `FBNeo - Arcade Games.lpl`, as
  RetroArch's own scanner files them - `resources/platform/roms_folders.cfg` (`<folder>=<database>`),
  `RetroArchScanner::Options::folderAliases`; two folders may share one playlist, each pass treats the
  other's entries as foreign.
- `RetroArchPlaylist` keeps the header (`RetroArchPlaylistHeader`, every top-level field but `items` as
  opaque JSON text, `version` first on save; a six-line file has none and comes back as JSON 1.0). Checked
  against a hand-written copy of what RetroArch 1.22 wrote on the Pi (15 header fields).
- A playlist is rewritten only when the merge changed something (`DirEntry::replaceFile` over a `.tmp` -
  `MoveFileEx` on Windows, `rename` elsewhere); an unreadable one is left alone; a folder that yields
  nothing and had no playlist gets none. The merged list is sorted by label, case-insensitively.
- `GamesFingerprint::takeAllFiles()` over the ROM folders -> `roms.fingerprint`; `checkForChanges()` waits
  for both trees to be still, `ScanService::fingerprintsMatchDisk()` is the startup check for both.
- The launcher: `ScanStage::ScanningRoms` on the status line ("Scanning ROMs 2/9: <system>"), the ROM
  count in the "Scan complete" line, and `ScanUpdate::playlistsWritten` -> `GuiLauncher::
  refreshPlaylistNames()` + a `reloadGames()` of the RetroArch set (a playlist game is re-found by its
  image path, its id being only its position).
- `tools/make_usb.py` fakes a RetroArch install (stub binary, `fake_libretro.info` for three systems,
  zipped ROMs) so the PC smoke test runs the pass.
- **Verified on the Pi 400** (2026-09-18 evening, 848 ROMs over 9 populated folders, 54 folders in all,
  the pass takes ~1 s): the first run left the 7 RetroArch-written playlists byte-identical and rewrote
  only Genesis (added `Bubsy II (USA, Europe).gen`, which RetroArch's scanner had skipped) and Arcade
  (added `neogeo.zip` - the BIOS set, **a wart step 2 removes** by keeping only sets the FBNeo rdb knows).
  Deleting the NES playlist, moving a SNES ROM out and copying a NES ROM in were all picked up in one
  cycle 17 s later (NES rebuilt from the files, SNES minus one, the copy in as `zip#rom`); putting them
  back rewrote both again with no `.tmp` left behind, the 1.5 header intact. **Not yet on a console.**

### 2. Identification by database (offline where the `.rdb`s are)

`RdbReader` gets a CRC index. Per system folder, if `<retroarch>/database/rdb/<system>.rdb` exists, open
it (5-30 MB, one at a time, in the worker), CRC the file - for a zip, each entry's CRC from the central
directory, then the archive's own - and take the rdb's `name` as the label and its `crc` as the entry's;
no match keeps step 1's label. Arcade sets match by the archive's name in the FBNeo/MAME rdbs (`rom_name`)
rather than CRC. CD systems (PC Engine CD, Neo Geo CD, Sega CD, 3DO): keep the file name - their serial
formats differ per system and are not worth it yet.

This works on the console too when RetroBoot's rdbs are there, and costs nothing when they are not.

**Done 2026-09-19.** What differs from the text above:

- `RdbReader` reads `crc` (4-byte binary, an integer accepted too), `size` and `rom_name`;
  `findByCrc`/`findByRomName`. `ableem::Crc32` (`engine/crc32.h`, over miniz) hashes a loose file
  streamed, with a cap - `Options::maxCrcBytes`, 64 MB - so a CD image is never read; a zip member's CRC
  comes from the central directory as before.
- `RetroArchScanner::identify()` runs on each folder's `ScannedRoms` before the merge when
  `Options::rdbDir` names the databases (`ScanService` passes `Env::getPathToRetroarchRdbDir()`): a
  member or loose file by CRC, an arcade set (`wholeArchive`) by `rom_name` - **not** by the archive's
  CRC, which is the reference set's and a repacked one would miss. A hit sets the label to the record's
  `name`; a miss keeps the stem, nothing is dropped. The merge rule grew one clause: an identified entry
  replaces an existing entry for the same ROM (full path, member included) whose label differs - so a
  file-name label from an earlier scan or a hand-written one is corrected, while RetroArch's own identified
  entries (same name) stay untouched.
- CD systems are simply the size cap; no serial logic.
- **Metadata**: `RetroArchService::ensureMetadata()` opens `<rdb dir>/<playlist>.rdb` the first time a
  playlist is asked for, fills publisher/year/players by `findByName(label)` and drops the reader
  (Favorites/History copy from the source playlist). The meta panel shows "publisher, year", then the
  core on its own line, then "n Players" for a RetroArch game the database knows; a game it does not
  know looks as before (the core's name).
- **On the Pi 400**: 679 of 848 named in ~2 s for all 54 folders (the 30k-record NES rdb included) -
  NES/SNES/Genesis/Odyssey2 100%, Intellivision 127/131, 7800 70/86, ColecoVision 63/141, Atari 5200
  0/71: the misses are dumps the databases do not have, they keep their file names. `neogeo.zip` is
  "Neo Geo" now (it is a record in the FBNeo rdb, not a BIOS flag - the plan's "step 2 drops it" was
  wrong; it stays listed, honestly named). The 8 rewritten playlists are RetroArch's with the renamed
  labels, headers intact.

### 3. Box art - offline first, online when there is a network

Offline: `ThumbnailLookup` already finds `<retroarch>/thumbnails/<system>/Named_Boxarts/<label>.png`; with
step 2's labels the names match exactly. The scanner records the resolved path in the playlist? No - the
`.lpl` has no field for it and RetroArch would drop it; the launcher looks it up as it does today.

Online (`OnlineAssets`, a core service, worker-side): for every entry with no thumbnail, fetch
`https://thumbnails.libretro.com/<system>/Named_Boxarts/<escaped label>.png` into the thumbnails tree.
The fetch is **an external command named by the platform ini** (`download_command=curl -sfL -o "%o" "%u"`
on the PC, `wget -q -O "%o" "%u"` on the Pi, empty on the console until a tool is known to be there) run
through `System::execUnixCommand` from the worker with a timeout - the app has no HTTP client and is not
getting one for this. Gated by a config.ini key (`online=true`, an Options row "Fetch box art online"),
by the ini having a command, and by one cheap reachability probe per scan (the same command against
`thumbnails.libretro.com/`, 3 s). A console with no network never notices; one with the AutoBleem kernel
and an adapter gets covers.

Also the `.rdb`s themselves, the same way, when `database/rdb/` is empty and the network is there:
`buildbot.libretro.com/assets/frontend/database-rdb.zip` is 40 MB, unpacked with `ZipArchive` - so a
console that does have network is not left without step 2.

**Done 2026-09-19** (`core/services/online_assets.*`, `tests/core/test_online_assets.cpp`). As planned,
with these particulars:

- `OnlineAssets` runs the platform ini's `download_command` through `std::system` from the scan worker
  (`curl -sfL -m 20 -o "%o" "%u"` on the PC and the Pi - curl's `-m` is the timeout, the app has none of
  its own; empty on the console). Into a `.part` renamed on success, so a cut-off fetch leaves nothing.
- Gate: config.ini `online=true` (default; Options row "Fetch box art online", shown only where the ini
  has a command) **and** a command **and** one probe per cycle (`<thumbnails base>/`), and only when there
  is something to fetch - a Pi with every cover on disk makes no request at all. `ScanService::setOnline()`
  is fed by `App::applyOnlineSetting()` at start and after Options.
- Order in `scanRetroArchRoms()`: `ensureDatabases()` (the bundle, when `database/rdb/` has no `.rdb`;
  `rdb/*.rdb` inside, unpacked into `database/`) -> the ROM scan with identification -> `fetchBoxArt()`
  over `RetroArchScanResult::games` (every entry under the ROM folders, with its final label) for the
  ones the worker's own `ThumbnailLookup` finds nothing for (fuzzy fallback included).
- A server miss is remembered in `<thumbnails>/<system>/Named_Boxarts/.autobleem-missing.txt` (one
  escaped name per line; delete it to retry) after a re-probe confirmed the server is up; the network
  dropping mid-pass ends the pass without marking anything. `ScanUpdate::boxArtFetched` ->
  `app.thumbnails().clearCache()` + a reload of the RetroArch set; `ScanStage::FetchingBoxArt` on the
  status line.
- **On the Pi 400**: 67 games without a cover, 36 fetched (the arcade sets under their rdb names among
  them), 31 not on the server, ~1 min for the pass; a second scan makes no request.

### 4. What the user sees

- The scan status line already shows the PS1 pass; it gets the ROM pass too ("Scanning Sega - Mega
  Drive - Genesis... 99 games").
- The Game Manager's per-game preview works for RetroArch games (it already reads the thumbnails tree).
- `payload_rpi/README.md`'s "Games for the other systems" shrinks to "copy them in, wait for the line".

**Done 2026-09-19**, as far as it goes: the status line came with step 1 ("Scanning ROMs n/m: <system>",
"Fetching box art n/m: <game>" with step 3, the ROM count in "Scan complete"), the README with steps 1-3.
The Game Manager item was **left out on purpose**: it lists USB PS1 games only - it is the folder manager
(delete a game, flush covers) - so "its preview for RetroArch games" would mean listing ROMs there with a
delete action, a feature nobody asked for. The carousel shows the covers; the meta panel the metadata.

### 5. `UpdateRoms.exe` - the console's online path, from a PC

The PlayStation Classic's USB stick spends its life being plugged into a PC to get games copied on. So
the steps that want a network run **there**: a small Windows program shipped in the stick's root
(`payload/UpdateRoms.exe`, built by `make_win.sh` next to the launcher - links `ableem_engine` + `ab_core`
and, for its window, `ableem`; everything `-static` so it runs on any Windows without MSYS2), which, run from
the stick, does for the RetroArch folders exactly what the console's scan would do with a network:

- finds the stick's root from its own location (the drive it sits on), reads the same
  `resources/platform/psc.ini`, `info/` and `cores.cfg` the console would, so its idea of systems and
  cores is the console's;
- step 1's scan and step 2's identification, writing the playlists **with the console's paths**
  (`/media/roms/...`, never `E:
oms\...`) - `RetroArchScanner` takes the target's root prefix as a
  parameter for exactly this, and its tests cover a Windows source tree mapped to a `/media` target;
- step 3's downloads with the PC's network: thumbnails for every entry, `database-rdb.zip` when
  `database/rdb/` is empty - `download_command` from the ini is `curl.exe` (in every Windows since 10);
- shows a **small window** while it works, not a console: built on `lib_ableem`'s ui half (SDL2 linked
  statically - MSYS2 ships the `.a`s - so the exe is still one file, ~3 MB), so it has the launcher's
  logo, font and colours for free. One screen: the AutoBleem logo, a line saying which system it is on
  ("Nintendo - Nintendo Entertainment System - 37 of 99"), a progress bar over the whole run (files
  scanned + downloads pending), a scrolling log underneath (one line per system: "99 games, 91
  identified, 12 covers fetched"; a warning line for a folder it does not know), and a Close button
  when it is done - plus a summary of what changed. The scan runs on a worker thread reporting through
  `ScanProgressListener` (the same interface the console's `ScanService` uses) and the main thread draws
  at 30 fps, exactly `ScanService`'s pattern. `--quiet` runs it without the window for scripts, printing
  the same log lines.

Nothing on the console changes: it boots, its scan sees the playlists and the thumbnails already there
(they are ordinary files under `retroarch/`), and its own offline pass has nothing left to do. The same
program on a Pi's SD card in a PC reader does the same for `RetroArch/roms/` (it reads `rpi.ini` when
the card's tree says it is a Pi - `Autobleem/rc/launch_rb.sh`'s Pi header, or simpler, a `platform=`
line the installer writes into `config.ini`). A Linux/macOS build of the same tool is `make_sys.sh`'s
business if anyone asks.

Not in scope for it: PS1 games (the console's own scan does those, and it needs no network), themes,
anything that touches the console's databases.

**Done 2026-09-19** - `apps/updateroms/` (its own CLAUDE.md has the detail). As planned, with:

- The stick's kind is **not** told by the RetroArch folder's name: a stick in a PC is exFAT, where the
  Pi's `RetroArch/` and RetroBoot's `retroarch/` are the same folder. RetroBoot's own folder is the
  console's mark, the Pi installer's `retroarch.cfg` the Pi's; `--target psc|rpi` overrides.
- The target's `usb_root` is a new key in `platform/psc.ini` (`/media`) and `rpi.ini`
  (`/media/autobleem`), read by the tool only; the PC's `pc.ini` supplies `download_command`.
- Fresh entries' core paths are mapped onto the target's RetroArch dir, and `RetroArchScanner::merge`
  now maps a kept entry that names *this* machine's ROM folder (a launcher scan run on the PC) onto the
  target's - both were this PC's drive letter otherwise.
- The window: first written as `AppBase` on `ab_classic` with the stick's theme, which made the exe 28 MB
  plus 50 MB of MSYS2 DLLs (SDL2_image/mixer with every codec); the owner then asked for **a plain Win32
  window, no SDL, no AutoBleem rendering** - `win32_window.cpp`, statically linked, a `-mwindows` exe
  that `--quiet` can still print from (`AttachConsole`). `tools/make_updateroms_bundle.sh` builds it
  Release into `build_updateroms/`, strips and UPX-packs it: **one 540 KB file** plus README.txt.
- Verified against the fake tree with the real network (146 databases fetched and unpacked, playlists with
  `/media/...` paths, covers fetched); **not against a real console stick** - the console has never run
  this build.

## What is left (2026-09-19)

- The console has never run any of this: steps 1-3 are verified on the Pi 400 and the PC, step 5 on the
  PC's fake tree. `/media/roms` on the console is from the old playlists, not from hardware.
- `neogeo.zip` next to the arcade sets is listed as "Neo Geo" (it is an FBNeo database record); the BIOS
  belongs in `system/`, which the Pi README says.

## Caveats to keep in view

- Merge rules against playlists RetroArch also rewrites: never fight it, never touch Favorites/History,
  and keep RetroArch-written entries' CRCs/labels over ours.
- The worker writes files RetroArch may be reading (a RetroArch launched from the carousel while the scan
  runs): write + rename, and no scanning while a launch is in progress (`ScanService::setWatching(false)`
  around it, as for PS1).
- A `roms/` folder name we do not know (no `.info` lists it, not in `cores.cfg`): skip it, say so once.
- The console has never run this build; step 1 is pure logic and needs the PC and Pi only, but its console
  paths (`/media/roms`, RetroBoot's `info/`) are from the old playlists, not verified on hardware.
- 32-bit `size_t` on the Pi and the console: CRCing a 700 MB `.chd` is fine, but do not read it into memory.
