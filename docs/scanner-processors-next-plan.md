# Scanner processors - what comes next (plan)

**Status (2026-09-25):** a list for later, nothing started. The facility itself is done and in `develop`
(autobleem-main's `docs/archive/scanner-processors-plan.md`, steps 1-9); `proc_unzip` is public with its CI; the installers make
`System/Processors`. The owner will test on hardware later (autobleem-main's `docs/tester-checklist.md`,
section 11). Everything below waits for that, and for the owner's go.

## 1. `proc_unzip` v1.0.0 - its first release

It has only the rolling `nightly` release. A `v1.0.0` tag runs its CI's `release` job (the package for every
platform key, built in `autobleem-build:latest`). Before tagging: the hardware pass (console and Pi), and a
look at whether the tag and `package/processor.ini`'s `Version=` should move together (`Version=` is 1.1.0
since 7z and RAR went in - the first tag could simply be `v1.1.0`). After it: the download site could list processors like the Store lists Apps -
see 3.

## 2. `.7z` in `proc_unzip` - done (2026-09-25)

proc_unzip 1.1.0 reads `.7z` and `.rar` (RAR 1.5-4.x and RAR5, volume sets `Game.partN.rar` or `Game.rar` +
`.r00`...) as well as `.zip`, still with no AutoBleem code: libarchive 3.8.9 (the read core and the 7-Zip, RAR
and RAR5 readers) over liblzma 5.8.4 (decoders only), vendored and trimmed, built with hand-written
configuration. The LZMA SDK route was not taken: its 7z decoder unpacks a whole solid block into memory, which a
solid 7z of a disc would not survive on the console, and it has no RAR; RARLAB's UnRAR is not GPL-compatible.
`Match=*.zip;*.7z;*.rar`. A file a stopped run left is compared byte by byte as the archive unpacks (libarchive
gives no CRC). Not read: a split 7z, anything with a password. proc_unzip's CLAUDE.md has the details.

## 3. Processors in the Store

A Store item kind `processor` (ext_store): one zip per processor, laid out as `System/Processors/<name>/`
with the platform's `bin/<key>/`, installed there by the Store's installer and added to the sequences by the
next scan (a new processor goes to the end, switched on). Needs: the item kind in the catalog format and
`ContentInstaller`, the Store's tab (or a filter in Apps), the site's `/store/` page. The Store is the
extensions session's work - coordinate, do not edit ext_store from here.

## 4. Launch-time processors

The first plan's "Later": a third kind, `--prepare` / `--cleanup` around a launch - a format decoded next to
the game just for the session (in the runtime dir on a Pi or a PC; on the console `/tmp` is RAM, so only
small things), or a mod applied without touching the original. Same protocol and manifest (`Kinds=launch`),
run by `LaunchService` before the emulator and after it returns, with the bubble replaced by the launch
splash. Open questions for the owner: whether a launch may wait for a processor at all (a slow decode delays
every start), and whether the emulator may be handed a path the processor chose.

## 5. Signatures or a trusted list

Once processors come from the Store (3), a processor is code run as root on the console. Options: the Store
installs only what its catalog lists with a SHA-256, and the launcher runs any processor as it does now (the
catalog is the trust); or a signature file next to `processor.ini`, checked by the launcher before the first
run. Decide together with the extensions' SDK package (which has the same question for plugins).

## 6. Watch in testing (from the first plan)

Heavy jobs on the console: how long a large job takes, the launcher's frame rate while one runs, and whether
being stopped for every launch means it never finishes. The fix, if needed: `Heavy=true` in processor.ini,
run only from the Scanner processors screen ("Run now") instead of every automatic scan.
