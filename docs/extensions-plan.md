# Extensions (plan)

**Status (2026-09-24):** planned, nothing built. An **extension** is a program built against the AutoBleem
SDK, which draws its screens with the user's theme and our UI components. It is copied onto the stick by
hand and started from one place in the launcher's System menu. The first one is the **AutoBleem Store**
(`docs/store-plan.md`).

This is our answer to what Project Eris calls "mods". Nothing of theirs is used (autobleem-main's
`decisions.md`): the idea is general, and the design below is written from our own code.

## Where we already are

Two extensions exist in all but name:

- **PSC-Bios and ABFlashKit** (autobleem-console-tools) are separate programs built from the
  autobleem-core submodule:
  - they link `ab_core` + `ab_classic` (`AppBase`, `Gui`, `PanelStyle`, `TextRenderer`, the list menus,
    `GuiConfirm`, `GuiKeyboard`, `GuiFactsPage`, `GuiActionMenu`);
  - they find their folder and the data root through `EnvironmentSetup::forTool()`;
  - they read the same `config.ini` and `theme.json`, so they look like the launcher and speak its
    language.
- **The launcher starts one of them** today by hand. The System menu's Hardware Information item runs
  `Apps/pscbios/run.sh` (`GuiLauncher::loop_openSystemMenu`, `evoui_launcher_actions.cpp`): it closes the
  audio, runs the tool with `System::runAndWait`, and brings the audio and pads back afterwards.

Extensions turn that one special case into a mechanism: a folder format, a list screen, a launch protocol
and a way back.

## The decisions (the owner, 2026-09-24)

1. **An extension is a binary package** built with our SDK: autobleem-core, meaning `ab_core`,
   `ab_classic` and `lib_ableem`. Its screens are our screens, in the selected theme.
2. **One central place to run them**: the System menu's **Extensions** item opens a list of what is
   installed, and Cross runs one.
3. **Installed by hand**: the user unpacks an extension's package onto the stick (or the data partition,
   or the Windows data folder). Nothing installs or updates an extension for them, and that includes the
   Store.
4. **Every target**: an extension is built per platform like the launcher (psc, rpi, rpi64, pcusb, win).
5. **The AutoBleem Store is the first extension.**

## The design

### A separate process, not a plugin (recommended)

An extension is its own **executable**, statically linked against the SDK, which the launcher starts and
waits for, as it starts a game. The alternative is a `.so`/`.dll` loaded into the launcher (`dlopen`), and
it was set aside for three reasons:

- **No C++ ABI to keep stable.** A plugin would have to match the launcher's compiler, standard library
  and every SDK class layout, and our targets are built by four different compilers (the console's gcc-6,
  Debian's cross gcc, the PC's, llvm-mingw). A static executable carries its own copy of the SDK, so a
  launcher update never breaks an installed extension, and the reverse.
- **A crash stays in the extension.** The launcher comes back and says so.
- **It is already how the console tools work**, and those are proven on hardware.

What it costs: an extension runs only while it is open. The launcher is waiting meanwhile, so nothing of
the extension runs in the background. For the Store this means downloads happen while the Store is open,
and the queue is saved and resumes the next time it is opened. See "Later" for a background companion
process.

### The folder

`Extensions/<name>/`, next to `Apps/` and `Themes/` at the data root (`Env::getPathToExtensionsDir()`,
set by `EnvironmentSetup` like the others):

```
Extensions/store/
    extension.ini
    icon.png            the list's icon (theme-neutral, 128x128)
    autobleem-store     the program (autobleem-store.exe on Windows)
    lang/               its own translations, <Language>.txt in the launcher's format
    ...                 anything else it needs; its state goes to System/Extensions/<name>/
```

`extension.ini`:

