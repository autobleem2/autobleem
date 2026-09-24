# Multi-platform Apps and extensions (plan)

**Status (2026-09-24):** planned, nothing built. This file defines one folder format that carries a
program for **every platform we build for, now and later**. The same folder holds several platforms'
binaries side by side. Its ini says which binary is for which platform, and the launcher, `run.sh` and
Windows all pick the right one by the same rule. It applies to both kinds of folder:

- **Apps** (`Apps/<name>/`, `app.ini`): third-party programs the carousel's Apps set starts;
- **extensions** (`Extensions/<name>/`, `extension.ini`, `docs/extensions-plan.md`): our SDK programs
  started from the System menu.

The Store (`docs/store-plan.md`) installs Apps in this format.

## Why

Today an App is a console App:

- `app.ini`'s `Startup=run.sh` names one script;
- `run.sh` starts one binary, built for the console;
- `rc/app_env.sh` hard-codes `/media` and `Autobleem/lib/apps`.

That already does not fit the Pi (32 and 64-bit), the PC stick or Windows, which has no `sh` at all. A new
target, such as the Atari VCS (`docs/atari-vcs-plan.md`) or a 64-bit PC stick, would mean yet another
layout. A stick or data partition may also carry an App for more than one machine, and an App's data
files (levels, music, its `pad.ini`) are the same on every platform, so they should be stored once.

## Platform keys

A **platform key** names what a binary was built for. The launcher knows an **ordered list** of the keys
it accepts: the most specific first, falling back to more generic ones. It is `Env::appPlatformKeys()`,
derived from the build's target (`AB_TARGET`) and its compiled architecture. These are the same facts
`UpdateService` already turns into `psc`/`rpi`/`rpi64`/`pcusb`/`win`.

| target | keys, in order | why the generic key (or not) |
|---|---|---|
| `psc` | `psc` | no generic key: the console has glibc 2.24, its own SDL 2.0.14 on Wayland and no X, so a generic armhf Linux binary would not load |
| `rpi` (32-bit) | `rpi`, `linux-armhf` | any armhf Linux build against Bookworm's libraries runs |
| `rpi64` | `rpi64`, `linux-arm64` | |
| `pcusb` | `pcusb`, `linux-i386` | |
| `win` | `win`, `windows-x86_64` | |
| `dev` | `dev`, then `linux-x86_64` or `windows-x86_64` | a development machine |
| future (e.g. Atari VCS) | `<target>`, `linux-x86_64` | an App already built for generic x86_64 Linux runs there without repackaging |

- A target key means "built and tested for exactly this machine". A generic key means "any machine of this
  OS and architecture with a current distribution".
- A new target adds a row here and a case in `Env::appPlatformKeys()`. No existing App needs a change.
- The platform ini may add keys after the built-in ones (`app_platform_keys=` in
  `resources/platform/<platform>.ini`) for an odd machine. It cannot remove any.

## The folder

```
Apps/opentyrian/
    app.ini
    run.sh                      optional: only for an App that needs to set something up (below)
    icon.png                    shared by every platform
    data/                       the App's own files, shared (levels, music, ...)
    pad.ini                     the virtual gamepad profile, shared
    bin/psc/opentyrian          one folder per platform key
    bin/rpi64/opentyrian
    bin/linux-armhf/opentyrian
    bin/pcusb/opentyrian
    bin/win/opentyrian.exe
    lib/psc/...                 optional: libraries only this platform's binary needs
    lib/rpi64/...
```

`bin/<key>/` and `lib/<key>/` are conventions, not rules. The ini says where each binary is.

### `app.ini` (and `extension.ini`, which uses the same keys)

```ini
[app]
Title=OpenTyrian
Author=...
Version=2.1.20221123
Image=icon.png
Readme=readme.txt

; the program, per platform key (paths relative to the folder)
Exec.psc=bin/psc/opentyrian
Exec.rpi64=bin/rpi64/opentyrian
Exec.linux-armhf=bin/linux-armhf/opentyrian
Exec.pcusb=bin/pcusb/opentyrian
Exec.win=bin/win/opentyrian.exe
; and/or one pattern for every key without its own Exec.<key> line ({key} = the key being tried);
; an App following the bin/<key>/ convention needs only this line
Exec=bin/{key}/opentyrian

Args=--fullscreen                ; optional, shared; Args.<key>= overrides it for one platform
Lib=lib/{key}                    ; optional, added to the library path (Lib.<key>= overrides)
Kernel=false                     ; unchanged: needs the AutoBleem kernel
```

**The rule, in one sentence:** for each key in `Env::appPlatformKeys()`, in order, use `Exec.<key>` if
the ini has it; otherwise use `Exec` with `{key}` replaced by that key. The first candidate whose file
exists is the program.

- `.exe` is added on Windows when the file named has no extension and the `.exe` exists, so one
  `Exec=bin/{key}/opentyrian` serves every platform.
- An App with no candidate for this machine is **not listed** in the Apps set. The Store shows it as
  "Not available for this system".
- `Args`, `Lib` and `Env.<VAR>=` (extra environment variables) resolve the same way: `.<key>` first, then
  the plain key with `{key}` replaced.
