# The Store (plan)

**Status (2026-09-24):** planned, nothing built. This file describes a screen that downloads and installs
Apps and games from inside the launcher. It came out of a look at Project Eris' "PSC Store"
(`github.com/hampter-mods/pscstore-release`, release-only, closed source). What we take from it is the idea,
not the code: a controller-driven library you can add to without pulling the stick, and a tab-separated list
of download URLs that the user owns.

## What PSC Store does, and what we take

PSC Store is an Eris `.mod` (C/C++, SDL2, GLES, SQLite, curl + OpenSSL, 7-Zip). Its "store" has no shop
behind it:

- The catalogue is Eris' own game database, lightly edited, with Eris' covers.
- A title becomes downloadable only when the user supplies URLs in `/ps1_external_sources.tsv`. That file
  holds per-disc URLs, optional sizes and boot aliases, and the feature is off by default.
- On top of that it has:
  - a download queue;
  - a `.cue` generated for a bare `.bin`;
  - serial checks;
  - an install to USB or internal storage;
  - a local web page, paired by QR code, for editing sources and uploading archives from a phone or PC;
  - optical-drive import and play (kernel modules, a patched core, LibCrypt);
  - save backups, playtime, and a signed self-update.
- It needs Eris networking and a USB WiFi adapter.

**We take:**
- the in-launcher screen and the background queue;
- the user-owned TSV sources, as a first-class feature (see the decisions below).

**We do not take:**
- the web page and uploads from another device;
- the optical drive;
- internal-storage installs, because the console's own storage is never written.

## The decisions (the owner, 2026-09-24)

1. **One screen, `GuiStore`, reached from the System menu** (L2+R2 -> "Store"). It is a launcher screen like
   Memory Cards or Game Manager, not an App.
2. **Every target.** The store is part of the launcher on `psc`, `rpi` (32-bit), `rpi64`, `pcusb` and
   `win`, not a console feature.
   - The screen and its sources are the same everywhere.
   - What differs per target is data: which catalog it reads (`store/<platform>/`) and which download
     command it runs (`PlatformConfig`).
   - The console needs a network for downloading, and has one only with the AutoBleem kernel's WiFi, as for
     the update. Without it the screen still opens: it browses what is cached and says "Not connected".
3. **Our Apps format is a store item.** An App is shipped as one zip per App per platform
   (`opentyrian-psc-<version>.zip` holding `Apps/opentyrian/...`) and installs to `Apps/<name>/`. Updates
   and removal are the store's too.
4. **TSV sources are not limited.** They are how legal games from private sources get distributed
   (a homebrew publisher, a rights holder, the user's own server).
   - Any number of sources, local files or remote URLs, any host, http or https.
   - Every item kind a source can name, Apps included.
   - No allowlist, no on/off switch, no filtering of what a line points at.
   - We ship no sources but our own catalog, and the screen labels which source an item came from.
5. **A clean implementation - no Project Eris or PSC Store code** (a standing rule). Nothing of theirs is
   used: no source, binary, script, database, TSV sample, artwork or text, not even their patched
   third-party components. Their public descriptions are read to understand *what* the feature does.
   Everything here is designed and written from scratch on our own code (`UpdateService`, `ThemeInstaller`,
   the scanner, the archive readers). Should their TSV columns ever be accepted, that is a mapping written
   from a published description of the format, not from their code.
6. **No installs pushed over the network.** The store only *pulls*: nothing listens on a port, and there
   is no web page, no upload and no pairing. Copying to the stick or the data partition stays the way
   to bring your own files.

## The pieces

Everything SDL-free goes in **autobleem-core**, so it is a core commit first and then a submodule bump here:
- the formats go in `lib_ableem` (the engine, where the JSON library lives);
- the services go in `ab_core`.

Each service ships with its tests in the same commit. The screen lives here, in `ab_evoui`.

### Item kinds

| kind | installs to | from |
|---|---|---|
| `app` | `Apps/<name>/` (`app.ini` + `startup`) | our catalog; a TSV line |
| `ps1` | `Games/<title>/` (cue/bin, chd, pbp, img; the scan does the rest) | our catalog (homebrew and freeware); a TSV line |
| later: `rom:<system>` | `RetroArch/roms/<system>/` (then the ROM scan writes the playlist) | TSV |
| later: `theme` | `Themes/<name>/` (`ThemeInstaller`, as a dropped zip is today) | our catalog; TSV |

The kind is a string in both formats, so a new kind is a new installer and no format change. An older
launcher skips kinds it does not know.

### Our catalog (engine: `StoreCatalog`)

`<repo_url>/store/<platform>/catalog.json` is built by the site's `repo_index.py` (the `index_store`
function to be written). `<platform>` is `Env::platformName()` plus the architecture where it matters:
`psc`, `rpi`, `rpi64`, `pcusb`, `win`. It is the same key set `UpdateService` already derives in
`app.cpp`.

