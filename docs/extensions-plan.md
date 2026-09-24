# Extensions (plan)

**Status (2026-09-24):** planned, nothing built. An **extension** is a **plugin**: a shared library
(`.so`, or `.dll` on Windows) that the launcher loads into its own process. It builds its screens with our
UI components in the user's theme, and may keep a service running in the background while the carousel is
showing. Extensions are copied onto the stick by hand and run from one place, the System menu's
Extensions list. The first one is the **AutoBleem Store** (`docs/store-plan.md`).

This is our answer to what Project Eris calls "mods". Nothing of theirs is used (autobleem-main's
`decisions.md`): the idea is general, and the design below is written from our own code.

## The decisions (the owner, 2026-09-24)

1. **An extension is a plugin, loaded into the launcher**, not a separate program. It uses the launcher's
   own copy of the SDK (autobleem-core: `lib_ableem`, `ab_core`, `ab_classic`), the launcher's window,
   renderer, input, audio and theme, and it can work in the background.
2. **One central place to run them**: the System menu's **Extensions** item opens a list of what is
   installed, and Cross runs one.
3. **Installed by hand**: the user unpacks an extension's package onto the stick (or the data partition,
   or the Windows data folder). Nothing installs or updates an extension for them, and that includes the
   Store. **Extensions are separate downloads**: none is bundled with a release package, the Store
   included.
4. **Every target**: an extension folder is multi-platform, with one library per platform key
   (`docs/app-format-plan.md`).
5. **The AutoBleem Store is the first extension.**
6. **PSC-Bios and ABFlashKit stay Apps.** The console needs both (pads and WiFi, the kernel), so they ship
   with the console package in `Apps/` as today. They are not extensions.
7. **Translations**: an extension in the autobleem2 organisation follows the 16-language rule, with its own
   `lang/` files. Anyone else's falls back to English for whatever string it lacks.

## What a plugin costs, and how the plan pays for it

A plugin is faster to switch to, shares the launcher's state, and can run in the background, which is
what the Store needs to keep downloading in the carousel. In exchange, three things have to be designed
in rather than hoped for:

| cost | answer |
|---|---|
| **The C++ ABI.** A plugin calls the launcher's C++ classes directly, so it must be compiled the same way: same compiler, standard library and class layouts. | An **SDK ABI stamp** each side checks before anything is called, a deliberately small **SDK surface** that the ABI promise covers, and a CI check that catches a layout change (below). |
| **One copy of everything.** A plugin carrying its own copy of the SDK would have its own `Gui` singleton, `Env` statics and `Lang`, which is two states in one process. | The plugin links **against the launcher**: the launcher exports its symbols, and the plugin binds to them at load time. The launcher stays one executable. |
| **A crash takes the launcher down.** | A **crash guard**: an extension that was running when the launcher died is disabled at the next start, and the user is told. |

### How a plugin links against the launcher

- **Linux** (psc, rpi, rpi64, pcusb):
  - `autobleem-gui` is linked with its symbols exported (CMake `ENABLE_EXPORTS`, which is `-rdynamic`).
  - A plugin is built with the SDK's headers and **left with undefined SDK symbols**. `dlopen` binds them
    to the executable's own.
  - A plugin's own third-party libraries sit in its folder's `lib/<key>/`, found through a `$ORIGIN`
    RUNPATH on the plugin. The no-RPATH rule is about the launcher's binary, which stays without one.
- **Windows** (win):
  - The executable exports its symbols (`--export-all-symbols` with MinGW, again `ENABLE_EXPORTS`) and
    produces an import library, `libautobleem-gui.dll.a`.
  - A plugin links against that library, and `LoadLibraryEx` looks for the plugin's own DLLs in its
    folder.