- `Startup=` (today's key) still works: an ini with no `Exec` is started through its `Startup` script, as
  now, on the platforms that have `sh`. It is how every existing App keeps working unchanged.

### Where the rule lives

**One implementation, in C++, in core** (`AppManifest`, `lib_ableem`'s ini + core's `Env`, with tests):
`AppManifest::load(folder, keys)` returns the resolved program, args, library path, environment and the
key that matched, or why nothing matched.

- **The launcher** (`GameQueryService::apps()` for the list, `LaunchService::planApp` for the start)
  resolves the program. It exports the result to what it starts:

  | variable | value |
  |---|---|
  | `AB_APP_DIR` | the folder |
  | `AB_APP_EXEC` | the resolved program, absolute |
  | `AB_APP_ARGS` | the resolved args |
  | `AB_APP_LIB` | the resolved library path |
  | `AB_APP_KEY` | the key that matched |
  | `AB_PLATFORM` | the target |
  | `AB_PLATFORM_KEYS` | the whole ordered list, space separated |
  | `AB_ROOT` | the data root |

- **Linux targets:**
  - An App **without** its own `run.sh`: the launcher runs `rc/app_run.sh`, a generic script shipped with
    the launcher. It sources `rc/app_env.sh` and then does `exec "$AB_APP_EXEC" $AB_APP_ARGS`.
  - An App **with** a `run.sh` (to prepare a config file, pick a data set, ...): the launcher runs that
    instead, with the same environment. It sources `app_env.sh` and ends in `exec "$AB_APP_EXEC" ...`.
  - Either way the script "knows" the binary from the ini, because the launcher resolved it from the ini.
- **Windows:** no `sh`. The launcher starts `AB_APP_EXEC` directly (the Windows product's direct-launch
  path, `Env::directLaunch()`) with the same environment and `AB_APP_LIB` prepended to `PATH`. A `run.sh`
  is ignored there; an App that needs set-up on Windows puts a `run.cmd` in `Exec.win`.
- **Run by hand** (ssh, debugging) there is no launcher to resolve anything. `rc/app_env.sh` then resolves
  the ini itself, with the same rule written in `sh`, when `AB_APP_EXEC` is not already set:
  - the keys come from `$AB_PLATFORM_KEYS`, else from the file the launcher writes at start-up,
    `<state>/platform_keys` (one line);
  - a test runs both resolvers over the same fixture folders and compares the results, so the two cannot
    drift.

### `rc/app_env.sh` becomes platform-neutral

- `AB_ROOT` comes from the environment; `/media` is only the console's fallback.
- The shared libs pack (`Autobleem/lib/apps`, the console's) is linked only when `AB_APP_KEY` is `psc`.
  `AB_APP_LIB` is prepended in every case.
- The `$HOME`/XDG and virtual gamepad sections stay as they are.

The console's and `payload_linux/`'s copies are the same file after this, which settles one line of
autobleem-main's `todo.md` for this script.

### Extensions

An extension is a plugin (a `.so`/`.dll` loaded into the launcher, `docs/extensions-plan.md`), so
`extension.ini` names a library rather than a program: `Plugin=bin/{key}/store`, resolved by the same
`AppManifest` rule over the same keys, with `.so` or `.dll` added per platform. `Args` and `Env` do not
apply to something loaded in-process. A plugin's own libraries in `lib/<key>/` are found through its
`$ORIGIN` RUNPATH, not `Lib`. `ExtensionService` uses `AppManifest` and adds only the extension's own
fields (`Name`, `Description`, `Background`).
- **PSC-Bios and ABFlashKit stay Apps** (decided 2026-09-24). They are converted like the other console
  Apps (step 4), with only a `psc` binary.

### Packages and the Store

- **One folder, several platforms**: a package is a zip of `Apps/<name>/...`. It may carry one platform
  (`opentyrian-psc-<v>.zip`: the shared files plus `bin/psc/`) or several
  (`opentyrian-<v>.zip`: all of them).
- **Installing merges**: `AppInstaller` (store plan) lays a package over an existing folder of the same
  App, and does not delete another platform's `bin/<key>/` or `lib/<key>/`.
  - The shared files and the ini are replaced by the package's, so the same version is expected for every
    platform.
  - A package whose `Version` differs from what is installed replaces the other platforms' binaries too,
    so no two versions ever mix. The Store then offers the other platforms again.
- The site's catalog stays per platform (`store/<platform>/catalog.json`). An item's zip is the
  one-platform package, so a console never downloads Windows binaries.

## Steps

Each step is one commit (core first, then the submodule bump), with its tests.

1. **Not done.** Core: `Env::appPlatformKeys()` (the table above, plus `app_platform_keys=` from the
   platform ini), and `AppManifest` (the rule: `Exec.<key>`, `Exec` with `{key}`, `.exe`, `Args`/`Lib`/
   `Env`, the `Startup` fallback), with tests over fixture folders.
2. **Not done.** Launcher: `GameQueryService::apps()` lists only Apps with a candidate;
   `LaunchService::planApp` runs through `AppManifest` with the environment above; `rc/app_run.sh`;
   `app_env.sh` made platform-neutral, with its `sh` resolver and the comparison test. Existing Apps
   (`Startup=run.sh`) are checked to start unchanged.
3. **Not done.** Windows: the direct launch of `AB_APP_EXEC` with `PATH` and the environment; an `Apps/`
   folder in the Windows product's data root.
4. **Not done.** The eight console Apps converted (`tools/pack_psc_apps.py`): binaries to `bin/psc/`,
   `Exec=bin/{key}/<name>`, `run.sh` kept only where it does something. They repack per App for the Store.
5. **Not done.** `ExtensionService` on `AppManifest` (with the extensions plan's step 1).
6. **Not done.** OpenTyrian as the first real multi-platform App: `psc`, `rpi`, `rpi64`, `pcusb`, `win`
   builds in one folder, run on each (the tester checklist).
7. **Not done.** Documentation for App authors: the folder, the keys table, the ini, when to write a
   `run.sh`; the manuals' Apps section.

## Settled (2026-09-24)

- **The 32-bit PC stick does not accept `linux-x86_64`**, even on a 64-bit CPU: its libraries are i386.
  A 64-bit PC stick would be a new target key (`pcusb64`) with `linux-x86_64` after it.
- **No per-platform data keys** (`Data.<key>=`) until an App needs one.
