# The AutoBleem Store (plan)

**Status (2026-09-24):**
- **Working on the Windows dev build**, from the launcher's Extensions list, against a local test site: the
  sources are read, an App and a game are installed, and the launcher rescans.
- **In its repository**, `autobleem2/ext_store` (2026-09-24), built with the launcher through
  `AB_EXTENSION_DIRS` until the SDK package exists.
- **Still open**: the site's catalog and per-App packages (step 6), Apps for the other targets (step 7),
  hardware (step 8) and the manuals (step 9).

The Store downloads and installs Apps and games without
pulling the stick. It is the **first AutoBleem extension** (`docs/extensions-plan.md`): a program built on
the AutoBleem SDK, loaded into the launcher as a plugin. It ships with every platform's installer (since
2026-09-25; it was a separate download, unpacked by hand) and runs from the System menu's Extensions list.

It came out of a look at Project Eris' "PSC Store" (`github.com/hampter-mods/pscstore-release`,
release-only, closed source). What we take from it is the idea, not the code: a controller-driven library
you can add to without pulling the stick, and a tab-separated list of download URLs that the user owns.

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
- a controller-driven screen with a download queue;
- the user-owned TSV sources, as a first-class feature (see the decisions below).

**We do not take:**
- the web page and uploads from another device;
- the optical drive;
- internal-storage installs, because the console's own storage is never written.

## The decisions (the owner, 2026-09-24)

1. **The Store is an extension, "AutoBleem Store"** (the name stays, though nothing is sold):
   - it lives in `Extensions/store/` and is started from System menu -> Extensions;
   - it is a plugin with `Background=true`, so its downloads go on while the carousel is showing;
   - it is **a separate download**, never bundled with a release, and the user installs it by hand like
     every extension - **changed on 2026-09-25**: every platform's installer ships it, an install or update
     puts in the version it brings;
   - it lives in its own repository (`autobleem2/ext_store` - an extension's repository is named `ext_<name>` - with autobleem-core as a
     submodule);
   - it never offers or installs extensions.
2. **Every target.** It is built for `psc`, `rpi` (32-bit), `rpi64`, `pcusb` and `win`.
   - The screen and its sources are the same everywhere.
   - What differs per target is data: which catalog it reads (`store/<platform>/`) and which download
     command it runs (`PlatformConfig`).
   - The console needs a network for downloading, and has one only with the AutoBleem kernel's WiFi, as for
     the update. The Store's `extension.ini` says `Network=required`, so **the launcher refuses to open it
     offline**: its row in the Extensions list is greyed with "Needs a network connection" (the extensions
     plan). On a console that means a stock kernel, or the AutoBleem kernel with its WiFi down.
3. **Our Apps format is a store item.** The App folder is multi-platform (`docs/app-format-plan.md`):
   one `Apps/<name>/` holds every platform's binary in `bin/<key>/`, and `app.ini` says which is which.
   The Store downloads the one-platform package for this machine (`opentyrian-psc-<version>.zip`) and
   *merges* it into the folder, never deleting another platform's binaries. Updates and removal are the
   store's too.
4. **TSV sources are not limited.** They are how legal games from private sources get distributed
   (a homebrew publisher, a rights holder, the user's own server).
   - Any number of sources, local files or remote URLs, any host, http or https.
   - Every item kind a source can name, Apps included.
   - No allowlist, no on/off switch, no filtering of what a line points at.
   - We ship no sources but our own catalog, and the screen labels which source an item came from.
   - The Sources tab carries one static line, "You are responsible for what your sources contain". It is
     a notice, not a gate: nothing has to be confirmed.
   - Sources are not checked for dead links in advance (that would cost a request per line). A dead link
     shows up when its item is downloaded.
5. **A clean implementation - no Project Eris or PSC Store code** (a standing rule, autobleem-main's
   `decisions.md`). Nothing of theirs is used: no source, binary, script, database, TSV sample, artwork or
   text, not even their patched third-party components. Their public descriptions are read to understand
   *what* the feature does. Everything here is designed and written from scratch on our own code
   (`UpdateService`, `ThemeInstaller`, the scanner, the archive readers).
6. **No installs pushed over the network.** The store only *pulls*: nothing listens on a port, and there
   is no web page, no upload and no pairing. Copying to the stick or the data partition stays the way
   to bring your own files.

## The pieces

- **In autobleem-core** (the SDK, usable by the launcher too), each with its tests:
  - `Downloader` and `abfetch --continue`;
  - `AppInstaller` and `GameInstaller`;
  - a generic detail pane in `ab_classic`.
- **In ext_store**:
  - the formats, `StoreCatalog` and `StoreSourceTsv`;
  - `StoreService`;
  - the screen, `GuiStore`;
  - the extension's packaging.

### Item kinds

| kind | installs to | from |
|---|---|---|
| `app` | `Apps/<name>/` (`app.ini` + `startup`) | our catalog; a TSV line |
| `ps1` | `Games/<title>/` (cue/bin, chd, pbp, img; the scan does the rest) | our catalog (homebrew and freeware); a TSV line |
| later: `rom:<system>` | `RetroArch/roms/<system>/` (then the ROM scan writes the playlist) | TSV |
| later: `theme` | `Themes/<name>/` (`ThemeInstaller`, as a dropped zip is today) | our catalog; TSV |

The kind is a string in both formats, so a new kind is a new installer and no format change. An older
Store skips kinds it does not know.

### Our catalog (`StoreCatalog`)

`<repo_url>/store/<platform>/catalog.json` is built by the site's `repo_index.py` (the `index_store`
function to be written). `<platform>` is `Env::platformName()` plus the architecture where it matters:
`psc`, `rpi`, `rpi64`, `pcusb`, `win`. It is the same key set `UpdateService` already derives in the
launcher's `app.cpp`. `repo_url` comes from the platform ini.

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