```json
{"schema": 1, "platform": "psc", "date": "2026-10-01",
 "items": [
   {"id": "app/opentyrian", "kind": "app", "title": "OpenTyrian", "version": "2.1.20221123",
    "author": "...", "licence": "GPL-2.0", "description": "...",
    "image": "https://.../store/psc/opentyrian.png",
    "files": [{"name": "opentyrian-psc-2.1.20221123.zip", "size": 4521330, "sha256": "...", "url": "..."}],
    "requires": ["pack/psc-libs"]},
   {"id": "ps1/SLUS-99999", "kind": "ps1", "title": "Some Homebrew", "serial": "SLUS-99999", ...}
 ]}
```

- `files` uses the `UpdateFile` shape (`name`, `url`, `sha256`, `size`) the update catalogs already have,
  so `UpdateCatalog`'s parsing helpers are reused.
- `requires` names something that must be installed first. The console's Apps need the libs pack in
  `Autobleem/lib/apps`, so `pack/psc-libs` maps to the existing `psc/libs/latest.json`.
- A catalog item always carries a `sha256`.

### TSV sources (engine: `StoreSourceTsv`)

A source is a UTF-8 text file with tab-separated values, one file (not one item) per line:

```
# autobleem-store 1                       <- optional first line: the format and its version
# name: Acme Homebrew                     <- optional: the source's display name
kind	title	url	size	sha256	disc	serial	image	version	description
ps1	Some Game	https://acme.example/some-game-d1.chd	412334080		1	SLES-12345	https://acme.example/sg.png
ps1	Some Game	https://acme.example/some-game-d2.chd	398442496		2
app	Acme Player	https://acme.example/acme-player-psc.zip						1.2
```

- **Columns are named by a header line** (the first non-comment line whose fields include `url`), so they
  come in any order and more can be added. Unknown columns are ignored.
- **Without a header**, a line is `title<TAB>url[<TAB>size]`, `kind` is `ps1`, and a bare list of links
  works. This is our own format. It is not designed to be compatible with PSC Store's, and no file of theirs is
  used to test against (decision 5).
- **Lines with the same `kind` + `title` are one item**, with `disc` ordering the files. An item's
  `version`, `serial`, `image` and `description` come from whichever of its lines has them.
- `size` and `sha256` are optional:
  - with `sha256`, a download that does not match is rejected;
  - with only `size`, the size is checked;
  - with neither, the download is trusted as it came.
- A malformed line is skipped and logged (the line number and why); the file still loads.

**Where sources come from:**
- every `*.tsv` in `System/Store/sources/` (drop one on the stick);
- every URL listed in `System/Store/sources.txt`, one per line. The screen adds one with the on-screen
  keyboard, or it can be edited on a PC. A remote TSV is re-fetched when the screen opens, at most every
  few minutes, and the last good copy is kept in the cache.

### Downloading (core: `Downloader`, `StoreService`)

- **`Downloader`** is `UpdateService::downloadFile` taken out and shared. It writes to `<name>.part`,
  checks the size and sha256, then renames; progress is the `.part` file's size as it grows. It runs the
  platform's command through the existing `CommandRunner` seam.
