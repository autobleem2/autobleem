# Testing the launcher's UI

Full detail for what CLAUDE.md's "Smoke test layout" section only points to: the DebugDriver, `ra_drive.py`
(RetroArch's own network command interface), and LAN testing. Moved here from CLAUDE.md (DOCS-1, ~2026-09-27)
to keep the developer-facing file terse; nothing here changed behaviour.

## The DebugDriver

`lib_ableem/include/ableem/ui/debug_driver.h` is how the UI is tested: a dev build started with
`AB_DEBUG_PORT=<port>` (and `AB_NO_SPLASH=1`) - `AppBase` starts it, so the console tools take it too
(`ab_drive.py start --tool abflashkit`; PSC-Bios is an extension, reached through the launcher) - takes
pad/keyboard input and hands frames back over a socket - `press x`, `down l2`, `key escape`, `text abc`,
`shot a.png`, `screen` (the class name of the screen showing, from `GuiScreen::show`'s stack), `window
hide|show`. `tools/ab_drive.py start|run|sheet|stop` is the client (`run "menu 6; wait_screen GuiOptions; shot
a.png"`); a whole walk through the screens takes seconds, with the window hidden. `win_drive.ps1` is the old
way, kept for a keyboard-only smoke test.

### Shots, clips and waiting

- **`shot <file>`** always shows a frame drawn after the command and after the last input - never an older one.
  The driver waits up to 5 s for it and answers `err no frame` if none came (a screen that has hung). A relative
  path goes under `AB_DEBUG_OUT` when that is set (a sandbox sets it to its own folder), otherwise under the
  program's working directory. The reply names the full path. `grab` is the same frame, sent back over the
  socket.
- **`clip start <name>` ... `clip stop`** records what the launcher shows, 25 samples a second, into the folder
  `<name>` (a `.mp4` on the name is dropped; the folder goes where a shot would). A sample is written as the next
  PNG only when the picture changed, and `clip stop` writes `clip.ffconcat`, which gives every frame its time.
  `ab_drive.py` then makes `<name>.mp4` with ffmpeg (if ffmpeg is on the PATH) and removes the frames:
  `ab_drive.py run "clip start menu.mp4; menu options; wait_idle 500; clip stop"`. By hand:
  `ffmpeg -f concat -safe 0 -i <name>/clip.ffconcat -vf fps=25,format=yuv420p <name>.mp4`. A clip ends by itself
  after 10 minutes, or when the connection that started it closes.
- **Frames are read back only when asked.** Reading a frame back from the GPU costs a few milliseconds (much more
  in the VM's software renderer), so the renderer copies a frame only while a shot, a grab, a clip or a wait needs
  one. An idle launcher with the driver on costs nothing extra.
- **`wait_screen <Name> [seconds]`** waits until that screen shows (15 s by default). **`wait_idle <ms>
  [seconds]`** waits until the picture has rested for that long: either the screen reports that only its looping
  decorations move (the play button, the arrow - the frame pacer's "ambient" state), or two frames that far apart
  are the same. Use these instead of `wait <ms>`: a script is then as fast as the machine and does not break on a
  slow one.
- **`busy`** answers `ok 1` while a busy spinner shows (`Gui::beginBusy` - "Applying settings..." right after
  Options closes, deleting a game, ...), else `ok 0`. **`wait_ready [seconds]`** waits until it is `0` AND the
  picture has rested ~300 ms (`wait_idle`'s test), 10 s by default; on a timeout it answers `err not ready after
  <s> s (screen <Name>, busy 0|1)`. While the spinner shows every press is dropped (and flushed at its end), yet
  `screen` already says `GuiLauncher` - so after anything that closes Options, `wait_ready` before the next press.
  `screen`'s reply is unchanged.
- **`items`** lists the rows the showing screen published, `|`-separated; **`selected`** answers where the cursor
  is: `ok <index>|<name>` (0-based into `items`; `ok -1|` when the screen published none; the name is empty
  past the end). Published by: the System/Quick menu (English keys, no headings), the classic lists - Options,
  Game Manager, the game editors, Memory Cards, the USB/RetroArch pickers (the row text as displayed, so
  **translated**; a heading row is included with a leading `#`, e.g. `#Display`, so an index matches what is
  drawn, and the cursor never rests on one) - and the launcher's set picker (the tab showing), Extensions and Scanner
  processors (titles as displayed). A screen's rows are its own: a dialog over a list has none until it
  publishes, and the list's come back when the dialog closes. Hardware Information has no cursor and publishes
  nothing.
- **`quit`** makes the program leave the way a power off does: every screen closes, the databases close, the
  process ends. So does **SIGTERM** (and SIGINT) since 2026-09-28 - `kill <pid>` or `systemctl stop` is a clean
  stop now, on every target. Before, SDL turned the signal into a window-close event, which the launcher (off a
  dev machine) took for a lost display: it rebuilt the display and carried on, so it had to be killed. Leaving
  this way writes no selection file, so on the console `rc/selection.sh` treats it as a stop, not a choice. The
  console's power button works as before.

### The client's own steps (`ab_drive.py`, also in `abvm.py drive` / `sandbox drive`)

These are done by the client, on top of the driver's words:

- **`menu <item>`** (the L2+R2 System menu) and **`quick <item>`** (d-pad Up, the Quick menu) first wait for
  `GuiLauncher`, then for the picture to rest (`wait_idle 300 10` - right after Options or Game Manager close the
  launcher shows `GuiLauncher` while a busy spinner still ignores all input), then hold the chord for real (`down
  l2`, `down r2`, 150 ms, `up r2`, `up l2`; `down up` ... `up up` for `quick`) and wait up to 2 s for
  `GuiSystemMenu`; the whole thing is tried up to 3 times. After the pick they wait (up to 10 s) until the System
  menu is gone, so the chosen screen is up when the step ends and a script needs no `wait_screen GuiManager`.
  `<item>` is a title (the English key) or an index. The cursor is walked there **closed-loop**: the client reads
  the driver's `selected` after every step and presses again until it is on the row - a double move on a slow
  frame is walked back instead of picking the wrong item (a driver without `selected` falls back to counting).
- **`select <row>`** moves the cursor of the list showing (Options, Game Manager, an editor, the set picker,
  Extensions, Scanner processors) to a row by its text as shown - translated, case-insensitive, a unique prefix -
  or by its index (headings count, `#...`); nothing is pressed, so `select Language; tap right` changes that row.
- **`tap <btn>`** = one short press (`press <btn> 60`; `tap <btn> <ms>` a longer one) - `tap down` moves exactly one
  row. **`hold <btn>`** / **`release <btn>`** = the driver's `down` / `up` (`hold <btn> <ms>` = `press <btn> <ms>`).
  **`dpad <up|down|left|right>`** and a bare **`up`/`down`/`left`/`right`** are a tap too, `dpad center` does
  nothing - `dpad down; wait 200; dpad center` no longer lets the key repeat fire and skip a row. Only the logical
  names (`x o s t start select l1 r1 l2 r2` and the d-pad) are taken; `@1 tap a` and the other padsim words go to the
  driver as before. (In `abvm.py run` these words are padsim's own, not the client's.)
- **`home`** presses Circle until `GuiLauncher` shows (at most 6 presses) - the way out of Game Manager or Options
  that an earlier run left open; a script can start with it.
- **A failed run keeps its shots.** A failing step stops the run, but the replies of the steps before it are
  printed, the shots and grabs they made are written (and, in `sandbox drive` / `run`, still brought back to
  `--out`), then one `error: step N '<step>' failed: ...` line names the step and the exit code is non-zero.

### Virtual pads (padsim's words)

The driver can make up to four pads inside the program, with the same commands as the test VM's padsim
(`tools/vm/padsim.c`), so one script tests the pad logic in a sandbox and in the VM. It needs SDL 2.24 or newer
(the PC stick, a dev machine; not the console, whose SDL is older - there the commands answer `err`).

| Command | What it does |
|---|---|
| `@2 profile ds4 bt` | pad 2 becomes a Bluetooth DualShock 4 (plugged in again as that pad). Profiles: `x360` (wired), `ds4` (`usb` or `bt`), `generic` (a pad SDL has no mapping for - what starts a mapping wizard) |
| `@2 plug`, `@2 unplug` | the cable in or out (a Bluetooth pad: switched on or off) |
| `@1 press a`, `@1 release a` | a button down or up. Xbox names on every profile: `a b x y` are Cross, Circle, Square, Triangle on a DualShock; also `l1 r1 l2 r2 select start guide l3 r3` |
| `@1 tap a [ms]`, `@1 hold a <ms>` | down, then up after 120 ms (or ms) |
| `@1 stick left <x> <y>` | a stick, -32768..32767 each |
| `@1 trigger r2 <0..255>` | a trigger |
| `@1 dpad down`, `@1 dpad up-left`, `@1 dpad center` | the d-pad |
| `@1 reset` | everything released and centred |
| `@1 battery 20`, `@1 battery off` | the pad's battery level (not on an x360, which is wired) |
| `@1 cable in`, `@1 cable out` | a charging cable: "Charging", "Full" at 100, or back on the battery |

`@n` picks the pad (1-4); without it the command goes to pad 1. Pad 1 is plugged in as an x360 by its first
command; pads 2-4 wait for `plug` or `profile`. **Always write the `@n` in a script meant for both the sandbox and
the VM**: `press x` without it is the driver's older logical press (a Cross, down and up), not padsim's.

The pads have the real pads' vendor and product ids and names. One difference from padsim: SDL marks a pad made
inside the program as "virtual" in its GUID, so SDL's database line for the real pad does not apply - the x360 and
the DualShock get SDL's own standard mapping instead (the same buttons), and the generic pad gets none, like an
unknown pad. A battery is a folder like the Sony driver's (`ps-controller-battery-aa:bb:cc:00:ab:0<n>` with
`capacity`, `status`, `type`, `scope`) under `AB_PAD_BATTERY_DIR`, where the launcher reads pad batteries from;
without that variable `battery` answers `err`.

The keyboard: `key`/`text` as before, plus padsim's words - `kbd tap enter`, `kbd press shift`, `kbd combo
ctrl+alt+delete` (the modifiers held with the last key), `kbd type Hello` (as typed text).

**`AB_INPUT_ISOLATED=1`** makes the program ignore the machine's own input devices: no keyboard, mouse or text
events, no real pad opened or listed (the raw joystick API too), and the keyboard counts as present only once the
driver typed something. Only the driver drives it. A sandbox sets it, so padsim's pads - the VM's hardware test -
never reach a sandbox.

`AB_WINDOW_SIZE=1280x720` makes the window that size and never full screen. SDL's `offscreen` driver (a headless
sandbox) otherwise reports a 1024x768 desktop, and the launcher drew 1024x576 with black bars.

`AB_SHOT=<file%d.bmp>` has the launcher save the frame it presents every 3 s - the way to look at a Pi's
screen over ssh (a systemd drop-in `Environment=AB_SHOT=/tmp/ab%%d.bmp` on `autobleem.service`, removed
afterwards: 8 MB a frame into tmpfs) or at a PC whose screen is in use; pcsx-abnxt has the same as
`PLAT_SDL2_SHOT`. The resume-slot picker on the PC needs a clean return from a game (`filename.txt` in the
game's `!SaveStates`), which the splash runner simulates when the file is there.

### The keyboard

Every screen driven by the pad works from a keyboard, on every platform - a PC stick, Windows, a Pi, a USB
keyboard on the console. `ableem::Input` applies `lib_ableem/include/ableem/ui/keyboard_map.h` (header-only,
tested in `test_keyboard`) to every key event:

| Key | Pad | | Key | Pad |
|---|---|---|---|---|
| Arrows | d-pad | | F1 | Select |
| Enter (and keypad Enter) | Cross | | F2 | Start |
| Backspace, Esc | Circle | | Page Up / Page Down | L1 / R1 |
| Tab | Triangle | | Home / End | L2 / R2 |
| Space | Square | | F10 | L2+R2 (the launcher's System menu) |

A held key's repeats are swallowed (the screens have their own hold logic). The power button
(`SDL_SCANCODE_SLEEP`) and the console's Reset/Open keys are not in the map and behave as before. A screen
that takes typed text turns the map off for its own duration (`GuiKeyboard`: `setKeyboardAsPad(false)` +
`setRawKeyboard(true)`, restored on close) - there Enter, Esc, Backspace and the arrows are the text field's.
**On a dev host** the old letter map stays alongside: `X O S T` = cross/circle/square/triangle, `I J K L` =
d-pad, `Space` = Start, `B` = Select, `Q E 1 2` = L1 R1 L2 R2. It owns Space (Start, not Square), and `Esc`
stays the power off (exits) - Backspace is Circle there; the two maps share no other key. The DebugDriver's
`key` command goes through the map as a real key does (`key f10` opens the System menu), so a script can test
it. `tools/win_drive.ps1 -Usb <usb> -Sequence "x;5;space;8"` starts the exe, posts letter-map keys to its
window, screenshots after each, and collects the logs.

**Keyboard presence** (`Input::keyboardPresent()`, what the Button Guide shows the keyboard column by): a key
seen this session, or `ableem::KeyboardPresence::detect()` (`engine/keyboard_presence.*`) - on Linux every
`/sys/class/input/eventN/device/capabilities/key` bitmap with Enter and at least 20 letters (a pad's `BTN_*`,
the console's power/reset buttons and a number pad are not keyboards; the word size, 32 or 64 bits, is told
from the text, since a 32-bit userland on a 64-bit kernel cannot know the kernel's), on Windows
`GetRawInputDeviceList`'s `RIM_TYPEKEYBOARD`. Asked afresh each time, so a keyboard plugged in later counts.

## LAN testing

The driver can listen on a non-loopback address for a trusted LAN test rig. On the launcher, set
`AB_DEBUG_BIND=<IPv4>` (default 127.0.0.1) and `AB_DEBUG_TOKEN=<token>` - the driver refuses to start without
a token when the bind is not loopback, and the client must send `auth <token>` on its first line (compared in
constant time, never logged). **Prefer `AB_DEBUG_TOKEN` in the environment over `--token` on the command
line** - a command-line argument sits in the process list (`ps`/Task Manager) for anything else on the same
machine to read; the env var does not. A peer has `DebugDriver::AuthTimeoutMs` (5 s) to send that first line
and it may not exceed `DebugDriver::MaxAuthLine` (4 KB) - past either, the connection is dropped as if it had
closed, so one client that never authenticates cannot tie up the driver, which serves one connection at a
time. Neither limit applies once `auth` succeeds: an authenticated session reads with no timeout, since real
commands in a script can be minutes apart. The script reaches it with `--host <addr>` (default 127.0.0.1) and
`--token <t>` (or env `AB_DEBUG_TOKEN`, preferred as above) - a refused `auth` is reported with the token
redacted (`'auth ***': err auth`), never echoed. Command `grab`: replies `ok <n>` followed by exactly n bytes
of PNG data - a test fetches screenshots over the socket without writing to the device.

Example: start the launcher on the device with `AB_DEBUG_PORT=<port>`, `AB_DEBUG_BIND=<its LAN IP>`,
`AB_DEBUG_TOKEN=<token>`; then from a PC run
`AB_DEBUG_TOKEN=<token> python tools/ab_drive.py run "..." --host <ip> --port <p>`.

**Security note:** the token travels in plain text over TCP - use this only on a trusted LAN test rig with a
token that is not a real credential; `AB_DEBUG_BIND=0.0.0.0` listens on every interface.

`start` (always local, no `--host`) is the one exception to all of the above: it drops an inherited
`AB_DEBUG_BIND` from the launch it starts, since a non-loopback bind would leave nothing listening on the
127.0.0.1 it always talks to - but it honours an inherited `AB_DEBUG_TOKEN`, authenticating its own readiness
commands with it, so a leftover `AB_DEBUG_TOKEN` from testing a device in the same shell does not break
`start`, and a `stop`/`screen`/... run straight after keeps matching it with no `--token` of its own.

A peer that authenticates and then drops the connection mid-command (Ctrl-C, a WiFi drop) never crashes the
driver: every socket send uses `MSG_NOSIGNAL` on POSIX (Windows has no `SIGPIPE` to raise) and a failed send
just closes that client and goes back to accepting the next one.

## `tools/vm/` - a test VM with virtual pads and a keyboard

`tools/vm/abvm.py` drives the PC-USB test VM from a dev PC: installs a build into it (and restores the
original), restarts the launcher, sends pad and keyboard steps, takes whole-screen shots and MP4 clips, and
runs `ab_drive.py` scripts through a tunnel. The VM is shared: take its lease first (`ABVM_WHO=<name>`,
`abvm.py lock take <task>`); a busy VM is exit code 3. `tools/vm/padsim.c` is the guest's side - up to four
virtual pads (x360, DualShock 4 over USB or Bluetooth, an unmapped one) with hot-plug, battery and charging,
and a USB keyboard - for everything that reads a real input device (PSC-Bios's wizard, the emulators, the
Apps). Their docstrings are the reference; autobleem-main's `docs/pc-test-machine.md` describes the machine.

**Sandboxes** (`abvm.py sandbox ...`): while someone holds the VM for a hardware test, the application can still be
tested - extra launchers run headless in the same VM, each with its whole root in a folder on the test machine's
disk (never the stick), their own DebugDriver port and their own lease. `sandbox start <name> --build
~/src/autobleem/dist/pcusb` lays a fresh pcusb build over the template, `sandbox drive <name> "<ab_drive script>"
--out DIR` drives it and brings the shots, grabs and clips back (also from a run that failed halfway), `sandbox
reset` starts it afresh in under a second. `--ext <zip or dir>` (repeatable, on `new`/`start`/`reset`) lays an
extension over the sandbox as well - a zip is unzipped at the root (the extension zips hold `Extensions/<name>/...`),
a directory `.../extensions/<name>/` is copied to `Extensions/<name>/`; what it replaces is kept once in
`<sandbox>/.abvm/ext-backup/<name>`. E.g. `--ext ~/src/ext_store-theme/dist/ext_store-pcusb-1.0.1.zip`.
`sandbox release <name>` stops a launcher its holder left running (the same clean quit as `sandbox stop`) before it
gives the lease back - a forgotten launcher takes a whole CPU and a place under the two-sandbox cap; a release
refused because someone else holds the sandbox touches nothing. When the cap is full, `sandbox start` first stops
the launchers whose lease is gone (expired - their holder died - or released without a stop) and only then refuses.

A sandbox runs with `AB_INPUT_ISOLATED=1` (the VM's own pads and keyboard never reach it), `AB_WINDOW_SIZE`
(1280x720, `--size WxH` or `ABVM_SANDBOX_SIZE` for another), `AB_MAX_FPS=30 AB_AMBIENT_FPS=5` (the VM draws in software, every frame costs CPU: an idle launcher's
ambient frames - the Play pulse, the arrow - drop from 30 to 5 a second, about 160% CPU -> 30%, while input and
animations stay at 30; a walk step right after a scroll may need a slightly longer wait), and its outputs in its own folder:
`AB_DEBUG_OUT=<sandbox>/.abvm/out` and `AB_PAD_BATTERY_DIR=<sandbox>/.abvm/power_supply`. Since the sandbox's root
is a folder on the test machine, the launcher writes a `shot` or a clip's frames straight onto the test machine's
disk; `sandbox drive` gives each run its own `.abvm/out/<run>/`, turns clips into MP4 there with ffmpeg and copies
the folder to `--out`. `start` returns once the launcher screen shows, so the first shot of a script is of it.
`stop` sends the driver's `quit`, then SIGTERM, and kills only if neither worked.

The script language is the same in `run` (the VM) and `sandbox drive`: padsim's pad words (with `@n`), `wait`,
`shot`, `clip start`/`clip stop`, and the driver's own words (`wait_screen`, `wait_idle`, `screen`, `key`, `text`,
`grab`, `menu`, `quick`, ...). In `run`, pad words go to padsim, the driver's words to the stick's launcher, and
`shot`/`clip` record the VM's whole screen; in a sandbox the sandbox launcher's driver does all of it. Example,
the same in both:

```
python tools/vm/abvm.py sandbox drive sb1 "@1 profile ds4; @1 tap a; wait_screen GuiLauncher; @1 dpad right; @1 dpad center; wait_idle 300; shot next.png" --out out
```

## `tools/ra_drive.py` - driving RetroArch itself

`tools/ra_drive.py` is the DebugDriver's counterpart for **RetroArch itself** - not the launcher's own
screens, which the DebugDriver already covers, but the RetroArch UI a game's Square/RetroArch option or the
system menu's "RetroArch" item hands off to. It drives RetroArch 1.22.2's own network command interface
(`command.h`/`command.c`, UDP, `network_cmd_port` - 55355 by default) rather than a socket of ours:
`press up|down|left|right|a|b|toggle` (`MENU_*`), `wait_status PAUSED|PLAYING|CONTENTLESS` (`GET_STATUS` -
RetroArch has no per-menu `screen` reply, only playing/paused/no-content), `shot`/`wait_shot <ref.png>`
(`SCREENSHOT` has no reply either, so this polls `screenshot_directory` for a new file, then average-hashes
it against a reference PNG - the practical "wait for this menu" substitute), `wait_log <pattern>` (greps
`retroarch.log`, `log_to_file`), `menu <n>`, and any exact command name bare (`QUIT`, `RESET`,
`LOAD_STATE_SLOT 2`, ...), checked against the confirmed `map[]`/`action_map[]` tables before it is sent.
`start` writes a throwaway `retroarch.cfg` (`network_cmd_enable`, `video_driver=gl`, `audio_driver=null`,
`log_to_file`+`log_dir`+`log_to_file_timestamp=false` for a predictable log path, `screenshot_directory`) and
launches RetroArch under Xvfb with `LIBGL_ALWAYS_SOFTWARE=1` - **Linux only**: run it on the Debian test
machine over ssh (there is no Windows RetroArch in this project, and none is fetched to a dev PC to run this;
see "Where this runs" in the tool's own docstring).

**Menu state is a capture of the Xvfb display, not a RetroArch screenshot**: RetroArch 1.22.2 logs nothing
for a menu toggle/transition, and its `SCREENSHOT` command does not capture the RGUI/ozone menu overlay
either (only the content's own rendered frame). `shot_display <file>` (`--cfg DIR` required, local only)
captures the Xvfb display `start` is running on directly with `xwd -root` - the whole framebuffer as the X
server drew it, menu included, independent of RetroArch entirely. `start` records the display number (parsed
from `xvfb-run`'s own `-e` diagnostic output) and a pinned `Xauthority` path next to the pidfile; a small
pure-Python XWD decoder (`xwd_to_rgb_rows`, file_version 7 ZPixmap, 24/32bpp) converts to PNG via the same
Pillow `wait_shot` already needs, no netpbm/ImageMagick dependency in the shipped tool. Needs `xwd` (Debian's
`x11-apps`) on PATH.

`tools/test_ra_drive.py` covers the protocol, the log-tail parser, the script parser and the XWD decoder
offline against a fake UDP server / synthetic files standing in for RetroArch and `xwd` - no RetroArch binary
or `xwd` needed to run those.

### Four confirmed upstream RetroArch 1.22.2 issues

All found while building `ra_drive.py` and all worked around in the tool itself, never in this project's own
code:

1. `video_driver=sdl2` segfaults the official AppImage 100% of the time under Xvfb -
   `XScreenSaverQueryExtension()` in its bundled `libXss.so.1`/`libXext.so.6` crashes inside the host's
   `libX11.so.6` (an Xlib extension-registration ABI mismatch), independent of the core or the Xvfb screen
   size. `gl`/`glcore` never crash - `CFG_TEMPLATE` stays on `gl`.
2. `GET_STATUS` segfaults the instant it is answered while any core is actively running - gdb-confirmed as
   the same crash address inside RetroArch's own binary with three unrelated cores, independent of pause
   state. `cmd_start`'s own readiness poll uses `VERSION` instead, and a script must never send
   `GET_STATUS`/`wait_status` against a running instance either.
3. Without `--verbose` on the command line, `retroarch.log` is opened but nothing is ever written into it
   past the startup banner, no matter how long the instance runs - `cmd_start` always passes it.
4. `QUIT` does not reliably exit the process under Xvfb (confirmed: a fresh instance sent bare `QUIT` was
   still running 5+ seconds later) - `stop` sends it anyway (in case a future build honours it) but relies on
   `os.killpg()` on the process group `cmd_start` creates with `start_new_session=True` - a bare PID kill only
   reaches `xvfb-run`'s own wrapper shell, never the `Xvfb`/`retroarch` children it spawns.
