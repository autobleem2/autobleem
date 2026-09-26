# Multi-platform Apps and extensions (plan)

**Status (2026-09-25):** steps 1-3 are done: the rule is in core, the launcher lists and starts Apps by it,
the three rc scripts carry it, and the Windows product has its `Apps/` folder (the launcher and
AutoBleemWinSetup make it). OpenTyrian is the first App built for every target (`autobleem2/app_opentyrian`,
step 6 - built and run on the dev PC, the hardware pass to come). What is left: converting the eight console
Apps (the third-party ports, one `app_<name>` repository each - autobleem-main `docs/decisions.md`) and the
App authors' documentation.
This file defines one folder format that carries a
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

# the program, per platform key (paths relative to the folder)
Exec.psc=bin/psc/opentyrian
Exec.rpi64=bin/rpi64/opentyrian
Exec.linux-armhf=bin/linux-armhf/opentyrian
Exec.pcusb=bin/pcusb/opentyrian
Exec.win=bin/win/opentyrian.exe
# and/or one pattern for every key without its own Exec.<key> line ({key} = the key being tried);
# an App following the bin/<key>/ convention needs only this line
Exec=bin/{key}/opentyrian

# optional, shared; Args.<key>= overrides it for one platform
Args=--fullscreen
# optional, added to the library path (Lib.<key>= overrides)
Lib=lib/{key}
# optional environment variables, NAME=value separated by ';' (Env.<key>= adds or overrides per platform)
Env=SDL_AUDIODRIVER=alsa;TYRIAN_DATA=data
# whether the App runs with our virtual pad mapper (abpadd + the libabpad.so preload,
# docs/virtual-gamepad-plan.md); absent = true, what every App before the key had
VirtualPad=true
# unchanged: needs the AutoBleem kernel
Kernel=false
# optional (2026-09-26): which group the set picker's Apps tab lists it under
Category=Games
```

**`VirtualPad=`** (decided 2026-09-24) is the App's own statement about the pad mapper:

- `true`: the App is meant to be played through it; the default, and what every App before the key had.
- `false`: the App reads the pads its own way (it ships its own mapping, or has no use for a pad), and
  `app_env.sh` starts neither the daemon nor the preload for it.

The launcher passes it as `AB_APP_VIRTUAL_PAD=1|0` (`AppManifest::usesVirtualPad()`); a `run.sh` started
by hand reads it from the ini the same way.

**`Category=`** (decided 2026-09-26) is which group the set picker's Apps tab lists an App under - `Games`,
`Emulators`, `Tools` or `Media`, case-insensitive (`GameQueryService`'s `parseAppCategory`); missing or
anything else is `Other`. The tab always starts with "All apps", then one row per category that has at
least one App present (`GameQueryService::appCategories()`); a stick with every App in one category shows
just that one row alongside "All apps". The app_* repos and the Store catalog will carry their own category
later - not part of this change.

**Repositories** (decided 2026-09-24): an App's source repository is named **`app_<name>`**
(`app_opentyrian`), an extension's **`ext_<name>`** (`ext_store`). The folder it installs to keeps the bare
name (`Apps/opentyrian/`, `Extensions/store/`).

Comments are `#` lines (`IniFile` knows no `;` comments), and keys are case-insensitive: `IniFile`
lower-cases them. That is why the environment is one `Env=` list rather than a key per variable, whose
name would lose its case.

**The rule, in one sentence:** for each key in `Env::appPlatformKeys()`, in order, use `Exec.<key>` if
the ini has it; otherwise use `Exec` with `{key}` replaced by that key. The first candidate that is an
existing file is the program.

- `.exe` is added on Windows when the file named is not there as given and the `.exe` is, so one
  `Exec=bin/{key}/opentyrian` serves every platform.
- An App with no candidate for this machine is **not listed** in the Apps set. The Store shows it as
  "Not available for this system".