### TSV sources (`StoreSourceTsv`)

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
  works. This is our own format. It is not designed to be compatible with PSC Store's, and no file of
  theirs is used to test against (decision 5).
- **Lines with the same `kind` + `title` are one item**, with `disc` ordering the files. An item's
  `version`, `serial`, `image` and `description` come from whichever of its lines has them.
- `size` and `sha256` are optional:
  - with `sha256`, a download that does not match is rejected;
  - with only `size`, the size is checked;
  - with neither, the download is trusted as it came.
- A malformed line is skipped and logged (the line number and why); the file still loads.
- **The NoPayStation layout is read too** (the owner, 2026-09-24: it is well known). Its column names map
  onto ours: `PKG direct link` is the url, `File Size` the size, `Title ID` the serial, and `Name` the title
  (only in a header without a `title` column; in ours `name` is the file's name).
  - Lines of the same title with different serials (one per regional release) are separate items, their
    ids `<kind>/<title>/<serial>`.
  - Their links are PSN packages (`.pkg`), which are not disc images. The Store lists such items as **Not
    installable** and never queues or fetches them.
  - Implemented from the layout's column names alone: no list of theirs was downloaded to build or test it.

**Where sources come from:**
- every `*.tsv` in `System/Store/sources/` (drop one on the stick);
- every URL listed in `System/Store/sources.txt`, one per line. The Store adds one with the on-screen
  keyboard, or it can be edited on a PC. A remote TSV is re-fetched when the Store opens, at most every
  few minutes, and the last good copy is kept in the cache.

### Downloading (`Downloader` in core, `StoreService`)

- **`Downloader`** is `UpdateService::downloadFile` taken out into core and shared. It writes to
  `<name>.part`, checks the size and sha256, then renames; progress is the `.part` file's size as it grows.
  It runs the platform's command through the existing `CommandRunner` seam.
- **Resume**: a PS1 game is 300-700 MB, and a console's USB WiFi nano adapter is slow.
  - `abfetch` gains `--continue`: `Range: bytes=<size of .part>-`, append on `206`, start over on `200`.
  - The platform inis get a `store_download_command` that uses it (`abfetch --continue` on the console,
    `curl -C -` elsewhere). It falls back to `update_download_command` when unset.
  - `%r` in these commands is the launcher's resources folder, where `abfetch` ships. It is the same
    `Env::getWorkingPath()` the update uses, because the plugin runs inside the launcher.
- **`StoreService`** (owned by the Store's `Extension` object, which lives as long as the launcher does):
  - merges our catalog and the sources into one list;
  - knows what is installed (`System/Store/installed.json`: id, kind, version, source, where);
  - runs **the queue on one worker thread at the lowest OS priority**, like the launcher's scan
    (`System::lowerCurrentThreadPriority()`).
  - **Downloads go on after the Store's screen is closed.** The plugin's `poll()`, called once a frame from
    the launcher, hands progress to `ExtensionHost::notify()`, which is a `NotificationBubble` under the
    scan's. Finished installs are applied there as well.
  - **Paused around a game launch** through `suspend()`/`resume()`, so the emulator gets the CPU and the
    USB bus.
  - The queue is saved (`System/Store/queue.json`), so a power-off or the console's standby only pauses
    it. After a restart, a queue with work left resumes on its own.
- **Network**: `ExtensionHost::networkUp()` (`System::hasDefaultRoute()` on every Linux target; always
  true on Windows).
  - The Store cannot be opened offline (`Network=required`).
  - Its background queue is still loaded at start-up. It waits while there is no route and resumes when
    one appears, so a WiFi drop in the middle of a download is only a pause.
  - If the network goes while the Store is open, the screen says "Not connected" and the queue waits.
- **Space**: before a download, `System::getAvailableSpace()` of the target filesystem is compared with
  the item's size times two (the archive plus what comes out of it). It is refused with a message when
  short. The console's FAT32 file limit (4 GB) is below any PS1 disc.

### Installing (`AppInstaller`, `GameInstaller` in core)

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
  4. The Store calls **`ExtensionHost::requestRescan()`**. The launcher's scan verifies the game, finds
     the serial, fills the metadata, cover and `.m3u`, and the carousel reloads.
- **`AppInstaller`** follows `ThemeInstaller`:
  1. The root of the archive, or its one folder, must contain an `app.ini` that `AppManifest` resolves
     for this machine, otherwise the install is refused.
  2. The package is laid over `Apps/<name>/`: the shared files and the ini are replaced, other platforms'
     `bin/<key>/` and `lib/<key>/` are kept (unless the version changed - see the app format plan), and a
     `pad.ini` the user edited is kept.
  3. The App's data is under `Home/` (`app_env.sh`), so an update or a removal never loses a save.
  4. The Store calls **`ExtensionHost::reloadApps()`**.
  - `app.ini` gains a `Version=` key (read by the launcher's `GameQueryService::apps()` too). An update is
    offered when the catalog's version differs from the installed one.
- **Removal**: an App's folder, or a game's folder (followed by `requestRescan()`). The game's `!SaveStates` stays unless
  the user confirms it, as in the launcher's Game Manager.
- Both installers live in core so the launcher can use them too, for example for an archive dropped into
  `Games/` or `Apps/` (not planned).

### The screen (`GuiStore`, in the extension)

It is a *full* panel by the UI standard (the launcher's `CLAUDE.md`, "UI styling standards"), drawn with
the SDK's components in the user's theme. The detail pane on the right shows the cover (the item's
`image`, fetched into `System/Store/cache/`, or a local thumbnail by title), then source, version, size,
author, licence and status. The launcher's `GameDetailPane` is in `ab_ui`, outside the SDK, so a generic
pane (a picture, then label/value facts) moves down into `ab_classic` for this, and the launcher's pane is
rebuilt on it.

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
  - Select: find by letter.
  - Circle: Back. Downloads go on in the background.
- An item already present is marked installed: by `installed.json`, or matched by serial for a game the
  user put on the stick themselves.
- Every string goes through `_()`, in the Store's own `lang/` files, all 16 languages in the same commit.

### The site (autobleem-repo)

- **The Store itself** is published like the launcher's components: `ext_store-<platform>-<v>.zip`,
  laid out as `Extensions/store/...`, from its repository's CI.
- **The catalog**: `repo_publish.sh store <platform> <zip|png|json>...` puts files under
  `store/<platform>/`. `repo_index.py` builds `catalog.json` from them plus a per-item `<id>.json` (title,
  author, licence, description, requires). The page does not have to show the store; if it does, it fits
  into the approved look.
- **Apps**: `tools/pack_psc_apps.py` gets a `--per-app` mode that writes one zip per App instead of the
  one dated pack. The pack stays for the PC installer.
- **Games on our catalog** are homebrew or freeware whose licence allows hosting, each with its licence in
  the item. Anything else reaches a user through a TSV source.

## Steps

The multi-platform folder format (`docs/app-format-plan.md`, steps 1-2) and the extension mechanism
(`docs/extensions-plan.md`, steps 1-4) come first. Each step here is one commit
(or a core commit plus a submodule bump) with its tests. Steps 1-4 are testable on a PC before any screen
exists.

1. **Done** (2026-09-24; the repository's CI on 2026-09-25 - ext_store's `ci/build.sh` and `build.yml`: the
   Store for psc/rpi/rpi64/pcusb/win and abstored for Linux and Windows, a nightly release, and the `site` job
   that puts them on the Store page).
   - The formats live in the engine, because the JSON library is private to lib_ableem:
     `ableem::StoreCatalog` and `StoreSourceTsv` (`engine/store_catalog.*`), tested in
     `tests/core/test_store_catalog.cpp`. The JSON key `requires` is the field `dependsOn` (a C++20 keyword).
   - The repository has no autobleem-core submodule yet: it is built by the launcher through
     `AB_EXTENSION_DIRS` until the SDK package (extensions plan, step 5) lets it build on its own.
2. **Done** (2026-09-24).
   - `Downloader` (`core/services/downloader.*`) was extracted from `UpdateService`, whose tests pass
     unchanged. It resumes a `.part` with the resume command, and drops a `.part` that a resume got
     nowhere with.
   - `abfetch --continue`/`-C`: Range requests; a 206 is appended, a 200 starts over, a 416 counts as
     complete, and a stopped download keeps its bytes. Tested against the loopback server.
   - `store_download_command` in the five platform inis (`abfetch --continue` on the console,
     `curl -C -` elsewhere), and `Env::storeDownloadCommand()` with `%r` resolved and the update's command
     as the fallback.
3. **Done** (2026-09-24). `core/services/content_installer.*`:
   - `ArchiveUnpacker` handles zip, tar.gz and 7z.
   - `AppInstaller`: the `Apps/<name>/` layout, a root `app.ini` or the archive's one folder; refused when
     the App cannot run here; another platform's package merges; a new `Version` drops every platform's
     binaries; the user's `pad.ini` is kept.
   - `GameInstaller`: archives and plain files, the disc files gathered from any depth, a `.cue` for a
     bare `.bin`, a FAT-safe folder name with " (2)", and refusal when there is no disc image.
   - Both go through staging, with a free-space check.
   - Tested in `tests/core/test_content_installer.cpp`.
4. **Done** (2026-09-24). `StoreService` (ext_store `src/store_service.*`):
   - the sources, cached;
   - `installed.tsv` (not JSON: the JSON library is not in the SDK surface);
   - `queue.txt`;
   - one low-priority worker thread. Its downloads are stopped through core's new cancellable
     `System::runShellCommand(line, cancelled)`: a process group on Linux, a job object on Windows. A pause
     keeps the bytes and the place in the queue, and a cancel drops them.
   - Tested against a fake site in `tests/test_store_service.cpp`: sources, install, update, remove, a
     two-disc game, a failed checksum, pause and resume, stop and restart, cancel, offline.
   - The state lives in the extension's own state directory, `System/Extensions/store/` (`sources/`,
     `sources.txt`, `cache/`, `downloads/`, `staging/`), not the `System/Store/` this plan first named.
5. **Done on Windows** (2026-09-24).
   - `GuiStore`: Apps / Games / Downloads / Sources, the list and a detail pane of its own. The generic
     `ab_classic` pane is still to do; it was not needed.
   - 29 strings of its own in 16 languages, in its `lang/`, which the host loads over the launcher's.
   - Walked through with `tools/ab_drive.py` against a local site (`AB_STORE_CATALOG`,
     `python -m http.server`): an App installed from the catalog, a game from a TSV source, the rescan
     after it.
6. **Done** (2026-09-24/25): `store` in `repo_publish.sh`/`repo_index.py` and the Store page
   (`store/index.html`); the Store's own packages there, per system, from its CI (`extensions/store/`), with
   abstored and LAN Share in a LAN server tab (the LAN server work: autobleem-pc-tools
   `docs/lan-share-plan.md`); `pack_psc_apps.py --per-app` (autobleem-appliance) and the first items - the
   eight console Apps of apps-psc-20260920 in `store/psc/`, next to Terminal. The step as first written:
   The site: the Store's packages, `store` in `repo_publish.sh`/`repo_index.py`,
   `pack_psc_apps.py --per-app`, and the first items: OpenTyrian and the other seven console Apps for
   `psc`.
7. **Not done.** Apps for the other targets: OpenTyrian built for `rpi`, `rpi64`, `pcusb` and `win`
   (the App sources are the tier-2 `screemerpl` repositories, see autobleem-main's `todo.md`). Each is
   packaged by that repository's CI and published to `store/<platform>/`.
8. **A TODO for the testers** (the owner, 2026-09-25: autobleem-main's `docs/todo.md`, "Testers"). On hardware
   (the tester checklist): the console on the AutoBleem kernel's WiFi (an App,
   a two-disc game from a TSV, a resume after a standby), a Pi 400, the PC stick, Windows.
9. **Mostly done**: the manuals' Store section (3.12) and "On the PC" chapter (5.2 LAN Share), abstored's
   README and INSTALL-linux.md, LAN Share's CLAUDE.md. Left: the TSV format written up for source owners.
   The step as first written: the manuals (a "Store" section, the TSV format for source owners) and the
   CLAUDE.md files.

## Open questions

- **Apps beyond the console** depend on `docs/app-format-plan.md` (the multi-platform folder, the
  Windows direct launch). Until it and step 7 are done, a target's catalog may simply have no Apps, and the
  tab says so.
None left: the notice, the name and dead-link checking were decided on 2026-09-24 (decisions 1 and 4).

## Later

- The `rom:<system>` and `theme` kinds.
- Search by keyboard.