```ini
[extension]
Name=AutoBleem Store                 ; shown in the list (untranslated; a lang/ file may translate it)
Description=Download apps and games
Author=AutoBleem team
Version=1.0.0
Api=1                                ; the launch protocol version it was built for (see below)
Exec=bin/{key}/autobleem-store      ; the multi-platform rule (docs/app-format-plan.md)
Icon=icon.png
```

The program is found by **the same rule as an App's** (`docs/app-format-plan.md`: `Exec.<key>`, then
`Exec` with `{key}`, over `Env::appPlatformKeys()`, resolved by `AppManifest`). One folder can therefore
carry every platform's binary in `bin/<key>/`, next to the shared icon and `lang/`. An extension with no
binary for this machine is shown greyed: "Not available for this system". A package may hold one platform
or all of them, and unpacking one over another adds its `bin/<key>/`.

### The API has two halves

- **At build time**: the SDK, which is autobleem-core at a tag.
  - An extension repository has autobleem-core as a submodule, as autobleem-console-tools does.
  - It builds in the autobleem-build image for every target.
  - It gets a CMake helper, `ab_add_extension(<name> SOURCES ... ICON ... LANG ...)`, which builds the
    executable and packs `<name>-<platform>-<version>.zip` laid out as `Extensions/<name>/...`.
  - On top of `ab_classic`, the SDK gains a small **`ab_extension`** library:
    - `ExtensionApp : AppBase`, which does the start-up: `EnvironmentSetup::forExtension()` (the root,
      its folder, its state dir), its `lang/` merged over the launcher's, and a check of the protocol;
    - `ExtensionResult`, the way back (below).
