# A quiet stick - plan (2026-09-24)

**Goal:** the stick (the console's USB stick, the Pi's / PC stick's data partition) is written only when
the *user's* state changes: a save, a memory card, a resume slot the player kept, a setting the player
changed, a game added or removed. Logs, intermediate files and rewrites of unchanged data go to RAM or
nowhere. A tester can still get full logs, and a crash still leaves evidence on the stick.

Why: every write is flash wear, a window for FAT corruption when the power goes (the PSC has no clean
power-off path besides our standby), and on a stock console the stick is FAT32 with a dirty flag we
already fight (`checkstick.sh`). Idle, boot and a no-change scan should write nothing at all.

This plan is based on a full audit of the rc scripts (console and Linux), the launcher and autobleem-core,
pcsx-ab, pcsx-abnxt and RetroArch's shipped config (2026-09-24). File:line references are as of that day.

## Status (2026-09-24)

Branch `feature/quiet-stick` in autobleem (launcher), autobleem-core, pcsx-abnxt and autobleem-appliance,
pushed, **not merged**. CLAUDE.md's "The quiet stick" section describes the result.

| Step | State |
|---|---|
| 0 guard tests, `tools/stick_writes.sh` | done; `test_quiet_stick` green. **Device numbers not taken yet** |
| 1.1-1.6 logs in RAM, keep switch, crash capture, Save logs | done (launcher, core, both payload trees) |
| 1.7 pcsx-abnxt line-buffered stdout | done (pcsx-abnxt) |
| 1.8 Linux targets: RuntimeDirectory, journald volatile | done (autobleem-appliance) |
| 2.1-2.8 write only what changed (ini, cfg, scan, db, history, rc) | done |
| 2.9 refused games in the Game Manager | done |
| 3.1 selection hand-over in RAM, written on leaving only | done |
| 3.2 exit resume point in RAM (`AB_EXIT_DIR`) | done for pcsx-abnxt; **classic pcsx-ab: not yet** (no local build) |
| 3.3 memory-card set in place (`AB_MEMCARD_DIR`), slot read in place (`AB_LOAD_STATE`) | done for pcsx-abnxt; classic: not yet |
| 3.4-3.5 RetroArch via `--appendconfig`, Persist RetroArch config | done |
| 3.6 extensions' crash guard in RAM | done |
| 3.7 App cache in RAM | done (with 1.5) |
| 4 CLAUDE.md, manuals (en/pl) | done; manual PDFs not rebuilt/published |

Found and fixed on the way: a broken cue the scan cannot heal was rewritten at every scan; the first scan
wrote `Game.ini` values the second one normalised (Favorite/Lightgun/Play_using_ra/publisher); a
selection left over from a game hid a later crash from `selection.sh`; RetroArch changes the player made
during a game were thrown away by the old `.bak` restore.

Still to do: the classic pcsx-ab's share of 3.2/3.3 (ab_exit_dir etc. as in pcsx-abnxt, on the build
server); `stick_writes.sh` on the console and a Pi, the numbers here; verify on RetroArch 1.22 that
`--appendconfig` + the restore behave as `configuration.c` says; verify the exit dir on a console with a
real game (the Windows build of pcsx-abnxt compiles it, but could not be exercised without a BIOS).

## What is written today

The worst offenders by volume and frequency, measured by reading the code (not by a tracer - Phase 0 does
that):

