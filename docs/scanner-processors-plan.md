# Scanner processors (plan)

**Status (2026-09-24):** built - steps 1-8 on `feature/processors` (autobleem-core and the launcher) and in
`proc_unzip`, verified on the Windows dev build end to end (unzip before the scan, the bubble, the sorting
screen). Not yet run on a console or a Pi; step 9 (`tools/proc_check.py`) is still to do. What changed on the
way is marked **Built:** below.

## The decisions (the owner, 2026-09-24)

1. **Processors**, in `System/Processors/`. A folder processor is the request's "preprocessor".
2. **A processor may delete the original**, but only once its output is complete: output is written as
   `<file>.part` and renamed into place, and only then is the original deleted.
3. **Mods and patches run at scan time**, in place. The state file is what makes sure a game is not patched
   twice.
4. **Repositories are named `proc_<name>`** (like `app_<name>` and `ext_<name>`).
5. **Processors always run on the automatic scan**, heavy ones too. If testing on the console shows
   problems, this rule gets revisited.
6. **The first processor is `proc_unzip`**, which is also the code example for authors.
7. **The user puts processors in order**: they run as sequenced chains, and the user can sort them
   (see "Sequences").
8. **Folder processors (preprocessors) are the first thing a scan does**, whatever started it: the user
   (Re-Scan Games), the watcher seeing a changed tree, or the start-up check. Nothing of the scan itself
   (not the loose-file move, not the disc merge, not the hierarchy, not the ROM scan) runs before they
   have finished, because everything after them has to see what they left.
9. **The built-in ECM decoding stays as it is** for now (`EcmDecoder`, the "Decompressing ECM" stage); no
   `proc_unecm` yet.

A **processor** is a small console program, written by anyone, that the scan runs over the games before it
reads them. It lives on the stick in `System/Processors/<name>/`. It can turn a format the launcher does not
read into one it does (a zipped `.bin/.cue`, `.7z`, `.rvz`, a future disc format - what CHD was before we
vendored libchdr), or change a game's data (a translation patch, a texture or audio mod, a region fix). The
launcher does not know or care what it does: it starts it, reads its progress line by line, and shows that
progress in the launcher's `NotificationBubble`, the way the scan's own progress is shown today.

It is the scan's counterpart of an App (`docs/app-format-plan.md`): a separate process, not a plugin, so it
can be written in anything, crash without taking the launcher down, and need no ABI stamp. Extensions
(`docs/extensions-plan.md`) are the in-process kind and are not involved here.

## Two kinds, one program

The request names two kinds; they differ only in *what they are given*, so one executable may be either
or both. Its manifest says which.

| kind | runs on | when | example |
|---|---|---|---|
| **folder processor** | the whole `Games/` tree, or the whole `RetroArch/roms/` tree | once per scan, before anything is read, only when the tree changed | unpack every `.zip` found anywhere, sort loose ROMs into system folders |
| **item processor** | one PS1 game folder, or one ROM file | once per game or ROM it claims, only when that item changed | convert a game's `.rvz` to `.chd`, apply `Game.xdelta` to its `.bin` |

Folder processors run first - the first thing any scan does - then item processors, each in the order the user set (see "Sequences"), so
a chain works: a folder processor unpacks a `.7z`, and an item processor then converts what came out of it.

## Sequences

With several processors installed, the order matters. `unzip -> rvz2chd -> translation patch` works;
`patch -> unzip` patches nothing. So processors run as **sequences** that the user sorts, not in whatever
order they happen to be listed.

**Two sequences**, one per kind of content, because a processor that does both may belong at a different
place in each:

- **PS1**: every processor with `games-folder` or `ps1` among its `Kinds`.
- **ROMs**: every processor with `roms-folder` or `rom` among its `Kinds`.

Each sequence is one ordered list, kept in `System/Processors/sequence.ini`:

```ini
[ps1]
unzip
rvz2chd
-translation_patch        ; a leading '-' = installed but switched off
region_fix

[roms]
unzip
rom_sorter
```