- **At run time**: the launch protocol, the only contract between a launcher and an installed extension,
  versioned by `Api=`:
  - **Command line**: `<exec> --root <data root>`. The working directory is the extension's folder.
  - **Environment**:
    - `AB_VERSION` (the package version, as for an App);
    - `AB_EXTENSION_API` (the launcher's protocol version);
    - `AB_PLATFORM` (`Env::platformName()`);
    - `AB_EXTENSION_RESULT` (the path of the result file).
  - **Everything else it reads from the data root, as the console tools do**:
    - `config.ini` (theme, language, colours, the online setting);
    - `theme.json` through `Theme`;
    - `resources/platform/<platform>.ini` through `PlatformConfig`, which gives download commands and
      paths.
  - **The way back**: before exiting, an extension may write requests to `AB_EXTENSION_RESULT`, one per
    line. The launcher acts on the ones it knows and ignores the rest:
    - `rescan` (games were added or removed): `app.scans().requestScan()`;
    - `reload-apps` (the Apps set changed);
    - `reload-config` (config.ini changed, e.g. a language or theme);
    - `message <text>`: shown in the launcher's notification line.
  - **The exit code**: 0 is normal. Anything else makes the launcher show "<name> closed with an error"
    and log the code; the extension's own log is `System/Logs/<name>.log`.
  - A launcher shows an extension whose `Api` is newer than its own as greyed out: "Needs a newer
    AutoBleem".

### The launcher's side

- **`ExtensionService`** (ab_core, with tests) is the list:
  - it scans `Extensions/*/extension.ini` and resolves `Exec` for the platform;
  - it checks that the file exists and that `Api` and `Platforms` fit;
  - it reads the icon path and the translated name;
  - it plans the launch: argv, environment, result file.
  
  It goes through a `ProcessRunner`, as `LaunchService` does, so the tests record the launch and the dev
  host really runs the extension in its own window.
- **The launch** is `AutoBleem::launchGame()`'s preamble and epilogue, taken out and shared:
  1. Close the audio and flush the pads.
  2. Pause the scan watcher.
  3. `Gui::releaseDisplay()` when the runner needs the exclusive display. On the console and a Pi only one
     process can hold the display, so the extension must get it.
  4. Run the extension.
  5. `Gui::display(true)`, audio and music back, then act on the result file.
  
  The PSC-Bios special case in the Hardware Information item moves onto this path. Whether PSC-Bios and
  ABFlashKit themselves become extensions is an open question below.
- **`GuiExtensions`** (`evoui/screens/evoui_extensions.*`) is a *compact* panel by the UI standard, like
  the system menu:
  - one row per extension: its icon, name, and description (or why it cannot run);
  - Cross runs it, Circle goes back, L2/R2 page;
  - with nothing installed: "No extensions installed" and a line saying where they go.
- **The System menu** gets `SystemMenuAction::Extensions`, "Extensions" / "Run an installed extension",
  on every target. It is present even when the list is empty, which tells the user the feature exists.

### Trust

An extension is native code, and on the console it runs as root, like an App or a game's emulator. The
boundary is decision 3: only the user puts one on the stick. The launcher never downloads one, and the
Store never offers one. There is no sandbox, and the plan does not pretend otherwise.

## Steps

Each step is one commit, or a core commit plus a submodule bump, with its tests.

1. **Not done.** Core: `Env::getPathToExtensionsDir()`, `EnvironmentSetup::forExtension()`, and
   `ExtensionService` (the ini through `AppManifest` - the app format plan's step 1 comes first - plus
   `Api`, the plan, the result file's parser), with tests.
2. **Not done.** Core: the `ab_extension` library (`ExtensionApp`, `ExtensionResult`) and the
   `ab_add_extension()` CMake helper (build + zip). A sample extension, `hello`, in autobleem-core's
   `examples/`: one themed screen that writes `message Hello` back. It is the SDK's smoke test in CI.
3. **Not done.** The launcher: the shared launch preamble/epilogue (out of `launchGame()`),
   `GuiExtensions`, the System menu item, the result handling, the 16 languages. Walked through with
   `tools/ab_drive.py` on the Windows build with `hello` installed.
4. **Not done.** The dev layout: `tools/make_usb.py` gets an `Extensions/` folder, and the DebugDriver can
   drive an extension (`AppBase` already starts it for the console tools).
5. **Not done.** The first real extension: the **AutoBleem Store** (`docs/store-plan.md`) in its own
   repository (proposed `autobleem2/autobleem-store`, laid out like autobleem-console-tools), with
   packages for every target published on the site next to the launcher's.
6. **Not done.** The SDK documentation for extension authors: the folder, `extension.ini`, the protocol,
   `ab_add_extension`, building in the autobleem-build image, and the UI standard they draw by.
   Manuals: an "Extensions" section (where to unpack one, how to run it).
7. **Not done.** On hardware (the tester checklist): `hello` and the Store on the console, a Pi, the PC
   stick and Windows. Check the display handover in particular: Wayland on the console, KMS on a Pi.

## Open questions

- **PSC-Bios and ABFlashKit as extensions?** They are already built this way. They would move from
  `Apps/` to `Extensions/` and be run from the Extensions list. The Hardware Information item would keep
  running PSC-Bios directly, because users know it there. The launcher and the installers put these two
  on the stick today, which contradicts "installed by hand" only in spirit: they are part of the console
  package, not add-ons. The owner's call.
- **A package bundled with a release**: should the Store ship inside the launcher's packages (pre-unpacked
  into `Extensions/`), or only as a separate download? Decision 3 reads as a separate download. Bundling
  is a packaging choice that does not change the mechanism.
- **Translations**: whether an extension's `lang/` files fall under the rule that every string is
  translated into all 16 languages in the same commit. Proposed: yes, for extensions in the autobleem2
  organisation; a third party's are their own affair, and a missing string falls back to English.

## Later

- **A background companion**: an extension may declare `Background=<exec>` in `extension.ini`, a headless
  process the launcher starts after the extension exits.
  - It reports through a status file the launcher reads once a frame and shows in a `NotificationBubble`.
  - It is paused around a game launch.
  - This is what would let the Store's downloads continue in the carousel. It is not needed for the first
    version.
- Extension-provided System menu items or launcher hooks: not planned. The list is the one entry point.