| When | What | Where | Kind |
|---|---|---|---|
| every log line | plog -> `System/Logs/autobleem.log` (1 MB x 3, unbuffered `write()` per record) **and** the console appender -> `AB_out.txt` (run.sh / session.sh redirect, `unitbuf`) | `log.h:55-65`, `main.cpp:66,108`, `src/resources/run.sh:3` | log, twice |
| every boot | `start.sh:1` `sed -i` rewrites `rc/boot.sh`; `backup.sh:18` copies `AutoBleem.rdb` over itself; `checkstick.sh` `.session` + `sync` | rc | redundant |
| every launcher start | `config.ini` saved unconditionally (`config.cpp:30`, again at `:111` if a default was set); `backup_internal.sh` `sync` | core | redundant |
| every scan | every game's `Game.ini` (`game_scanner.cpp:494`), `UPDATE GAME` + a `replaceDiscs` transaction per game (`scan_service.cpp:407-409`), all `SUBDIR_*` rows deleted and re-inserted, every multi-disc `.m3u` deleted and rewritten, `AutoBleem.lpl`, `gamelist.xml`, `autobleem.list`, both fingerprints, and four diagnostic dumps (`gameHierarchy_*`, `duplicateGames.txt`, `gamesThatFailedVerifyCheck.txt`) - **all of it even when nothing changed** | core / engine | redundant + log |
| every PS1 launch | `launch.log` (appended, never trimmed), `pcsx.log`, `pcsx.cfg` copied into `!SaveStates` (`launch.sh:18`), `autobleem_cfg.sh` twice (`autobleem.cpp:315`, `launch.cpp:330`), the history ranking (up to 101 `UPDATE`s in two DBs, `game_catalog.cpp:42-70`), `last_played`; a custom memcard set is copied ~4 times (`memcard_manager.cpp`) | rc / core | log + redundant |
| every PS1 exit | both emulators write the exit state (`.000`, ~1 MB) + a PNG + `filename.txt` (fsync) into `!SaveStates` - the launcher keeps it only if the player picks a slot, and then by copy + delete (`resume_point.cpp:222-247`) | pcsx-ab `main.c:369-414`, abnxt `ab_session.c:76-121` | intermediate |
| every RetroArch launch | `retroarch.cfg` and `retroarch-core-options.cfg` copied to `.bak`, rewritten in full 12-20 times (`ConfigFileEditor::replaceProperty` rewrites the file even when the line is identical, `config_file_editor.cpp:68-90`), restored from `.bak` afterwards; `card1.mcd` <-> `.srm` + `.bak`; `retroarch.log`, `launch.log`; 8 `mkdir -p` + `chmod`; RetroArch itself saves `retroarch.cfg` on exit (`config_save_on_exit` default true) and a runtime `.lrtl` per game | core / rc / RA | redundant + log |
| every App launch | `abpadd.log` (truncated), `abpad.log` (appended, never trimmed), `Home/.cache` on the stick | `app_env.sh:117,127` | log |
| every standby | `standby.log` appended (never trimmed) | `selection.sh:152` | log |
| per loaded extension | `System/Extensions/.active` created/deleted 2x at start and 2x per launch (crash guard) | `extension_runtime.cpp` | intermediate |