- **Two things to prove first (step 1):**
  1. **UPX.** The console and Pi binaries are packed with UPX (`make_psc.sh`, `tools/make_psc_package.sh`,
     `tools/make_rpi_package.sh`). A `dlopen`'d library binding to a packed executable's exported symbols
     has to be shown to work on the console. If it does not, the launcher is shipped unpacked (3 MB
     instead of 1 MB), or the SDK part moves into a shared `libautobleem-sdk.so` next to it.
  2. **Size.** Exporting every symbol grows the dynamic symbol table. The export list is limited to the
     SDK surface (a version script on Linux, a `.def` file on Windows) once that surface is fixed.

### The SDK surface and its ABI

- **The surface** is what a plugin may include: `autobleem-core/include/autobleem/sdk/`, a curated set of
  headers.
  - `GuiScreen`, `Gui` (`text()`, `assets()`, `panelStyle()`, the classic panel helpers).
  - `PanelStyle`, the list menus, `GuiConfirm`, `GuiKeyboard`, `GuiFactsPage`, `GuiActionMenu`,
    `GuiTextPage`, and the generic detail pane (from the Store plan).
  - `Config` (read), `Theme`, `Env`, `_()`/`Lang`, `System`, and the `PLOG_*` logging.
  - The engine helpers a plugin needs: `DirEntry`, `Strings`, `IniFile`, the archives, `Sha256`, the
    `Downloader` and installers from the Store plan.
  - `Extension` and `ExtensionHost` (below).
  
  Anything else in core stays private to the launcher.