- **The user sorts it** in the System menu's (L2+R2) "Scanner processors" (**Built:** there rather than in
  Options, next to Extensions - Options rows are values, not lists): L1/R1 switch between the PS1 and ROMs
  sequences, Up/Down pick a processor, Square picks it up and Up/Down then move it (Square again puts it
  down), Cross switches it on or off, Triangle "Run again on everything" (forgets its state and requests a
  scan). Each row shows its position, name, version and kinds, and its description is in the footer.
- **A new processor** (one installed but not yet in the file) is added to the end of each sequence it
  belongs to, **switched on**. Among several new ones, the manifest's `Order=` decides; `Order` is only the
  author's suggestion for where it goes, never a reason to reorder what the user already sorted.
- **A removed processor's line** is dropped the next time the file is written. The user's order for the
  rest is kept.
- The file is plain text on purpose, so it can be edited on a PC; a name the launcher does not know is
  ignored.

**How a sequence runs.** Within one sequence the folder processors come first, in the sequence's order,
over the whole tree. Then, **for each item, the whole chain runs on that item** before the next item:

```
for each game folder:                  (or ROM file)
    for each item processor, in sequence order:
        re-read the item (the previous processor may have changed its files)
        candidate? (Match / Systems, then --ismine if the processor answers it)
        already done? (the state file)
        --start
```

Running item by item, not processor by processor, means one game is finished (and then read by the scan)
before the next is started, a long first step does not keep every game waiting for the second, and a
launch that stops the chain leaves most games complete rather than every game half done. The item is read
again between steps because the candidacy depends on what the previous step left: a `.zip` is not a
`.rvz` until `unzip` has run.

A failure breaks **that item's** chain: the processors after it are skipped for that item (their input is
not what they would expect), and the chain is tried again when the item or the failed processor changes.
Other items go on.

## On the stick

```
System/Processors/
    unzip/
        processor.ini
        bin/psc/unzip          <- one binary per platform key, resolved like an App's (AppManifest)
        bin/rpi/unzip
        bin/windows-x86_64/unzip.exe
        lang/                  <- optional, the processor's own translations
    sequence.ini               <- the user's order and on/off, per sequence (see "Sequences")
System/Logs/processors.log     <- every processor's full output, with a header per run
<state>/processors.state       <- what ran on what, see "Not running twice"
```

A full stick is under "Examples" at the end of this document.

### `{key}`, the platform key

`Exec=bin/{key}/unzip` is a pattern, resolved by `AppManifest` exactly as an App's is
(`docs/app-format-plan.md`). The launcher tries its keys (`Env::appPlatformKeys()`) in order, and the first
`bin/<key>/unzip` that exists is run:

| the machine | keys, in order |
|---|---|
| PlayStation Classic | `psc` only (nothing built for a current distribution loads there) |
| Raspberry Pi, 32-bit | `rpi`, `linux-armhf` |
| Raspberry Pi, 64-bit | `rpi64`, `linux-arm64` |
| PC USB stick (i386 Debian) | `pcusb`, `linux-i386` |
| Windows product | `win`, `windows-x86_64` (`.exe` added) |
| dev build | `dev`, then `win`/`windows-x86_64` or `linux-x86_64` |

An author building one generic `linux-arm64` binary covers every 64-bit Pi without knowing our target
names; only the console needs its own `psc` build (the Stretch/gcc-6 toolchain). A processor with no
binary for the machine is left out of the list, as an App is.

### `processor.ini`

```ini
[Processor]
Name=Unzip
Description=Unpacks zipped PS1 games and ROMs
Author=someone
Version=0.2.3
Exec=bin/{key}/unzip        ; or Exec.<key>=..., exactly as app.ini (AppManifest)
Kinds=games-folder,roms-folder,ps1,rom   ; any subset
Match=*.zip;*.7z            ; item kinds: file patterns that make an item a candidate (empty = every item)
Systems=                    ; rom kind: RetroArch database names it cares about (empty = all)
Order=100                   ; where a *new* processor is placed in a sequence: lower first, ties by name
Timeout=600                 ; seconds without a single output line before it is killed (0 = never)
Modifies=true               ; false: it only reads (a checker) - never asked to stop for a launch
```