Also found on the way: `autobleem.list` has no reader left (its comment says "read by the rc shell
scripts" - none does since the SonyUI link scripts went); the manual says `gamesThatFailedVerifyCheck.txt`
is in `System/Logs`, the code writes it to the state dir (`Autobleem/bin/autobleem`); the console's
RetroArch splash waits for "Found display driver" in the on-stick `retroarch.log` (`launch_rb.sh:49`),
which RetroArch may print only with verbose logging (not set anywhere) - to be checked, it may mean the
splash always waits its full 30 s.

## Principles

1. **RAM first.** Every log and every hand-over file lives in a runtime directory on tmpfs:
   `AB_RUNTIME_DIR` = `/tmp/autobleem` on the console (tmpfs), `/run/autobleem` on the Linux targets
   (systemd `RuntimeDirectory=autobleem` in `autobleem.service` - Raspberry Pi OS Bookworm's `/tmp` is on
   the SD card), `<root>/System/Runtime` on Windows and the dev host (no tmpfs; nobody cares there).
   Logs are `$AB_RUNTIME_DIR/logs/`.
2. **Write only what changed.** Every writer that runs per boot, per scan or per launch compares before it
   writes. This lives in the library (one helper), not in each caller.
3. **Evidence survives a crash.** The runtime logs are copied to the stick when something went wrong, and
   only then (see Phase 1.5). A tester can ask for everything with one switch.
4. **User state stays exactly as it is.** Saves, cards, kept resume slots, settings the user changed,
   the databases' content: same files, same places, written at the same moments.

## Phase 0 - measure (before touching anything)

- `tools/stick_writes.sh` (run over ssh on the Pi and on the console): the data partition's sectors
  written (`/sys/block/<dev>/stat`, field 7) plus `inotifywait -m -r -e close_write,moved_to,create,delete`
  over the mount (the Pi has inotify-tools; for the console, a static `inotifywait` in `Autobleem/bin/`
  for the test only, or just the sector delta), around each scenario:
  boot to the carousel, 10 min idle, a rescan with nothing changed, PS1 launch + exit (no resume
  kept), PS1 launch + exit keeping slot 1, RetroArch game + exit, App + exit, standby + wake.
- Record the baseline in this file. **Targets:** idle and a no-change scan = 0 files; boot = `.session` and
  the dirty flag only; a PS1 round trip without playing = the card only if the game wrote it; RetroArch =
  `.srm`/card only if changed.
- A doctest (`autobleem-core/tests/core/test_quiet_scan.cpp`): scan a `TempDir` stick twice through
  `ScanService`; snapshot `(path, size, mtime)` of the whole tree after the first scan and assert the
  second changes nothing. Same for `Config` constructed twice, `IniFile::save` of an unchanged file and
  `LaunchService`'s RetroArch transfer with a recording `ProcessRunner`. These are the regression guard
  for everything below.

## Phase 1 - logs to RAM

1. **Env.** `ableem::Environment::getPathToRuntimeDir()` / `getPathToLogsDir()` (the latter moves from
   `System/Logs` to `<runtime>/logs`); `getPathToPersistentLogsDir()` = `System/Logs` for the few things
   that stay (below). `EnvironmentSetup` sets the runtime dir per platform (from `AB_RUNTIME_DIR` when the
   environment has it, so the rc scripts and the launcher agree). The launcher exports `AB_RUNTIME_DIR`
   and `AB_LOG_DIR` into everything it starts (`LaunchPlan::env`), so `launch.sh`, `launch_rb.sh`,
   `app_env.sh` and the Apps inherit it.
2. **The log switch, two ways in** (the owner's call: both). `keeplogs` in config.ini - an Options row
   "Keep logs on the stick", default off - and a marker file `System/Logs/keep` a tester can create from a
   PC. Either one turns it on (`AB_KEEP_LOGS=1` in the environment does too, for the dev host). The marker
   is what the boot scripts can see before the launcher runs: `ab_log.sh` checks it, so with it present
   even `boot.sh`/`checkstick.sh`/`selection.sh` log to the stick. On: `AB_LOG_DIR` is `System/Logs` and
   everything below behaves as today. The launcher reads both (`Env::keepLogs()`); the Options row only
   edits config.ini and says so when the marker is what keeps it on.
3. **plog.** The file appender writes `<runtime>/logs/autobleem.log` (256 KB x 2 is plenty in RAM). The
   console appender drops to WARNING+ when a file appender exists on a non-dev build, so `AB_out.txt`
   holds warnings and the children's output, not a second copy of every line. `Log::setLevel()` gets its
   first callers: `loglevel=` in config.ini / `AB_LOG_LEVEL` (debug/info/warning).
4. **Quieter INFO.** Demote to DEBUG: `games_hierarchy.cpp:90` ("compare X with Y", per game pair per
   scan), `config_file_editor.cpp:70,78,119,125,185`, `ini_file.cpp:77`, `game_database.cpp:823,840`,
   `carousel_game.cpp:172,182`, the per-game scan lines in `game_scanner.cpp:355,405,407`. Keep one INFO
   summary line per scan / launch instead.
5. **Shell side.** A sourced `rc/ab_log.sh` (all three payload trees) sets `AB_RUNTIME_DIR`/`AB_LOG_DIR`
   when the environment has not, `mkdir -p`s it, and offers `ab_persist_logs <reason>`:
   - `run.sh` and `autobleem-session.sh` send the launcher's stdout/stderr to `$AB_LOG_DIR/AB_out.txt`
     / `AB_err.txt`.
   - `launch.sh`, `launch_rb.sh`: `launch.log`, `pcsx.log`, `retroarch.log` into `$AB_LOG_DIR`, each
     truncated per launch instead of appended forever. The RetroArch splash greps the RAM copy.
   - `app_env.sh`: `abpadd.log`, `AB_PAD_LOG` into `$AB_LOG_DIR`; `XDG_CACHE_HOME` to
     `$AB_RUNTIME_DIR/cache/<app>` (Home's `.config`/`.local` stay on the stick - that is user state).
   - `selection.sh`'s `standby()`: a successful standby logs to RAM only; the busy-holders list and
     `poweroff_instead()` still append to the stick's `standby.log` (they are the failures we need).
   - `update.log` / `installer.log` stay on the stick (once per update, and the one log that matters when
     the update itself broke the launcher).
   - `backup.sh:43`'s `touch ui_menu.log` goes (nothing reads it).
6. **Crash capture (`ab_persist_logs`).** Copies `$AB_LOG_DIR` to `System/Logs/crash-<n>/` (the last 3
   kept, `n` from a counter file, no clock needed) when:
   - `selection.sh` finds no `autobleem_cfg.sh` (the launcher died) - before its reboot, since the reboot
     empties tmpfs;
   - `launch.sh` sees pcsx exit non-zero; `launch_rb.sh` sees RetroArch exit non-zero (replaces today's
     `retroarch_crash.log` mv + `dmesg`, which moves into the capture);
   - the launcher's own `main()` last-resort `catch`.
   And on request: a "Save logs to the stick" item on the Hardware Information screen (and `--sysinfo`
   output next to them), so a tester can hand over a normal session too.
7. **Emulators.** pcsx-abnxt `ab_config.c:124-125`: line-buffered, not `_IONBF` (the log is in RAM now,
   but a write per `printf` is still a syscall per line). pcsx-ab is fine as it is.
8. **Linux images** (autobleem-appliance): journald `Storage=volatile` (a drop-in; the PC image's journal
   is persistent today - to check), so the `tee` into the journal does not land on the system partition.

## Phase 2 - write only what changed

1. **`DirEntry::writeFileIfChanged(path, bytes)`** in the engine: reads the existing file (size first,
   then bytes), returns without touching it when identical, else `.tmp` + `replaceFile`. Tested.
2. **`IniFile::save`** serialises to a string and goes through it. That alone fixes: `config.ini` on every
   start (`config.cpp:30,111`), on every Options close, on every extension `reloadConfig` (`Config
   fresh;`), `Game.ini` for every game on every scan, the game editor's clamped key presses. The About
   screen's mini-game saves the high score once when the game ends, not per point
   (`gui_about.cpp:234-238`).
3. **`ConfigFileEditor`**: `replaceProperty` rewrites only when a line actually changes; a new
   `replaceProperties(file, map)` does one read and at most one write for a batch - used by
   `GameSettingsService` and by the pcsx side of `LaunchService`. (RetroArch: Phase 3.4 removes the edits
   altogether.)
4. **Scan, engine side:**
   - `.m3u` (`filesystem.cpp:84-114`): build the expected content, `writeFileIfChanged`; delete only the
     `.m3u` files that are not it.
   - `GamesFingerprint::save`, `RetroArchPlaylist::save` (`AutoBleem.lpl`), the EmulationStation
     `gamelist.xml`: through the helper. An empty `roms.fingerprint` is not written (absent = empty).
   - `autobleem.list`: delete the writer (no reader); fix the header comment and the test that checks it.
   - The diagnostic dumps: `gameHierarchy_before/after*` and `duplicateGames.txt` only at DEBUG level and
     into `AB_LOG_DIR`. `gamesThatFailedVerifyCheck.txt` is not written at all any more - the Game Manager
     shows those games (the owner's call), see 2.9. A file left by an older build is deleted once.
5. **Scan, database side** (`scan_service.cpp:392-409`, `game_scanner.cpp:58-76`):
   - compare the scanned record with the row already loaded; `UPDATE GAME` only for a changed game,
     `replaceDiscs` only when the disc list differs;
   - the `SUBDIR_*` tables: diff the new rows against the old, or at least skip the delete/re-insert when
     the hierarchy's fingerprint is unchanged;
   - all of `poll()`'s `Finished` writes in one transaction (today: 2+ journal create/fsync/delete per
     game).
6. **History** (`game_catalog.cpp:42-70`): `UPDATE` only the rows whose rank changes (moving a game from
   rank 5 to 1 touches 5 rows, not 101), one transaction per database. `updateDatePlayed` stays.
7. **rc:** `start.sh:1` runs `sed -i` only when `boot.sh` has a `\r` (`grep -q`); `backup.sh:18` copies
   `AutoBleem.rdb` only when `cmp -s` says it differs; `backup_internal.sh:13`'s `sync` only after a
   copy; `launch_rb.sh:68-70`'s `mkdir -p`/`chmod +x` behind `[ -d ]` / `[ -x ]` tests (a no-op
   `mkdir -p` on FAT is cheap, a `chmod` may not be).
8. **`pcsx.cfg` into `!SaveStates`** (`launch.sh:18`, `payload_linux` `launch.sh:61`,
   `launch.cpp:454-458` `copyCfgAsLf`): `cmp -s || cp`, and the C++ side through the helper.
9. **Games the scan refused, in the Game Manager** (replaces `gamesThatFailedVerifyCheck.txt`):
   - The scan already knows them (`onGameFailedVerify`, the reasons from `UsbGame::verify()`). They go
     into a regional.db table, `FAILED_GAMES (PATH, FOLDER, REASON)`, replaced in the scan's `Finished`
     transaction **only when the set differs** from what is there - a no-change scan still writes nothing.
     A table rather than memory, because a boot whose fingerprint matches runs no scan, and the list must
     still be there.
   - `GameDatabase::loadFailedGames()` / `replaceFailedGames()`, `ScanUpdate` says when the list changed,
     `GameQueryService::failedGames()` for the screen. Tested with the scan tests.
   - The Game Manager gets a heading row "Not added (n)" under the games, one row per folder; the detail
     pane shows the folder, the files it found and the reason in plain words. Cross on one offers "Delete
     folder" (the manager's existing delete, behind its confirm) - nothing else makes sense for a folder
     that is not a game. Hidden when the list is empty.
   - `UsbGame::verify()`'s reasons become `_()` strings (they are shown now), in all 16 language files in
     the same commit; the manuals (en/pl) point at the Game Manager instead of the file.

## Phase 3 - intermediate files off the stick

1. **The selection hand-over** `rc/autobleem_cfg.sh` -> `$AB_RUNTIME_DIR/selection.sh`, written once
   (drop the second `writeSelectionScript()`, `launch.cpp:330` vs `autobleem.cpp:315`). `selection.sh` and
   `autobleem-session.sh` read it there. The "no file = the launcher crashed" rule stays - tmpfs survives
   a launcher crash, only not a reboot, and the reboot is what the missing file triggers anyway.
2. **The exit resume state.** Both emulators (we own both) get a `-exitdir <dir>` option: the exit state,
   its PNG, `filename.txt` and `lastcdimg.txt` go to `$AB_RUNTIME_DIR/exit/` instead of `!SaveStates`.
   `ResumePointService::saveAfterLaunch` then copies them into the slot - one write of the bytes the
   player kept, none when they did not. Resuming passes the slot file directly (`-load <path>` taking a
   file, or a new `-loadfile`) instead of copying `.00N.res` -> `.000` first
   (`resume_point.cpp:175-216`). `exitedCleanly()` reads `filename.txt` from the exit dir. pcsx-ab stops
   writing `filename.txt` at start-up (`main.c:848`) and pcsx-abnxt stops writing `lastcdimg.txt` at
   start-up (`main.c:744`). A launcher without the new emulator version falls back to today's behaviour
   (the emulator ignores nothing it does not know - check its argv parsing first).
   **Trade-off:** a power cut mid-game loses nothing extra - the exit state only ever existed at exit.
3. **Memory card sets.** Instead of backup -> swap in -> play -> copy back -> restore
   (`memcard_manager.cpp:46-60,132-142,157-189`, ~4 copies of each 128 KB card per launch): give the
   emulator the set's card path (`-mcd1 <file>`, both emulators; pcsx.cfg already has an `Mcd1` path the
   frontend could be told to override). Nothing is copied; the game writes its frames straight into the
   set. The `backup/` crash-recovery path at start-up (`restoreAll`) stays for sticks left mid-swap by an
   older build, then goes in a later release. Same for RetroArch's `.srm`: copy `card1.mcd` to
   `$AB_RUNTIME_DIR/ra-saves/<base>.srm`, point RetroArch's `savefile_directory` there for the run (3.4),
   and copy it back only if `cmp` says it changed - no `.srm.bak` dance in `saves/`.
4. **RetroArch's per-game settings without touching its files.** `transferRaConfig` today backs up both
   files, rewrites them 12-20 times, and restores them afterwards - so the net effect on the stick is
   *nothing*, and whatever the player changed in RetroArch during that run is thrown away by the restore.
   The same effect with zero writes: write the per-game keys to `$AB_RUNTIME_DIR/ra-append.cfg` and start
   RetroArch with `--appendconfig` (both launch scripts take it as an argument from `LaunchService`); the
   core options go to a copy in `$AB_RUNTIME_DIR` named by `core_options_path` in that append file.
   **To verify on RetroArch 1.22:** that `--appendconfig` values are not baked into `retroarch.cfg` by
   `config_save_on_exit` (they were in some versions - if so, 3.5 is a prerequisite, not an option).
5. **RetroArch's own writes** - the shipped/installed `retroarch.cfg` (the installer's `installer_job.cpp`,
   `payload_linux/install.sh`, and a one-time fix-up for existing sticks through `set_cfg_key` /
   `ConfigFileEditor`):
   - `content_runtime_log = "false"`, `content_runtime_log_aggregate = "false"` (per-game `.lrtl` files
     nobody reads);
   - `log_to_file = "false"` stays; `log_verbosity` stays off (logs go to the RAM `retroarch.log` via
     stdout);
   - `history_list_enable` stays on (the launcher's History set reads it).
   - **`config_save_on_exit` is switchable** (the owner's call): config.ini `rapersist`, an Options row
     "Persist RetroArch config", default **on** (today's behaviour). The launcher does not edit
     `retroarch.cfg` for it: the value goes into the same `$AB_RUNTIME_DIR/ra-append.cfg` as the per-game
     keys (3.4), written for *every* RetroArch start - games from the carousel and the system menu's
     RetroArch item alike (`retroarch.sh`/`launch_rb.sh` pass `--appendconfig` whenever the file exists).
     Off: RetroArch never saves its config on exit, and "Save Current Configuration" in its menu is the way
     to keep a change. On: RetroArch saves at exit as it always did, and with the `.bak`/restore gone (3.4)
     a change the player made during a game now actually persists.
     **Verify first on RetroArch 1.22:** that an `--appendconfig` value is not written into
     `retroarch.cfg` by the save on exit. If it is, "on" would bake the per-game keys into the global
     file; then, with `rapersist` on only, the launcher strips exactly the keys it appended from
     `retroarch.cfg` after the run (through `replaceProperties`, which writes only if one is there).
6. **Extensions' crash guard** (`System/Extensions/.active`): moves to `$AB_RUNTIME_DIR`, and
   `ab_persist_logs` (Phase 1.6) copies it to the stick before the crash reboot, where
   `ExtensionRuntime` looks for it at the next start. The guard keeps working; the per-launch write/delete
   pairs leave the stick.
7. **App homes:** `XDG_CACHE_HOME` in RAM (Phase 1.5). `System/platform_keys` already compares first.

## Phase 4 - keep it quiet

- CLAUDE.md, "Conventions": *nothing that runs per boot, per frame, per scan or per launch writes to the
  data root unless the user's state changed; logs and hand-over files go to `AB_RUNTIME_DIR`; files go
  through `writeFileIfChanged`*. The Phase 0 doctests fail the build when someone breaks it.
- `tools/stick_writes.sh` rerun on the console and the Pi at the end; the numbers go into this file next
  to the baseline.
- The manuals (en/pl): where the logs are now, the "Keep logs on the stick" option, the crash folders.
- `docs/translation.md` flow for the new Options row / Hardware Information item (all 16 languages).

## Order and size

| Step | Repos | Size | Win |
|---|---|---|---|
| 0 measure + guard tests | AutoBleem2, autobleem-core | S | makes the rest checkable |
| 1 logs to RAM | autobleem-core, AutoBleem2 (rc x3 trees), autobleem-appliance, pcsx-abnxt | M | the largest volume: every log line, every launch |
| 2 write-if-changed | autobleem-core, AutoBleem2 rc | M | every boot and every scan go silent |
| 2.9 refused games in the Game Manager | autobleem-core, AutoBleem2 (screen, 16 languages, manuals) | M | the last per-scan file goes, and users see why a game is missing |
| 3.1, 3.4-3.7 hand-over, RetroArch, extensions | autobleem-core, rc, installer | M | every launch goes silent |
| 3.2-3.3 emulator exit state, memcards | pcsx-rearmed-develop, pcsx-abnxt, autobleem-core | L | ~1 MB+ per PS1 exit, the card copies |
| 4 docs, rerun | all | S | |

One commit per numbered item, the tests with it; a core change is a commit in autobleem-core and a
submodule bump here. Phases 1 and 2 are independent and can go in either order; 3.2/3.3 need emulator
releases before the launcher can rely on the new options, so the launcher side keeps the old path as a
fallback until both emulators ship.

## Decisions (the owner, 2026-09-24)

1. **The log switch: both** - config.ini `keeplogs` with its Options row, and the `System/Logs/keep`
   marker file (Phase 1.2).
2. **RetroArch's save on exit: switchable** - config.ini `rapersist`, Options row "Persist RetroArch
   config", applied through the append file (Phase 3.5).
3. **Crash folders: as proposed** - the last 3 kept (`System/Logs/crash-<n>/`), and a notification on the
   next start: the launcher's `NotificationBubble` says "AutoBleem restarted after a problem - logs saved
   to System/Logs/crash-<n>" once (`ab_persist_logs` leaves `System/Logs/crash-<n>/.new`, the launcher
   removes it after showing the message).
4. **Refused games: shown in the Game Manager**, no file (Phase 2.9).

New on-screen strings from these (two Options rows, the Hardware Information item, the crash message,
the Game Manager heading and the verify reasons) land in all 16 language files in the commit that adds
them.
