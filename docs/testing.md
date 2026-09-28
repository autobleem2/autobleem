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
