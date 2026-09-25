# Scanner processors - what comes next (plan)

**Status (2026-09-25):** a list for later, nothing started. The facility itself is done and in `develop`
(`docs/scanner-processors-plan.md`, steps 1-9); `proc_unzip` is public with its CI; the installers make
`System/Processors`. The owner will test on hardware later (autobleem-main's `docs/tester-checklist.md`,
section 11). Everything below waits for that, and for the owner's go.

## 1. `proc_unzip` v1.0.0 - its first release

It has only the rolling `nightly` release. A `v1.0.0` tag runs its CI's `release` job (the package for every
platform key, built in `autobleem-build:latest`). Before tagging: the hardware pass (console and Pi), and a
look at whether the version on the tag and `package/processor.ini`'s `Version=` should move to 1.0.0 together
(they are both 1.0.0 now). After it: the download site could list processors like the Store lists Apps -
see 3.

## 2. `.7z` in `proc_unzip`

The first plan's step 8 said `.7z` too "if a small enough decoder can be vendored". core already has
`SevenZipArchive` (`lib_ableem/engine/seven_zip_archive.h`, the Windows installer's), but proc_unzip links no
AutoBleem code on purpose - it is the example of a processor with nothing but the standard library. Options:
- vendor the LZMA SDK's 7z decoder (`7zDec.c`, `7zArcIn.c`, `LzmaDec.c`, ... - public domain, ~100 KB of
  source) into `third_party/lzma/`, as core's libchdr deps already do;
- a separate `proc_un7z`, so unzip stays the minimal example.
Either way the same rules: `.part` then rename, a stopped run's finished files kept (size + CRC), the archive
deleted after the last rename, the self-test and `proc_check.py` in CI. Match becomes `*.zip;*.7z`.

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
