# PSC-Bios - developer context

The console's hardware configuration tool, started by the launcher's *Hardware Information* item
(`SystemMenuAction::HardwareInfo` runs `Apps/pscbios/run.sh` on a PSC; a Pi or a PC gets the built-in
`GuiHardwareInfo` instead). One screen of facts - the time and timezone, the WiFi/ethernet/Bluetooth
dongles with their addresses, the pads and whether SDL has a mapping for each - and three things it can
do: **WiFi settings** (SSID typed or picked from a scan, password, driver mode, written for the kernel and
the network restarted; the timezone), the **gamepad mapping wizard** (a pad tested raw, every standard
input asked for in turn, the result written to the `gamecontrollerdb.txt` the launcher loads), and two
pages on DualShock 3 / Bluetooth pairing. Ported on 2026-09-18 from the 2020 standalone tool (a fork of the
old AutoBleem GUI, kept in git history under `psctools/pscbios`) onto `lib_ableem` + `ab_core` +
`ab_classic`; the root CLAUDE.md covers those and the build.

## What the console provides

Everything console-specific goes through two scripts the **AutoBleem kernel** ships in `/bin` - they are
not in this repository:

- `/bin/abnet list_ifaces | wlan_on | is_up <iface> | show_ip <iface> | scan | configure "<ssid>" "<pw>"
  | driver_mode <wext|nl80211> | restart | bt_up | bt_name`
- `/bin/settime tz` (the current zone) and `settime tzone "<zone>"`; the zone list is `timedatectl list-timezones`.

Plus two files: `/etc/autobleem/ssid.cfg` (three lines: SSID, password, driver mode - what `abnet
configure` is fed from) and `/etc/wpa_supplicant.conf` (read only, for the SSID an earlier setup left
there; its `"1"` means none). `/etc/autobleem` is `Env::getPathToKernelConfigDir()`, set by
`EnvironmentSetup::fromRoot()` on the console only; off the console `SsidConfig` keeps `ssid.cfg` next to
the tool. Without the kernel (`/bin/abnet` missing) the network rows and WiFi settings are off and the
pad wizard still works - the 2020 tool exited after "Custom Firmware Kernel Not Found".

## Layout

```
src/core/       pscbios_core (SDL-free, links ab_core; the tests link it)
  console_backend.*   ConsoleBackend interface; AbnetBackend (the popens above, through System::execUnixCommand[Lines])
                      and FakeBackend (a dev host: wlan0 on 192.168.1.23, three SSIDs, Europe/Warsaw, 16 zones)
  ssid_config.*       SsidConfig - ssid.cfg load/save, the wpa_supplicant.conf fallback, the paths
  game_controller_db.* GameControllerDb - gamecontrollerdb.txt with one mapping replaced under "#AutoBleem"
  pad_mapping.*       PadMapping - the wizard's logic: the 25 standard elements, detectChange(), the stick-half
                      merge (finalElements), the mapping line; the 2020 right-stick bug is fixed here
  network_status.*    NetworkStatus - the main screen's facts, one refresh() per RefreshInterval
src/screens/    the screens, on ab_classic (GuiFactsPage, GuiStringMenu, GuiConfirm, GuiKeyboard, GuiTextPage, GuiAbout)
  gui_pscbios_main.*  the opening screen, a GuiFactsPage (sections: time, WiFi, ethernet, Bluetooth, the controllers
                      with their mapping and the mapping file); Select = WiFi (kernel only), Square = gamepads,
                      Triangle = About, Circle = quit
  gui_network_menu.*  the WiFi settings: seven option rows (label left, value right - a compact panel), the two
                      actions among them; the restart's two messages are the busy spinner over the panel.
                      gui_ssid_scan_menu.* holds both pickers (SSID scan, timezone)
  gui_gamepad_menu.*  the gamepad section, a compact three-row list; gui_pad_config.* the wizard: the facts, the
                      stage's message and (while mapping) the entries in two columns down the left of the panel,
                      the DualShock picture at the right, the front buttons as RESET/OPEN/POWER chips in the footer;
                      pscbios_pages.* the static texts (the About credits with GuiAbout::HeadingMark headings)
src/pscbios_app.*     PscBios : AppBase - holds the ConsoleBackend; run() shows the main screen
src/main.cpp          EnvironmentSetup::forTool, the backend choice (FakeBackend under AB_DEBUG_HOST), logs
resources/            what ships in Apps/pscbios next to the binary: app.ini, run.sh, readme.txt, icon.png,
                      DS3.png (the wizard's picture), gamecontrollerdb.txt (the seed), lang/
```

`main()` → `EnvironmentSetup::forTool(argc, argv, "pscbios")`: the root is `/media` on the console (run.sh
passes nothing) or the argument on a dev host; the **working path** becomes the main GUI's
`Autobleem/bin/autobleem` (its `config.ini` gives the theme and the language, its `lang/` the shared
strings, its `themes/` fallback), the **app dir** stays the folder the tool was started from (run.sh `cd`s
to `/media/Apps/pscbios`) - `Env::getAppDir()` is where `DS3.png` and the tool's own `lang/` are read.