Why a manifest and not just `--version`: the scan must decide **without starting a process** whether a
processor could be interested in a game. Spawning `--ismine` for 500 games x 4 processors on the console
is minutes of fork/exec on every scan. `Match`/`Systems` filter first; `--ismine` is only asked of the
candidates, and only when the processor says it wants to be asked (it implements it - see below).

`--version` stays as the request described it, for a human (the sorting screen uses `Name=`, else the folder's name).

## The command line

The request's verbs are kept; the positional `{games/roms}` becomes a flag so the order of arguments
cannot be got wrong, and every call carries the protocol version.

```
proc --version
    #Unzip V0.2.3 - Unpacks zipped PS1 games and ROMs

proc --ismine --ps1 <game folder>
proc --ismine --rom <rom file> --system "<database name>"
    exit 0 = mine, 1 = not mine, anything else = error (logged, treated as not mine).
    Output ignored. Optional: a processor whose manifest has no Match is asked for every item; one with
    a Match is asked only for matching items, and may simply exit 0.

proc --start --games <Games dir>
proc --start --roms <roms dir>
proc --start --ps1 <game folder>
proc --start --rom <rom file> --system "<database name>"
    the job; output below.
```

The environment gives it the rest, so the command line never grows:

| variable | value |
|---|---|
| `AB_PROCESSOR_PROTOCOL` | `1` |
| `AB_ROOT`, `AB_GAMES_DIR`, `AB_ROMS_DIR` | the data root and the two trees |
| `AB_RDB_DIR` | RetroArch's `database/rdb/` (for a processor that identifies ROMs) |
| `AB_TMP` | a scratch dir in the machine's own temp, **off the stick**: `/tmp/abproc/<name>/` on Linux (the console's and a Pi's), `%TEMP%\abproc\<name>\` on Windows. Created empty before each run and removed after it. On the console `/tmp` is RAM (tmpfs, shared with the launcher and everything else), so it is for small scratch files only - a disc image goes as `.part` next to its target, never here |
| `AB_PLATFORM`, `AB_PLATFORM_KEYS` | as for Apps |
| `AB_LANGUAGE` | the launcher's language, so a processor can translate its own messages |
| `AB_VERSION` | the launcher's package version |

The working directory is the processor's own folder.

## The output

Line based, on stdout, read as it comes. stderr goes to the log only. The request's examples are valid as
they stand; the grammar only names what they already do and adds three optional lines.

| line | meaning |
|---|---|
| `#Starting - <text>` | first line; the text is what the bubble titles the run with (else `Name`) |
| `0`..`100` | percent of the current stage - the bubble's bar |
| `<n>/<m>` | *optional*: item counter in the current stage ("3/12") - shown next to the bar |
| `#<text>` | a new stage; the text is the bubble's second line, the bar goes back to 0 |
| `#WARN <text>` | *optional*: logged, one notification line, the job goes on |
| `#DONE` | finished, successfully (with or without having done anything) |
| `#ERROR - <text>` | finished, failed; the text is shown and logged |
| anything else | logged, otherwise ignored (so a chatty tool's own output does no harm) |

The result is **both** the last line and the exit code: success is `#DONE` *and* exit 0. A crash, a kill,
exit 0 without `#DONE`, or `#DONE` with a non-zero exit is a failure with a generic message. A processor
that has nothing to do prints `#Starting ...` and `#DONE` - the request's "unsupported folder" case - and
the bubble never shows it (a run that sent no percent, counter or stage within 1 s is not announced).

## The rules a processor must keep (the contract)

These go into the processor author's page (`docs/processors.md`, step 7), because the launcher cannot
enforce most of them:

1. **Atomic, through `.part`.** An output file is written as `<final name>.part` next to its target and
   renamed to its final name only when it is complete. **The original is deleted only after every output
   has been renamed into place**, never before and never while writing. A processor may be killed at any
   moment (a game launch, the power button, the timeout), and the next scan starts it again on the same
   input, so at start-up it removes any `.part` it left behind. The scanner ignores `*.part` files
   everywhere (the hierarchy, the fingerprints, the ROM scan and the digests), so a half-written file is
   never taken for a game. `AB_TMP` is only for small scratch files (a listing, a patch being unpacked):
   on the console it is RAM, and anything big written there takes memory from the launcher.
2. **Idempotent.** Run twice on its own output, it prints `#DONE` and changes nothing.
3. **Stay inside what it was given.** An item processor touches only that game folder or that ROM file
   (and a sibling it creates next to it); a folder processor only that tree. Never `System/`, never the
   console's own storage (the owner's standing rule: nothing on the eMMC, `/data` included).
4. **Say what it is doing, often.** A line at least every `Timeout` seconds, or it is killed.
5. **Deleting the original is the processor's decision and its author's responsibility** (under rule 1).
   The launcher never deletes anything on its behalf.
6. **A patch checks before it patches.** The state file stops a second run on an unchanged game, but a
   patch processor also recognises a game it has already patched (a checksum of the patched file, or a
   marker in Game.ini) and prints `#DONE`, in case the state file was lost.

## Inside the launcher

### Where it runs in the scan

`ScanService::runScan()` (autobleem-core, the worker thread, already at the OS's idle priority - a child
process inherits that on Linux; on Windows it is started with `IDLE_PRIORITY_CLASS`):

```
scan requested (the user, the watcher, the start-up check)
-> the PS1 sequence's folder processors over Games/        (new) - the very first thing
-> the ROMs sequence's folder processors over roms/        (new, only with romScanEnabled())
-> moveLooseGameFilesIntoSubDirs
-> mergeMultiDiscFolders
-> the PS1 sequence's item chain over each game folder     (new)
-> the ROMs sequence's item chain over each ROM file       (new, only with romScanEnabled())
-> GamesHierarchy::getHierarchy
-> scanGamesDirectory ...
-> RetroArchScanner::scan ...
-> fingerprints taken
```

**Built:** the item chains run *before* the tree is read, not between the hierarchy and the scan, so nothing
has to be re-read: the scan sees what the chains left. The candidates are found on disk, not from the
hierarchy - every folder under `Games/` with files of its own (a folder a processor is about to turn into a
game is not a game yet), every file under `roms/<system>/` - and each chain goes round again (up to three
times) when a step changed something, so what a step produced (unzip's `.rvz`, unzip's ROM) gets the rest of
the chain in the same scan.

Both sequences' folder processors run before anything else (decision 8), the ROMs' too, so the one
"preprocessing" phase is over before the scan reads a single folder. `scanning()` is already true from the
first line of `runScan()`, so the Game Manager is refused from the first processor on; `ScanStarted` stays
where it is (it carries the hierarchy's paths), and the processors' `ProcessorProgress` events are what the
bubble shows until then.

The fingerprints are taken after the processors, as they already are after the disc merge, so a
processor's own writes never make the watcher start another scan.

### New code

All SDL-free, in autobleem-core (`ab_core` / `ableem_engine`), each with its tests in the same commit:

- **`ProcessorOutput`** (`engine/processor_output.h`) - the line parser: feed it lines, it keeps
  `title/stage/percent/done/total/finished/succeeded/error/warnings`. Pure, exhaustively tested.
- **`System::runStreaming(exe, args, cwd, env, onLine, shouldStop)`** - the one new process primitive: a
  pipe on the child's stdout (and stderr, separately, to the log), `onLine` per complete line, `shouldStop`
  polled; on stop, SIGTERM, 3 s, SIGKILL to the child's process group (a kill-on-close job object on
  Windows, so what the processor started goes too). **Built:** `runAndWait` is left as it is, beside it.
- **`ProcessorCatalog`** (`core/services/processor_catalog.*`) - lists `System/Processors/*/processor.ini`,
  resolves each through `AppManifest`. **Built:** a processor with no binary for this platform is listed
  (greyed in the screen) and keeps its place in the sequences - the stick may go to another machine - but
  never runs here.
- **`ProcessorSequences`** (`core/services/processor_sequences.*`) - `sequence.ini`: `load(catalog)` gives
  the two ordered lists with each entry's on/off state, new processors placed by `Order`, unknown names
  dropped; `move(sequence, from, to)`, `setEnabled()`, `save()`. What the scan runs and what the sorting
  screen edits. Tested on its own (the placement and merge rules are where the bugs would be).
- **`ProcessorRunner`** (`core/services/processor_runner.*`) - runs one processor on one target through a
  `ProcessorProcess` interface (the real one is `runStreaming`, the tests pass a scripted fake), feeds
  `ProcessorOutput`, writes `processors.log`, enforces `Timeout`.
- **`ProcessorState`** (`core/services/processor_state.*`) - see below.
- **`ScanService`** - the calls above; `WorkerEvent::Kind::ProcessorProgress`/`ProcessorNotice`;
  `ScanUpdate` gains `processorProgressed`, `processor` (a `ProcessorActivity`: title, item, stage, percent,
  counter) and `processorNotices` (warnings and failures). `setProcessorsSuspended()` for a launch (below),
  `setProcessorLanguage()` for `AB_LANGUAGE`. The games fingerprint also counts the processors' `Match`
  files, so a dropped `.zip` starts a scan.

The launcher side, in this repository:

- **`GuiLauncher::applyScanUpdate()`** shows `ScanUpdate::processor` in the scan's `NotificationBubble`:
  first line the run's title (and the game's name for an item processor), second the stage, the bar from
  the percent, the counter at the right. Warnings and failures are a `NotificationLine` each ("Unzip failed
  on Crash Bandicoot - see processors.log").
- **System menu -> "Scanner processors"** (`GuiProcessors`, `evoui/screens/evoui_processors.*`): the two
  sequences, sorted and switched on and off as described under "Sequences"; saved to `sequence.ini` on
  leaving, and a scan is requested if anything changed. Not opened while a scan runs ("Wait for the scan to
  finish"). **Built:** no `ScanStage::Processing` - a silent processor is not announced at all.

### Not running twice

A processor runs on every *scan*, but not on every *item* every scan. `<state>/processors.state` is one
line per (processor, kind, target):

```
<processor folder>\t<processor version>\t<kind>\t<target, relative to the root>\t<input digest>\t<result>
```

The key is the processor, not its position, so reordering a sequence does not make everything run again.
Switching a processor off and on again does not either; "Run again on everything" does.

The **input digest** is what `GamesFingerprint` already computes - file names and sizes, **no mtimes** (no
battery clock on the console) - over the game folder, the ROM file, or the whole tree for a folder
processor. A target is skipped when the processor's version and the digest match a line with `ok` **or**
`failed`: a failing processor is not retried every scan until either the input or the processor changes.
The digest is taken again *after* the run (ok or failed), so the processor's own output does not count as a
change. **Built:** a target is keyed by its path relative to the tree, so a game moved into another folder is
offered again once (a processor is idempotent - it answers #DONE); a "not mine" from `--ismine` is recorded
too, so it is not asked again until the target changes. A PS1 game folder's digest is over its own files only
(its sub-folders are other games), and the launcher's own files (Game.ini, pcsx.cfg, .m3u) and `*.part` are
never part of a digest.

This is what keeps a scan with processors installed as cheap as one without, once everything has been
processed: a digest per candidate item, and no process started at all.

### Game launches and power off

A processor may be minutes into converting a disc when the user starts a game. The scan's own work is
cheap enough to leave running; a processor rewriting gigabytes on the same USB stick the emulator streams
from is not. So `AutoBleem::run()` calls `scans().setProcessorsSuspended(true)` around a game launch and RetroArch: a
running processor with `Modifies=true` is stopped (SIGTERM, up to 3 s, SIGKILL) and none starts; the scan goes
on without them. The run is recorded as *interrupted* (not `failed`), and resuming requests a scan when
something was held back - which is why rule 1 (atomic) matters. On exit and before the console's standby
`scans().stop()` stops them the same way.

## Steps

One commit each; core changes are an autobleem-core commit plus a submodule bump here.

1. **The protocol**: `ProcessorOutput` + tests. *Done* (core `ef8b1ab`; the author's page is proc_unzip's
   README and its `src/main.cpp` header for now).
2. **`System::runStreaming`** on Linux and Windows; tests over `proc_helper`, a scriptable fake processor
   built by the test tree. *Done* (`ef8b1ab`).
3. **`ProcessorCatalog` + `ProcessorSequences` + `ProcessorState`** + tests (`TempDir`, `EnvFixture`):
   new processors placed by `Order`, the user's order kept, a removed one dropped, the state keyed by the
   processor and not its position. *Done* (`ef8b1ab`).
4. **`ProcessorRunner`** + tests over the scripted fake: success, error, silent, timeout, killed, crash,
   `#DONE`-but-exit-1. *Done* (`dfc0024`).
5. **`*.part` ignored by the scanner**: `GamesHierarchy`, `GamesFingerprint`, `RetroArchScanner` and the
   digests, with tests. It comes before the integration so no half-written file ever reaches it. *Done*
   (`4ba7d34`; the hierarchy and the ROM scan needed nothing - no game or ROM extension ends in `.part`).
6. **`ScanService` integration**: PS1 side first (folder processors, then the item chain), then ROMs. The
   real-scan integration test runs the step-2 helper as two chained processors and checks the order, the
   re-read between steps, a failure stopping only that item's chain, the database afterwards, and that a
   second scan starts nothing. *Done* (`4954c27`).
7. **The launcher**: the bubble, notification lines, `GuiProcessors` (sorting, on/off), stop-on-launch; all 16
   language files. *Done*.
8. **`proc_unzip`**, its own repository (`autobleem2/proc_unzip`), written to be read as the example: `.zip`
   (and `.7z` if a small enough decoder can be vendored) of a PS1 game or a ROM, unpacked through `.part`
   files next to the archive, the archive deleted only after the last rename. Built for every platform key
   with miniz. `--ismine` answers from the archive's listing (a `.zip` with a `.cue`/`.bin`/`.chd`/`.pbp`
   inside for PS1; any single ROM for the ROM kind). It proves the whole chain on the console. *Done* in
   `E:\Programming\_work-processors\proc_unzip` (local, `.zip` only, Windows built and self-tested; the
   GitHub repository and the console/Pi builds are still to do).
9. **`tools/proc_check.py`**: runs a processor against a scratch copy of a folder and validates its output
   and the contract rules it can see (atomic temp names left behind, a second run changing nothing,
   files touched outside the target) - what an author runs before publishing. Later a Store category.

## Later, not in this plan

- **Launch-time processors** (`--prepare`/`--cleanup` around a launch): a format decoded next to the game just
  for the session, or a mod applied without touching the original. Same protocol, a third kind.
- A processor that only *reports* (a checker: bad dumps, missing BIOS) - already possible with
  `Modifies=false`; its warnings are the output.
- Signatures / a trusted list, once processors come from the Store.

## To watch in testing

- **Heavy jobs on the console** (decision 5): processors always run on the automatic scan. Things to look
  at on the PSC: how long a CHD conversion takes, whether the launcher's frame rate holds while one runs
  (the child runs at idle priority, but the USB stick is shared), and whether stopping it for every launch
  means it never finishes. If any of these is a problem, the fix is a manifest flag that makes a
  processor run only when the user asks.

## Decided later

- **The built-in ECM decoding** stays (decision 9). If a `proc_unecm` is ever made, the built-in decoder and
  it would both want the same `.ecm`, and one of them has to go then.

## Examples

Two processors, run against the protocol above: **unzip** (`proc_unzip`, the first real one, plan step 8)
and **unecm** - today's `EcmDecoder` as a program. unzip is both a folder processor and a ROM item
processor; unecm is a PS1 item processor, so the state file can skip every game without an `.ecm` without
starting anything.

### The stick

```
/media/                                          (the stick; the data folder on Windows)
├── Games/
│   ├── Crash Bandicoot (USA).zip                <- unzip turns this into a folder
│   ├── Tekken 3 (USA)/
│   │   ├── Tekken 3 (USA).cue
│   │   └── Tekken 3 (USA).bin.ecm               <- unecm turns this into the .bin
│   └── !SaveStates/ ...
├── RetroArch/roms/
│   └── Sega - Mega Drive - Genesis/
│       └── Sonic (World).zip
└── System/
    ├── Processors/
    │   ├── sequence.ini                         <- the user's order and on/off
    │   ├── unzip/
    │   │   ├── processor.ini                    <- Exec=bin/{key}/unzip
    │   │   ├── README.txt
    │   │   ├── lang/                            <- optional: Polski.txt, Deutsch.txt ...
    │   │   └── bin/
    │   │       ├── psc/unzip                    <- the console (Stretch gcc-6, armhf)
    │   │       ├── linux-armhf/unzip            <- any 32-bit Pi
    │   │       ├── linux-arm64/unzip            <- any 64-bit Pi
    │   │       ├── linux-i386/unzip             <- the PC stick
    │   │       └── windows-x86_64/unzip.exe     <- Windows
    │   └── unecm/
    │       ├── processor.ini                    <- Exec=bin/{key}/unecm
    │       └── bin/
    │           ├── psc/unecm
    │           ├── linux-armhf/unecm
    │           ├── linux-arm64/unecm
    │           ├── linux-i386/unecm
    │           └── windows-x86_64/unecm.exe
    ├── Logs/processors.log
    └── State/processors.state                   <- what ran on what (the state dir)

/tmp/abproc/unecm/                               (AB_TMP: RAM, only while unecm runs)
```

`sequence.ini` for this stick - unzip first, because a zip may hold an `.ecm`; unecm is only in the PS1
chain because its `Kinds` is `ps1`:

```ini
[ps1]
unzip
unecm

[roms]
unzip
```

An author who cares about some machines only ships those `bin/` folders: a processor with only `bin/psc/`
shows up on the console and nowhere else.

### unzip

```ini
[Processor]
Name=Unzip
Description=Unpacks zipped PS1 games and ROMs
Version=1.0.0
Exec=bin/{key}/unzip
Kinds=games-folder,rom
Match=*.zip
Systems=                     ; all - it answers "not mine" for arcade sets itself
Order=10
Timeout=120
Modifies=true
```

```
$ unzip --version
#Unzip V1.0.0 - Unpacks zipped PS1 games and ROMs
```

As a preprocessor over `Games/`, the first thing a scan does (the environment is the launcher's):

```
AB_PROCESSOR_PROTOCOL=1  AB_ROOT=/media  AB_GAMES_DIR=/media/Games
AB_TMP=/tmp/abproc/unzip  AB_LANGUAGE=English  AB_PLATFORM=psc

$ unzip --start --games /media/Games
#Starting - Unzip V1.0.0
#Looking for archives
#Unpacking Crash Bandicoot (USA).zip
1/2
0
14
38
61
87
100
#Unpacking Spyro the Dragon (Europe).zip
2/2
0
22
...
100
#DONE
$ echo $?
0
```

```
Games/Crash Bandicoot (USA).zip               -> Games/Crash Bandicoot (USA)/Crash Bandicoot (USA).cue + .bin
Games/RPG/Spyro/Spyro the Dragon (Europe).zip -> Games/RPG/Spyro/Spyro the Dragon (Europe).cue + .bin
```

A zip loose in `Games/` gets a folder named after it; a zip already in a game folder is unpacked where it
is. While unpacking, the stick holds `Crash Bandicoot (USA).bin.part`; it is renamed when complete, and
the `.zip` is deleted after the last rename.

Nothing to do - the bubble never shows this run:

```
$ unzip --start --games /media/Games
#Starting - Unzip V1.0.0
#DONE
```

An error - the zip is kept, the `.part` files are removed:

```
$ unzip --start --games /media/Games
#Starting - Unzip V1.0.0
#Unpacking Final Fantasy VII (USA).zip
1/1
0
31
#ERROR - Not enough space: Final Fantasy VII (USA).zip needs 2.1 GB, 0.8 GB free
$ echo $?
1
```

As an item processor on a ROM:

```
$ unzip --ismine --rom "/media/RetroArch/roms/Sega - Mega Drive - Genesis/Sonic (World).zip" \
                 --system "Sega - Mega Drive - Genesis"
$ echo $?
0                                   # one ROM inside: mine

$ unzip --ismine --rom "/media/RetroArch/roms/MAME/pacman.zip" --system "MAME"
$ echo $?
1                                   # arcade sets stay zipped: not mine

$ unzip --start --rom "/media/RetroArch/roms/Sega - Mega Drive - Genesis/Sonic (World).zip" \
                --system "Sega - Mega Drive - Genesis"
#Starting - Unzip V1.0.0
#Unpacking Sonic (World).zip
0
100
#DONE
```

### unecm

```ini
[Processor]
Name=UnECM
Description=Decodes .ecm disc images to .bin
Version=1.0.0
Exec=bin/{key}/unecm
Kinds=ps1
Match=*.ecm
Order=20                     ; after unzip: a zip may hold an .ecm
Timeout=300
Modifies=true
```

```
$ unecm --version
#UnECM V1.0.0 - Decodes .ecm disc images to .bin

$ unecm --ismine --ps1 "/media/Games/Tekken 3 (USA)"
$ echo $?
0                                   # has Tekken 3 (USA).bin.ecm

$ unecm --start --ps1 "/media/Games/Tekken 3 (USA)"
#Starting - UnECM V1.0.0
#Decoding Tekken 3 (USA).bin.ecm
0
7
19
33
...
100
#Checking Tekken 3 (USA).bin
100
#DONE
```

`Tekken 3 (USA).bin.ecm` -> `Tekken 3 (USA).bin.part` -> `Tekken 3 (USA).bin`, and only then is the `.ecm`
deleted. The `.cue` is not touched.

A multi-disc game uses the counter:

```
$ unecm --start --ps1 "/media/Games/Metal Gear Solid (USA)"
#Starting - UnECM V1.0.0
#Decoding Metal Gear Solid (USA) (Disc 1).bin.ecm
1/2
0
...
100
#Decoding Metal Gear Solid (USA) (Disc 2).bin.ecm
2/2
0
...
100
#DONE
```

A damaged file:

```
#Starting - UnECM V1.0.0
#Decoding Tekken 3 (USA).bin.ecm
0
7
#ERROR - Tekken 3 (USA).bin.ecm is damaged (bad checksum at sector 81234), the original was kept
$ echo $?
1
```

The state file records `failed`: not retried until the `.ecm` or unecm changes, and the rest of this
game's chain is skipped - a later patch processor never sees a half-decoded game.

Stopped by a game launch (SIGTERM from the launcher):

```
#Starting - UnECM V1.0.0
#Decoding Tekken 3 (USA).bin.ecm
0
7
43
```

No `#DONE`, so the run is recorded as *interrupted*. On the next scan unecm first deletes its own leftover
`Tekken 3 (USA).bin.part`, then starts again.

### What the launcher shows

The bubble during the unecm run above:

```
┌────────────────────────────────────────┐
│ UnECM V1.0.0 - Tekken 3 (USA)      1/2 │
│ Decoding Tekken 3 (USA).bin.ecm        │
│ ████████████░░░░░░░░░░░░░░░░░░░   43 % │
└────────────────────────────────────────┘
```

The title is `#Starting`'s text plus the game's name, the second line the last `#` stage, the bar the last
number, the counter the last `n/m`. An error is a notification line: "UnECM failed on Tekken 3 (USA) - see
processors.log".

`System/Logs/processors.log`, a header per run:

```
=== 2026-09-24 21:14:03  unecm 1.0.0  --start --ps1 "/media/Games/Tekken 3 (USA)"
... every line; stderr prefixed "! " ...
=== exit 0, #DONE, 41.2 s
```