- **Resume**: a PS1 game is 300-700 MB, and a console's USB WiFi nano adapter is slow.
  - `abfetch` gains `--continue`: `Range: bytes=<size of .part>-`, append on `206`, start over on `200`.
  - The platform inis get a `store_download_command` that uses it (`abfetch --continue` on the console,
    `curl -C -` elsewhere). It falls back to `update_download_command` when unset.
- **`StoreService`** (owned by `App`, `app.store()`):
  - merges our catalog and the sources into one list;
  - knows what is installed (`System/Store/installed.json`: id, kind, version, source, where);
  - runs **the queue on one worker thread at the lowest OS priority**, like the scan
    (`System::lowerCurrentThreadPriority()`).
  - `poll()` is called once a frame from `GuiLauncher::loop()` next to `pollUpdates()`, so a download
    keeps going while the user is back in the carousel, and its progress shows in a `NotificationBubble`
    under the scan's.
  - The queue is saved (`System/Store/queue.json`), so a power-off or the console's standby only pauses it.
  - **Paused around a game launch**, the same way the scan's watching is: the emulator gets the CPU and
    the USB bus.
- **Network**: `System::hasDefaultRoute()` on every Linux target (today only the console's update uses it);
  on Windows it is true. With no network the queue waits, and "Not connected" shows on the screen.
- **Space**: before a download, `System::getAvailableSpace()` of the target filesystem is compared with
  the item's size times two (the archive plus what comes out of it). It is refused with a message when
  short. The console's FAT32 file limit (4 GB) is below any PS1 disc.

### Installing (core: `AppInstaller`, `GameInstaller`)

Both unpack into **`System/Store/staging/<id>/`**, on the same filesystem as the target, so the last step
is a rename. Nothing half-written ever appears under `Games/`, where the scan's watcher would pick it up.

- **Archives**: `.zip` (`ZipArchive`), `.7z` (`SevenZipArchive`, built everywhere because CHD support is
  on everywhere) and `.tar.gz` (`TarArchive`). All three already refuse unsafe entry names.
  - The extracted total is checked against the free space before the first byte is written.
  - **RAR is not supported**: unrar's licence is not GPL-compatible, and libarchive would be a large new
    dependency for one format.
  - A plain disc file (`.chd`, `.pbp`, `.bin`, `.cue`, `.img`) is taken as it is.
- **`GameInstaller`**:
  1. Finds the disc files, however deep in the archive.
  2. Writes a `.cue` for a bare `.bin`, using the scanner's existing cue repair.
  3. Merges a multi-disc item into one folder, `Games/<title>/`. The title is `Strings`-sanitised, and a
     clash gets ` (2)`.
  4. Calls `app.scans().requestScan()`. The scan then verifies it, finds the serial, fills the metadata,
     cover and `.m3u`, and the carousel reloads.
- **`AppInstaller`** follows `ThemeInstaller`:
  1. The root of the archive, or its one folder, must contain an `app.ini`, otherwise the install is
     refused.
  2. The new folder replaces `Apps/<name>/`, keeping any `pad.ini` the user edited.
  3. The App's data is under `Home/` (`app_env.sh`), so an update or a removal never loses a save.
  - `app.ini` gains a `Version=` key (read by `GameQueryService::apps()` too). An update is offered when
    the catalog's version differs from the installed one.
- **Removal**: an App's folder, or a game through `GameCatalogService::deleteUsbGame()`, the Game Manager's
  path. The game's `!SaveStates` stays unless the user confirms it, as in Game Manager.

### The screen (`evoui/screens/evoui_store.*`, `GuiStore`)

It is a *full* panel by the UI standard (`CLAUDE.md`, "UI styling standards"), with the detail pane on the
right. The pane shows the cover (the item's `image`, fetched into `System/Store/cache/`, or a local
thumbnail by title), then source, version, size, author, licence and status.

- **Tabs, switched with L1/R1:**
  - **Apps**
  - **Games**
  - **Downloads**: the queue with progress, then what the store installed.
  - **Sources**: our catalog plus each TSV, with its item count, its last refresh and its errors;
    "Add a source URL" (keyboard) and "Remove".
- **Buttons:**
  - Up/Down move a row; L2/R2 page.
  - Cross: Install / Update / Cancel download, by state.
  - Triangle: Remove.
  - Square: sort (name / source / size).
  - Select: find by letter (the launcher's jump).
  - Circle: Back.
- An item already present is marked installed: by `installed.json`, or matched by serial for a game the
  user put on the stick themselves.
- The System menu gets `SystemMenuAction::Store`, "Store" / "Download apps and games". It is present on
  every target (no `#ifdef`), and its description says "Not connected" when there is no route.
- Every string goes through `_()` and into all 16 language files in the same commit.

### The site (autobleem-repo)

- `repo_publish.sh store <platform> <zip|png|json>...` puts files under `store/<platform>/`.
  `repo_index.py` builds `catalog.json` from them plus a per-item `<id>.json` (title, author, licence,
  description, requires). The page does not have to show the store; if it does, it fits into the approved
  look.
- **Apps**: `tools/pack_psc_apps.py` gets a `--per-app` mode that writes one zip per App instead of the
  one dated pack. The pack stays for the PC installer.
- **Games on our catalog** are homebrew or freeware whose licence allows hosting, each with its licence in
  the item. Anything else reaches a user through a TSV source.

## Steps

Each step is one commit, or a core commit plus a submodule bump. Steps 1-4 and 7 are testable on a PC
before any screen exists.

1. **Not done.** Engine: `StoreCatalog` (JSON) and `StoreSourceTsv` (the TSV format above: header,
   header-less, grouping, malformed lines), with tests.
2. **Not done.** `abfetch --continue` (Range/206/200) with tests; `Downloader` extracted from
   `UpdateService` (whose tests keep passing); `store_download_command` in the five platform inis.
3. **Not done.** Core: `GameInstaller` and `AppInstaller` over staging, with tests (zip/7z/tar.gz, a
   nested folder, a bare `.bin`, multi-disc, an unsafe name, no `app.ini`, not enough space, an update
   keeping `pad.ini`).
4. **Not done.** Core: `StoreService`: sources, merge, `installed.json`, the queue worker,
   pause/resume, `poll()`. Tests with a recording `CommandRunner` and a local catalog.
5. **Not done.** The launcher: the System menu item, `GuiStore`, the bubble, pausing around launches,
   the 16 languages. Walked through with `tools/ab_drive.py` on the Windows build against a local catalog
   (`file://` or a `python -m http.server`).
6. **Not done.** The site: `store` in `repo_publish.sh`/`repo_index.py`, `pack_psc_apps.py --per-app`,
   and the first items: OpenTyrian and the other seven console Apps for `psc`.
7. **Not done.** Apps for the other targets: OpenTyrian built for `rpi`, `rpi64`, `pcusb` and `win`
   (the App sources are the tier-2 `screemerpl` repositories, see autobleem-main's `todo.md`). Each is
   packaged by that repository's CI and published to `store/<platform>/`.
8. **Not done.** On hardware (the tester checklist): the console on the AutoBleem kernel's WiFi (an App,
   a two-disc game from a TSV, resume after a standby), a Pi 400, the PC stick, Windows.
9. **Not done.** The manuals (a "Store" section, the TSV format for source owners) and `CLAUDE.md`.

## Open questions

- **Apps on Windows and on the Pi/PC stick.** `app_env.sh` exists on the Linux targets, and "`Apps/` on
  the Pi and the PC stick" is still a *later* item in autobleem-main's `todo.md`. Windows has no
  `app_env` equivalent. Until step 7, a target's catalog may simply have no Apps, and the tab says so.
- **A notice when a source is added?** Decision 4 rules out a gate. A one-line "you are responsible for
  what your sources contain" under the Sources tab is not a gate; the owner's call.
- **The name.** Nothing is sold. "Store" is what users know from Eris; the alternative is something
  like "Get more".
- **Checking a whole source list for dead links** (a HEAD per URL) costs a request per line. For now a
  dead link shows up when it is downloaded.

## Later

- The `rom:<system>` and `theme` kinds.
- Search by keyboard.
- Showing a source's items inside the carousel before they are installed (greyed boxes).