## Theme and language

The tool draws with whatever theme the main GUI's `config.ini` names, through `AppBase` (`Theme`,
`ThemeAssets`, `TextRenderer`) - no theme files of its own any more (the 2020 package carried an 11 MB
`sony/` tree and a `theme.ini`). `AppBase` loads the main GUI's language file, `PscBios`'s constructor
`Lang::loadMore()`s the tool's `lang/<Language>.txt` on top: the shared classic screens' strings come from
the main file, the tool's own from its file (an empty value there falls through to the main one).
`resources/lang/` is Key=Value; `English.txt` is generated from these sources:

```
python tools/lang_tools.py --src-dir apps/pscbios/src --lang-dir apps/pscbios/resources/lang extract
python tools/lang_tools.py --src-dir apps/pscbios/src --lang-dir apps/pscbios/resources/lang update
```
`make_win.sh` runs `validate` on it. Five translations came over from the 2020 tool (Italiano, Polski,
Portuguese_BR, Slovak, Spanish); about half their strings changed with the port and are empty.

## The wizard

`GuiPadConfig` opens one joystick **raw** (`ableem::Joystick`, after `Input::flushPads()` let go of the
pads; `probePads()` takes them back after) and shows its axes/buttons/hats as numbers plus the DS3 picture
lit by the mapped view. It is driven by the console's **front buttons** - they arrive as keyboard scancodes:
Power (`Key::Sleep`, Escape on a keyboard) cancels/exits, Reset (`Key::Reset`, or Start = Space with the
keyboard as pad) picks the next pad, Open (`Key::Open`, or Return) starts mapping / skips an input / saves.
`Input::setPowerKeyAsKey(true)` for the screen's duration is what turns the power button into a key. The
mapping: `initialState` is the pad at rest; every frame `PadMapping::detectChange()` names the raw input
that moved; the screen waits for it to be released (`anythingHeld`) and moves on. At the end
`finalElements()` merges the stick halves, the line is given to SDL (`Input::addMapping`) for a test, and
Open once more asks for a name and writes it with `GameControllerDb` to `Input::currentMappingPath()` -
the file `probePads()` loaded, i.e. the kernel's `/etc/autobleem/gamecontrollerdb.txt` when it exists,
else the main GUI's `gamecontrollerdb.txt` - which the launcher loads at its next start (`Env::padMappingFiles()`).

## Build, run, test

Part of the root build: `make_win.sh` builds `pscbios.exe` (target `pscbios`, `apps/pscbios/`), runs
`tests/apps/test_pscbios_core.cpp` under ctest, validates the language files, formats and lints `apps/`.
`make_psc.sh` builds it for the console next to the launcher, runs `check_psc_binary.sh` and UPX on it and
copies the binary plus `resources/` into `payload/Apps/pscbios/`. Not built for the Pi.

Visual test on Windows: `python tools/make_usb.py usb` stages `usb/Apps/pscbios/` (resources + the exe);
`python tools/ab_drive.py start --tool pscbios` then `run "press select; wait_screen GuiNetworkMenu; shot
a.png"` drives it through the DebugDriver (AppBase starts it for every program since 2026-09-21; the
screen names are `GuiPscBiosMain`, `GuiNetworkMenu`, `GuiSsidScanMenu`, `GuiTimezoneSelect`,
`GuiGamepadMenu`, `GuiPadConfig`, `GuiTextPage`, `GuiAbout`); logs in `usb/System/Logs/pscbios.log`. The FakeBackend answers everything; a
`configure`/`restart`/`setTimezone` is logged and reflected in what it then reports. The wizard needs a
real pad (the keyboard-as-pad is not a joystick) - it shows "NO GAME CONTROLLERS OPENED" without one.

## Behaviour changes vs the 2020 tool

- No exit without the kernel; the pads can still be tested and mapped.
- The network facts are refreshed every 2 s by the clock, not every 75 frames (8 popens a frame-count).
- Titles are decorated once (`-=...=-`, not twice); the main screen returns instead of recursing.
- The right-stick merge checked the left stick's values (`gui_padconfig.cpp:132-139`); fixed, tested.
- The mapping is saved to the file the launcher loads (`Input::currentMappingPath()`), never to the
  tool's own folder, and the launcher actually loads it now (it never called `loadMappings` before).
- Dead code gone: `bluetool`, the BT menu, `cfgprocessor`, `DebugTimer`, the theme/sony copies.
- The shared look (2026-09-21, the root CLAUDE.md's "UI styling standards"): the facts page, compact
  option lists with the values at the right edge, chips for the front buttons, Back everywhere leaving
  loses nothing; the gamepad menu's copy of the controller list is gone (the opening screen has it).