- `Args` and `Lib` resolve the same way for the key that matched: `.<key>` first, then the plain key with
  `{key}` replaced. `Env=` is read first, then `Env.<key>=` overrides or adds variables.
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
  path, `Env::directLaunch()`) with the same environment and a `PATH` of `AB_APP_LIB`, then the launcher's
  own folder - so an App uses the launcher's `SDL2.dll`, as it uses the launcher's SDL2 on the console -
  then the inherited one. A `run.sh` is ignored there; an App that needs set-up on Windows puts a `run.cmd`
  in `Exec.win`. An App of the old kind (`Startup=` only) cannot run there: `AppManifest` refuses it, so the
  Apps set does not list it and the Store does not install it (2026-09-25).
- **SDL2** is never in an App's `lib/<key>/` (autobleem-main `docs/decisions.md`, "Third-party App ports"):
  the launcher's own on the console (`app_env.sh` puts `/tmp/lib` ahead of the libs pack for an App with
  `AB_APP_KEY=psc`) and on Windows (the `PATH` above), the system's on the Pis and the PC stick.
- **Run by hand** (ssh, debugging) there is no launcher to resolve anything. `rc/app_env.sh` then resolves
  the ini itself, with the same rule written in `sh`, when `AB_APP_EXEC` is not already set:
  - the keys come from `$AB_PLATFORM_KEYS`, else from the file the launcher writes at start-up,
    `System/platform_keys` (one line, space separated). The rule lives in `rc/app_resolve.sh`, which
    `app_env.sh` sources;
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

1. **Done** (2026-09-24). Core: `Env::appPlatformKeys()` (the table above, plus `app_platform_keys=` from
   the platform ini), and `AppManifest` (`core/services/app_manifest.*`: `Exec.<key>`, `Exec` with
   `{key}`, `.exe`, `Args`/`Lib`/`Env`, the `Startup` fallback), tested in `tests/core/test_app_manifest.cpp`.
2. **Done** (2026-09-24).
   - Core: `GameQueryService::apps()` lists only Apps with a candidate; `LaunchService::planApp` goes
     through `AppManifest` with the `AB_APP_*` environment; `System::runAndWait` and `LaunchPlan` carry an
     environment.
   - The launcher writes `System/platform_keys` at start-up.
   - Scripts: `rc/app_run.sh`, `rc/app_resolve.sh` (the `sh` copy of the rule), and one platform-neutral
     `rc/app_env.sh`. Each is the same file in `payload/`, `payload_linux/` and autobleem-appliance's
     `payload_linux/`.
   - Tests: `tests/rc/test_app_resolve.cpp` holds the shell copy to the C++ answers and the two payload
     copies identical; `test_launch`/`test_game_query` cover the new paths.
   - Existing Apps (`Startup=run.sh` only) are started exactly as before.
3. **Done.** The direct launch of the resolved program with `Args`, `PATH` and the environment is in
   (`planApp`, tested); the Windows product's data root has `Apps/` (`EnvironmentSetup::fromWindowsInstall`,
   `WindowsInstallJob`); OpenTyrian's Windows build ran on the dev PC as the launcher starts it. The first
   run on an installed Windows product is the tester checklist's.
4. **Not done.** The eight console Apps converted (`tools/pack_psc_apps.py`): binaries to `bin/psc/`,
   `Exec=bin/{key}/<name>`, `run.sh` kept only where it does something. They repack per App for the Store.
5. **Not done.** `ExtensionService` on `AppManifest` (with the extensions plan's step 1).
6. **Built.** OpenTyrian as the first real multi-platform App (`autobleem2/app_opentyrian`, 2026-09-25):
   `psc`, `rpi`, `rpi64`, `pcusb`, `win` packages from its CI. Left: a run on each (the tester checklist).
7. **Not done.** Documentation for App authors: the folder, the keys table, the ini, when to write a
   `run.sh`; the manuals' Apps section.

## Settled (2026-09-24)

- **The 32-bit PC stick does not accept `linux-x86_64`**, even on a 64-bit CPU: its libraries are i386.
  A 64-bit PC stick would be a new target key (`pcusb64`) with `linux-x86_64` after it.
- **No per-platform data keys** (`Data.<key>=`) until an App needs one.