- **`AB_SDK_ABI`** is an integer in core, and **the stamp** is `AB_SDK_ABI` plus the compiler family and
  major version, plus the target key.
  - The integer is bumped by any change to a surface class's layout or a surface function's signature.
    Adding a class or a free function does not bump it.
  - The plugin exports the stamp (`ab_extension_abi()`, a C function) and the launcher compares it with
    its own before calling anything else.
  - A mismatch leaves the extension greyed in the list ("Built for a different AutoBleem - needs an
    update from its author") and never loaded.
- **Keeping it**:
  - Surface classes added from now on hide their members behind a pointer to a private implementation, so
    growing them does not change their layout. The existing ones are moved over when they are touched.
  - A CI job compares the surface's ABI with the last release's (`abidiff` over the exported symbols on
    the native Linux build) and fails when the ABI changed but `AB_SDK_ABI` did not.
  - Every target is built in the autobleem-build image, so one release has one compiler per target, and a
    plugin built in the same image at the same core tag always matches.

### The interface

```cpp
// autobleem/sdk/extension.h
class ExtensionHost {                    // what the launcher offers a plugin
  public:
    virtual AppBase &app() = 0;          // config, theme, audio, the Gui
    virtual const std::string &folder() const = 0;     // Extensions/<name>/
    virtual const std::string &stateDir() const = 0;   // System/Extensions/<name>/
    virtual void requestRescan() = 0;    // games were added or removed
    virtual void reloadApps() = 0;       // the Apps set changed
    virtual void reloadConfig() = 0;     // config.ini changed (theme, language)
    virtual void notify(const std::string &title, const std::string &detail,
                        uint64_t done, uint64_t total) = 0;  // the launcher's NotificationBubble
    virtual void clearNotification() = 0;
    virtual bool networkUp() = 0;        // System::hasDefaultRoute() where it means something
};

class Extension {                        // what a plugin implements
  public:
    virtual ~Extension() = default;
    virtual void run() = 0;              // Extensions list -> Cross: shows its screens, returns when done
    virtual void poll() {}               // once a frame from GuiLauncher::loop(), when Background=true
    virtual void suspend() {}            // before a game launch: the display, audio and pads are about to go
    virtual void resume() {}             // after it, with the display back
    virtual void shutdown() {}           // before the launcher exits (power off, RetroArch): join threads, save
};

extern "C" AB_EXTENSION_EXPORT const char *ab_extension_abi();
extern "C" AB_EXTENSION_EXPORT Extension *ab_extension_create(ExtensionHost &host);
```

`AB_EXTENSION(MyExtension)` is a macro that writes both C functions, so an author writes only the class.
`ExtensionHost` is implemented by the launcher (`ab_ui`'s `App`), which is how an extension reaches the
scan and the Apps set without the SDK knowing what a launcher is.

### The life of a plugin

- **Loading**:
  - An extension with `Background=true` is loaded when the launcher starts, after the splash, so its
    `poll()` runs from the first frame.
  - Any other extension is loaded the first time it is run.
  - None is ever unloaded. Unloading a C++ library with static objects and threads is a known source of
    crashes, and the launcher's life is short enough.
- **`run()`** shows its screens as the launcher's own do: stack `GuiScreen`s built with `*gui`, closed with
  Circle. Then it returns to the list.
- **`poll()`** is called once a frame and must be cheap, like `ScanService::poll()`. Real work happens on
  the plugin's own threads, at the lowest OS priority (`System::lowerCurrentThreadPriority()`), with
  results handed over in `poll()`.
- **Game launches**:
  1. `AutoBleem::launchGame()` calls every loaded extension's `suspend()` before `Gui::releaseDisplay()`.
     The launcher's rule applies: **no `Texture` or `Font` kept across a launch**, so a plugin frees
     anything it drew with.
  2. Its threads pause, so the emulator gets the CPU and the USB bus.
  3. `resume()` follows `Gui::display(true)`.
- **`shutdown()`** runs before `AutoBleem::run()` returns, for every exit path (power off, RetroArch, an
  update).

### The crash guard

- Before calling into a plugin (`create`, `run`, and the first `poll` after a load), the launcher writes
  `System/Extensions/.active` naming it, and removes it afterwards.
- If the launcher starts and finds that file, the named extension crashed it last time. It is added to
  `System/Extensions/disabled.txt`, is not loaded, and the notification line says "<name> stopped
  AutoBleem and was disabled".
- The Extensions list shows it greyed with Triangle to enable it again.
- On the console a crash already brings the launcher back through `selection.sh` (a reboot), so a
  broken extension costs one restart, not a boot loop.

### The folder

`Extensions/<name>/`, next to `Apps/` and `Themes/` at the data root (`Env::getPathToExtensionsDir()`,
set by `EnvironmentSetup` like the others). The layout is the multi-platform format
(`docs/app-format-plan.md`):

```
Extensions/store/
    extension.ini
    icon.png                     the list's icon, 128x128, shared
    lang/                        its translations, <Language>.txt in the launcher's format, shared
    bin/psc/store.so             one library per platform key
    bin/rpi64/store.so
    bin/win/store.dll
    lib/<key>/                   optional: the plugin's own third-party libraries
```

`extension.ini`:

```ini
[extension]
Name=AutoBleem Store                 ; shown in the list (a lang/ file may translate it)
Description=Download apps and games
Author=AutoBleem team
Version=1.0.0
Plugin=bin/{key}/store               ; resolved by AppManifest's rule; .so / .dll is added per platform
Icon=icon.png
Background=true                      ; load at start-up and poll() every frame
```

The ABI stamp is read from the library itself, never from the ini, so a hand-edited ini cannot claim a
compatibility the binary does not have. An extension with no library for this machine is greyed:
"Not available for this system".

### The launcher's side

- **`ExtensionService`** (ab_core, with tests):
  - scans `Extensions/*/extension.ini`, resolving `Plugin` through `AppManifest`;
  - owns the loaded plugins, loading through a `PluginLoader` seam (`dlopen`/`LoadLibraryEx` for real, a
    table of in-process fakes in the tests);
  - checks the ABI stamp and runs the crash guard;
  - calls `poll`, `suspend`, `resume` and `shutdown` on each.
- **`GuiExtensions`** (`evoui/screens/evoui_extensions.*`) is a *compact* panel by the UI standard, like
  the system menu:
  - one row per extension: its icon, name, and description (or why it cannot run);
  - Cross runs it, Triangle enables a disabled one, Circle goes back, L2/R2 page;
  - with nothing installed: "No extensions installed" and a line saying where they go.
- **The System menu** gets `SystemMenuAction::Extensions`, "Extensions" / "Run an installed extension",
  on every target. It is present even when the list is empty, which tells the user the feature exists.
- **`App` implements `ExtensionHost`**:
  - `requestRescan()` is `scans().requestScan()`;
  - `notify()` feeds a second `NotificationBubble`, stacked under the scan's;
  - `reloadApps()` and `reloadConfig()` are flags `GuiLauncher` acts on after `run()` returns, or on the
    next frame for a background call.

### Trust

An extension is native code running inside the launcher, which on the console runs as root. The boundary
is decision 3: only the user puts one on the stick. The launcher never downloads one, and the Store never
offers one. There is no sandbox, and the plan does not pretend otherwise.

### For extension authors

- A release publishes **`autobleem-sdk-<target>-<v>.tar.gz`**: the surface headers, the `ab_add_extension`
  CMake helper, the ABI stamp, and on Windows the launcher's import library.
- An author builds in the autobleem-build image of the same channel (`:latest` for a release,
  `:develop` for a nightly). A mismatched build is refused at load time, not crashed.
- `ab_add_extension(<name> SOURCES ... ICON ... LANG ...)` builds the library, sets the RUNPATH and the
  export rules, and packs `<name>-<key>-<version>.zip` laid out as `Extensions/<name>/...` (one platform),
  or with `ALL_KEYS` all of them.

## Steps

Each step is one commit (a core commit plus a submodule bump where core changes), with its tests.

1. **Not done.** The proof, before any design lands:
   - a throwaway plugin that opens one `GuiConfirm`, `dlopen`'d by the launcher built with
     `ENABLE_EXPORTS`, run on the Windows dev build, on a Pi and on the console;
   - on the console and the Pi with the binary UPX-packed, too;
   - the size of the exported symbol table measured.
   
   The result decides the linking section: exports from the executable, or a shared SDK library.
2. **Not done.** Core: the SDK surface (`include/autobleem/sdk/`), `AB_SDK_ABI` and the stamp,
   `Extension`/`ExtensionHost`/`AB_EXTENSION`, `Env::getPathToExtensionsDir()`, `ExtensionService` with
   `PluginLoader` and the crash guard (tests with in-process fakes), and `ab_add_extension`. The multi-
   platform format's `AppManifest` (app format plan, step 1) comes first.
3. **Not done.** A sample extension, `hello`, in autobleem-core's `examples/`: one themed screen, and a
   background `poll()` that shows a notification. It is the SDK's smoke test in CI on every target.
4. **Not done.** The launcher: exported symbols (with the export list), `App` as `ExtensionHost`,
   `GuiExtensions`, the System menu item, `suspend`/`resume` around launches, `shutdown`, the 16
   languages. Walked through with `tools/ab_drive.py` on the Windows build with `hello` installed.
5. **Not done.** CI: the ABI check (`abidiff` against the last release) and the SDK package per target.
6. **Not done.** The first real extension, the **AutoBleem Store** (`docs/store-plan.md`), in its own
   repository (proposed `autobleem2/autobleem-store`), published as a separate download for every target.
7. **Not done.** Documentation for extension authors (the surface, the life of a plugin, the ABI rules,
   building in the image, the UI standard they draw by), and an "Extensions" section in the manuals.
8. **Not done.** On hardware (the tester checklist): `hello` and the Store on the console, a Pi, the PC
   stick and Windows. Check in particular the display handover around a game launch with a background
   extension loaded, and the crash guard.

## Open questions

- **The UPX result** (step 1) decides whether the console and Pi launchers stay packed.
- **How often `AB_SDK_ABI` may change.** Every bump means every extension needs a rebuild. Proposed: bumps
  only with a release, never within a release's nightlies once the release is tagged.

## Later

- Extension-provided System menu items, or launcher hooks beyond `poll`/`suspend`/`resume`: not planned.
  The list is the one entry point.
