#!/usr/bin/env python3
"""Drives the launcher through its DebugDriver (lib_ableem/include/ableem/ui/debug_driver.h) - a pad, a
keyboard and a screenshot over a socket, for automated looks at the UI without touching the user's screen.

  python tools/ab_drive.py start [--usb DIR] [--port N] [--show]   start the dev build on the usb tree with
                                                                   AB_DEBUG_PORT and no splash; AB_HEADLESS=1
                                                                   by default (SDL_WINDOW_HIDDEN from the
                                                                   first frame, dummy audio, no focus taken)
                                                                   - --show is the only way to get a visible
                                                                   window
                                 [--tool abflashkit]                ...a console tool instead (from
                                                                   usb/Apps/<tool>, staged by make_usb.py)
  python tools/ab_drive.py stop                                    a Quit event, then the process is killed
  python tools/ab_drive.py run "<script>"                          commands separated by ';', e.g.
                                   "down l2; press r2; up l2; wait 300; shot menu.png; press o"
  python tools/ab_drive.py run --file SCRIPT.txt                   the same, one command per line instead -
                                   easier to author/read for a long walk; '#'-comments and blank lines
                                   dropped, lines joined with ';' the same as an inline script
  python tools/ab_drive.py <command> ...                           one command, e.g. `shot a.png`, `press x`
  python tools/ab_drive.py sheet OUT.png IN1.png IN2.png ...       a contact sheet of shots (Pillow)

Every command but `start`/`stop`/`sheet` takes `--host <address>` (default 127.0.0.1) to reach a driver on
another machine - a Pi 400 or a PSC with AB_DEBUG_BIND set - and `--token <t>` (or the AB_DEBUG_TOKEN
environment variable) when that driver was started with AB_DEBUG_TOKEN: sent as the connection's first
`auth <token>` command, before anything else. `start` always launches on this PC (127.0.0.1 only, no
token needed) - driving a device is `run`/single commands with `--host` against a copy already running there.
`start` always talks to its own launch on 127.0.0.1 (never --host), so it drops AB_DEBUG_BIND from the
launched process's environment even if it is set in yours (e.g. left over from driving a device in the same
shell): a bind to one specific non-loopback address would leave nothing listening on 127.0.0.1 at all. A set
AB_DEBUG_TOKEN, though, is left as inherited and honoured: "set = required" applies on loopback too, so
`start` authenticates its own readiness commands (window hide, the wait for the first screen) with it, and
a `stop`/`screen`/... run straight after in the same shell keeps working with no --token of its own, exactly
as if AB_DEBUG_TOKEN were unset throughout.

The script language is the driver's (debug_driver.h has the full list; docs/testing.md explains it), and
tools/vm/abvm.py's `run` and `sandbox drive` take the same: press <btn> [ms], down/up <btn>, key <name>,
text <utf8>, wait <ms>, shot <file>, grab <local file>, clip start <name> / clip stop, frames, screen,
wait_idle <ms> [s], window hide|show|min|restore, quit; padsim's pad words for the driver's virtual pads
(`@1 profile ds4 bt`, `@1 tap a`, `@2 stick left 0 -32768`, `@1 dpad down`, `@1 battery 20`, `@1 cable in`, ...
- always with the @n in a script meant for both: a bare `press x` is the driver's logical Cross) and its
keyboard words (`kbd tap enter`, `kbd combo ctrl+c`, `kbd type abc`). Buttons of `press`: x o s t start
select l1 r1 l2 r2 up down left right. Client-side single steps (logical button names only): `tap <btn>` = one
short press (`press <btn> 40`, so `tap down` moves exactly one row; `tap <btn> <ms>` a longer one), `hold <btn>` /
`release <btn>` = the driver's `down`/`up` (`hold <btn> <ms>` = `press <btn> <ms>`), `dpad <up|down|left|right>` =
one short press, `dpad center` does nothing, and a bare `up`/`down`/`left`/`right` is a tap; `@1 tap a` and the other
padsim words still go to the driver. `home` presses Circle until GuiLauncher shows (max 6) - a script can start with
it. A failing step stops the run but keeps what was done: the replies so far are printed (shots and grabs are
already written), then one `error: step N '...' failed: ...` line on stderr and exit code 1. A `shot`/`grab` waits for a frame drawn after the last input, so
"press x; grab a.png" shows the result of the press. `shot` writes the frame on the machine running the
launcher (its path, relative to that process's cwd - or its AB_DEBUG_OUT); `grab` instead reads the frame back
over the socket and saves it at the local path given here (made absolute) - the way to get a screenshot off a
device without writing to its own storage. `clip start a.mp4 ... clip stop` records the frames the launcher
presents and, with ffmpeg on PATH, makes a.mp4 of them. Two of the client's own: `wait_screen <Name>
[timeout s]` polls `screen`
until that screen shows (a GuiScreen class name: GuiLauncher, GuiOptions, GuiConfirm, GuiSystemMenu, ...) -
`start` waits for GuiLauncher itself, so a script may press at once - `menu <item>` opens the L2+R2 System
menu and picks an item, and `quick <item>` the same from the Quick menu (d-pad Up in the launcher). Each first waits for GuiLauncher and a settled picture (`wait_idle`), holds the chord for real, waits
for the menu (up to 3 tries) and, after the pick, until the System menu is gone - the chosen screen is up when the
step ends. <item>
is the item's English title, in any language the launcher shows (`menu "Hardware Information"`, `menu
options`; case does not matter, quotes are optional, a unique prefix will do - `menu hard`), or a 0-based
index counting items only (headings are not counted). The names come from the driver's `items` reply, which
lists what the menu shows on this machine (Network & Controllers only where an extension provides it).

The launcher is started from a copy of build_win/'s exe (autobleem-gui-drive.exe, next to the resources
tools/make_usb.py staged), so the owner's own instance of autobleem-gui.exe can keep running.
"""
import os
import shutil
import socket
import subprocess
import sys
import time

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
DEFAULT_USB = os.path.join(REPO, 'usb')
DEFAULT_PORT = 7788
DEFAULT_HOST = '127.0.0.1'
# ABFlashKit (and PSC-Bios) moved to their own repository on 2026-09-23 (CLAUDE.md, "Where the code lives")
# and are no longer built here. Same convention as AB_PCSX_DIR / ../pcsx-ab (make_psc.sh): a sibling checkout,
# overridable for one that lives somewhere else.
CONSOLE_TOOLS_DIR = os.environ.get('AB_CONSOLE_TOOLS_DIR', os.path.join(REPO, '..', 'autobleem-console-tools'))


TAP_MS = 60  # a tap's hold: over a frame of a slow machine would be better, under the 350 ms where a list repeats
DPAD = ('up', 'down', 'left', 'right')
BUTTONS = ('x', 'o', 's', 't', 'start', 'select', 'l1', 'r1', 'l2', 'r2') + DPAD  # the driver's logical names


class RunFailed(RuntimeError):
    """a step of a script failed: what the steps before it replied stays in `replies` (their shots and grabs are
    written already), `step` is its 1-based number"""

    def __init__(self, step, text, error, replies):
        super().__init__(f'step {step} {text!r} failed: {error}')
        self.step = step
        self.replies = replies


def pid_file(port):
    return os.path.join(REPO, 'build_win', f'ab_drive-{port}.pid')


def _redact(line):
    """'auth s3cr3t' -> 'auth ***' - what a raised RuntimeError shows instead of the real token, so a refusal
    printed to a terminal or logged by a CI step never leaks it. Anything that is not an `auth ...` line is
    returned unchanged."""
    if line.startswith('auth '):
        return 'auth ***'
    return line


def clip_to_mp4(frames_dir, out=None):
    """a clip's folder (the driver's `clip stop`: PNG frames + clip.ffconcat) -> an MP4 at 25 fps, with ffmpeg;
    the frames are removed once the MP4 is there. None when ffmpeg is not installed (the frames stay)."""
    if not shutil.which('ffmpeg'):
        return None
    out = out or frames_dir.rstrip('/\\') + '.mp4'
    subprocess.run(['ffmpeg', '-y', '-loglevel', 'error', '-f', 'concat', '-safe', '0', '-i',
                    os.path.join(frames_dir, 'clip.ffconcat'), '-vf',
                    'fps=25,scale=trunc(iw/2)*2:trunc(ih/2)*2,format=yuv420p', '-c:v', 'libx264', '-preset',
                    'veryfast', '-crf', '26', '-movflags', '+faststart', out], check=True, stdin=subprocess.DEVNULL)
    shutil.rmtree(frames_dir, ignore_errors=True)
    return out


class Driver:
    def __init__(self, port=DEFAULT_PORT, host=DEFAULT_HOST, token=None, out_prefix=None, path_map=None):
        """out_prefix: the driver writes `shot`/`clip` somewhere this machine cannot name (a sandbox in the test VM,
        its AB_DEBUG_OUT): a shot's or clip's name is sent as out_prefix + its file name, relative, instead of made
        absolute here. path_map: turns a path the driver replies with into this machine's name for it (None when
        it has none), so a clip's frames can be made into an MP4 here."""
        self.sock = socket.create_connection((host, port), timeout=10)
        self.sock.settimeout(180)  # a reply can take a while: hold 60000, wait_idle, a clip's last frames
        self.buf = b''
        self.out_prefix = out_prefix
        self.path_map = path_map or (lambda p: p if out_prefix is None else None)
        self.clip_out = None  # where the running clip's MP4 goes
        if token:
            self.cmd('auth ' + token)

    def _line(self):
        # the next '\n'-terminated line, buffering whatever came with it (a `grab` reply's bytes included)
        while b'\n' not in self.buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise RuntimeError('driver closed the connection')
            self.buf += chunk
        line, self.buf = self.buf.split(b'\n', 1)
        return line

    def _exact(self, n):
        while len(self.buf) < n:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise RuntimeError('driver closed the connection')
            self.buf += chunk
        data, self.buf = self.buf[:n], self.buf[n:]
        return data

    def cmd(self, line):
        self.sock.sendall((line + '\n').encode('utf-8'))
        reply = self._line().decode('utf-8', 'replace')
        if reply.startswith('err'):
            # never echo a real token back - even on a refusal, which is the one reply guaranteed to happen
            # right after `auth <token>` on a wrong guess
            raise RuntimeError(f'{_redact(line)!r}: {reply}')
        return reply

    def grab(self, local_path):
        # "ok <n>" then exactly n raw PNG bytes - no file written on whatever machine the driver runs on
        self.sock.sendall(b'grab\n')
        header = self._line().decode('utf-8', 'replace')
        if header.startswith('err'):
            raise RuntimeError(f'grab: {header}')
        n = int(header.split(' ', 1)[1])
        data = self._exact(n)
        local_path = os.path.abspath(local_path)
        os.makedirs(os.path.dirname(local_path), exist_ok=True)
        with open(local_path, 'wb') as f:
            f.write(data)
        return 'ok ' + local_path

    def wait_screen(self, name, timeout=15.0):
        end = time.time() + timeout
        while time.time() < end:
            reply = self.cmd('screen')
            current = reply.split(' ', 1)[1] if ' ' in reply else ''
            if current == name:
                return 'ok ' + name
            time.sleep(0.05)
        raise RuntimeError(f'screen {name} did not show (now: {current})')

    def items(self, timeout=5.0):
        # the showing picker's items (DebugDriver `items`); published in its init(), so polled a moment
        end = time.time() + timeout
        while True:
            reply = self.cmd('items')
            names = [n for n in reply[3:].split('|') if n]
            if names or time.time() > end:
                return names
            time.sleep(0.05)

    def pick(self, item):
        # the item by its English title (case-insensitive, a unique prefix will do) or its index
        item = item.strip().strip('"\'')
        names = self.items()
        if item.isdigit():
            index = int(item)
            if index >= len(names):
                raise RuntimeError(f'no item {index}: {names}')
        else:
            wanted = item.lower()
            exact = [i for i, n in enumerate(names) if n.lower() == wanted]
            prefix = [i for i, n in enumerate(names) if n.lower().startswith(wanted)]
            found = exact or prefix
            if len(found) != 1:
                raise RuntimeError(f'item {item!r} ' + ('is ambiguous' if found else 'not found') + f': {names}')
            index = found[0]
        if not self.move_to(index):
            for _ in range(index):  # a driver without `selected`: count the presses, as before
                self.cmd(f'press down {TAP_MS}')
                self.frame_gap()
        self.cmd('press x')
        return 'ok ' + names[index]

    def selected(self):
        # the DebugDriver's `selected` ("ok <index>|<name>") as (index, name); None on a driver without it
        try:
            reply = self.cmd('selected')
        except RuntimeError:
            return None
        index, _, name = reply[3:].partition('|')
        try:
            return int(index), name
        except ValueError:
            return None

    def move_to(self, index, timeout=15.0):
        # closed loop: step the cursor until the driver says it is on row `index` - the presses are never counted,
        # so a double move (a list's own repeat on a slow frame, a late release) is walked back instead of picking
        # the wrong row. False on a driver without `selected`, for the caller to fall back to counting.
        end = time.time() + timeout
        while time.time() < end:
            now = self.selected()
            if now is None or now[0] < 0:
                return False
            if now[0] == index:
                self.rest()  # the cursor may still be settling - look once more after the picture rests
                again = self.selected()
                if again and again[0] == index:
                    return True
                continue
            self.cmd(f'press {"down" if now[0] < index else "up"} {TAP_MS}')
            moved = time.time() + 1.5
            while time.time() < moved:
                after = self.selected()
                if after and after[0] != now[0]:
                    break
                time.sleep(0.02)
        raise RuntimeError(f'the cursor did not reach row {index} in {timeout:.0f} s (now {self.selected()})')

    def select(self, item):
        # a classic list's row (Options, Game Manager, an editor, the set picker, Extensions...) by its text as
        # shown (translated, case-insensitive, a unique prefix will do) or its index, headings ('#...') excluded
        # from the name match; the cursor is moved there, nothing is pressed
        item = item.strip().strip('"\'')
        names = self.items()
        if item.isdigit():
            index = int(item)
        else:
            wanted = item.lower()
            rows = [(i, n) for i, n in enumerate(names) if not n.startswith('#')]
            exact = [i for i, n in rows if n.lower() == wanted]
            prefix = [i for i, n in rows if n.lower().startswith(wanted)]
            found = exact or prefix
            if len(found) != 1:
                raise RuntimeError(f'row {item!r} ' + ('is ambiguous' if found else 'not found') + f': {names}')
            index = found[0]
        if not self.move_to(index):
            raise RuntimeError('select needs a driver with `selected` (feature/driver-state or later)')
        return f'ok {index}|{names[index] if index < len(names) else ""}'

    def current_screen(self):
        reply = self.cmd('screen')
        return reply.split(' ', 1)[1] if ' ' in reply else ''

    def frame_gap(self, frames=3):
        # a press is seen by the screen on its next frame (a menu draws 10-15 a second in the VM): two presses
        # closer than that are handled together and a list moves two rows or none - so wait for frames to be drawn
        try:
            start = int(self.cmd('frames').split()[1])
            end = time.time() + 1.5
            while time.time() < end and int(self.cmd('frames').split()[1]) < start + frames:
                time.sleep(0.01)
        except (RuntimeError, ValueError, IndexError):
            time.sleep(0.2)

    def rest(self, ms=300):
        # no busy spinner and a picture at rest (`wait_ready`); an older driver: the picture at rest only
        try:
            self.cmd('wait_ready 10')
            return
        except RuntimeError:
            pass
        try:
            self.cmd(f'wait_idle {ms} 10')
        except RuntimeError:
            time.sleep(0.6)  # an older driver without wait_idle, or a picture that never rests: a plain pause

    def settle(self):
        # the launcher is back AND takes input: right after Options / Game Manager close it shows GuiLauncher
        # while a busy spinner still ignores every input (the busy rule) and it fades in - wait_idle rides both out
        self.wait_screen('GuiLauncher')
        self.rest()

    def open_system_menu(self, opener):
        # opener() sends the input that opens the menu; the whole thing is tried up to 3 times
        last = None
        for _ in range(3):
            if self.current_screen() == 'GuiSystemMenu':
                return  # a slow open that the previous try already caused
            self.settle()
            opener()
            try:
                self.wait_screen('GuiSystemMenu', 2.0)
                self.rest()  # a press while the menu still opens can move the cursor twice
                return
            except RuntimeError as e:
                last = e
        raise RuntimeError(f'the System menu did not open in 3 tries ({last})')

    def chord(self):
        # L2+R2 as a real hold, not a blip
        self.cmd('down l2')
        try:
            self.cmd('down r2')
            time.sleep(0.15)
            self.cmd('up r2')
        finally:
            self.cmd('up l2')

    def quick_key(self):
        self.cmd('down up')
        time.sleep(0.15)
        self.cmd('up up')

    def pick_and_wait(self, item):
        # the chosen screen opens late: wait until the menu is gone, so no `wait_screen` is needed after it
        reply = self.pick(item)
        end = time.time() + 10
        while self.current_screen() == 'GuiSystemMenu':
            if time.time() > end:
                raise RuntimeError(f'{item!r} was picked but the System menu is still showing after 10 s')
            time.sleep(0.05)
        return reply

    def menu(self, item):
        self.open_system_menu(self.chord)
        return self.pick_and_wait(item)

    def quick(self, item):
        self.open_system_menu(self.quick_key)
        return self.pick_and_wait(item)

    def home(self, max_presses=6):
        # Circle until the launcher shows: the way out of Game Manager / Options / whatever a run left open
        for _ in range(max_presses):
            if self.current_screen() == 'GuiLauncher':
                return 'ok GuiLauncher'
            before = self.current_screen()
            self.cmd('press o 60')
            end = time.time() + 1.0
            while time.time() < end and self.current_screen() == before:
                time.sleep(0.05)
        if self.current_screen() == 'GuiLauncher':
            return 'ok GuiLauncher'
        raise RuntimeError(f'home: still on {self.current_screen()} after {max_presses} presses of Circle')

    @staticmethod
    def alias(words):
        """the client's single-step words -> the driver's own command (None: not an alias). Only the logical button
        names are taken over, so `@1 tap a`, `hold a 300` and the rest of padsim's words still go to the driver."""
        w = words[0]
        if w in DPAD and len(words) == 1:
            return f'press {w} {TAP_MS}'
        if len(words) < 2:
            return None
        b = words[1]
        if w == 'dpad':
            # one short press per name - `dpad down; wait 200; dpad center` no longer lets the repeat fire
            if b == 'center' and len(words) == 2:
                return 'ping'
            return f'press {b} {TAP_MS}' if b in DPAD and len(words) == 2 else None
        if b not in BUTTONS:
            return None
        if w == 'tap' and len(words) <= 3:
            return f'press {b} {words[2] if len(words) > 2 else TAP_MS}'
        if w == 'hold' and len(words) == 2:
            return f'down {b}'
        if w == 'hold' and len(words) == 3:
            return f'press {b} {words[2]}'
        if w == 'release' and len(words) == 2:
            return f'up {b}'
        return None

    def run(self, script):
        """each step's reply in a list; a failing step raises RunFailed (a RuntimeError) that carries the replies
        of the steps done before it"""
        out = []
        n = 0
        for part in script.split(';'):
            part = part.strip()
            if not part:
                continue
            n += 1
            try:
                self.step(part, out)
            except (RuntimeError, OSError, ValueError, IndexError) as e:
                raise RunFailed(n, part, e, out) from None
        return out

    def step(self, part, out):
        words = part.split()
        aliased = self.alias(words)
        if aliased:
            out.append(self.cmd(aliased))
            if aliased.startswith('press '):
                self.frame_gap()
            return
        if words[0] == 'home':
            out.append(self.home())
            return
        if words[0] == 'select':
            out.append(self.select(part.split(None, 1)[1]))
            return
        if words[0] == 'shot':
            path = part.split(None, 1)[1].strip()
            if self.out_prefix is None:
                path = os.path.abspath(path)
            else:
                path = self.out_prefix + os.path.basename(path)
            part = 'shot ' + path
        elif words[0] == 'clip' and len(words) > 2 and words[1] == 'start':
            name = part.split(None, 2)[2].strip()
            stem = name[:-4] if name.endswith('.mp4') else name
            if self.out_prefix is None:
                stem = os.path.abspath(stem)
            else:
                stem = self.out_prefix + os.path.basename(stem)
            part = 'clip start ' + stem
            self.clip_out = os.path.basename(stem) + '.mp4'
        elif words[0] == 'clip' and len(words) > 1 and words[1] == 'stop':
            out.append(self.clip_stop())
            return
        elif words[0] == 'grab':
            out.append(self.grab(part.split(None, 1)[1].strip()))
            return
        elif words[0] == 'wait_screen':
            out.append(self.wait_screen(words[1], float(words[2]) if len(words) > 2 else 15.0))
            return
        elif words[0] in ('menu', 'quick'):
            what = part.split(None, 1)[1]
            out.append(self.menu(what) if words[0] == 'menu' else self.quick(what))
            return
        out.append(self.cmd(part))

    def clip_stop(self):
        # "ok <folder> <n> frames <s> s": the folder made into an MP4 next to it, where this machine can see it
        reply = self.cmd('clip stop')
        words = reply.split()
        local = self.path_map(words[1]) if len(words) > 1 else None
        if local and os.path.isdir(local):
            mp4 = clip_to_mp4(local, os.path.join(os.path.dirname(local.rstrip('/\\')), self.clip_out or
                                                   os.path.basename(local.rstrip('/\\')) + '.mp4'))
            if mp4:
                reply += ' -> ' + mp4
        self.clip_out = None
        return reply

    def close(self):
        self.sock.close()


def start(usb, port, show, tool=None):
    if tool:
        # abflashkit used to build in this tree's own build_win/apps/; since the D5 split (2026-09-23) it
        # builds in autobleem-console-tools' own build_win instead - try the sibling checkout first (see
        # CONSOLE_TOOLS_DIR above) and fall back to the old in-tree path for a checkout that still has one.
        exe = os.path.join(CONSOLE_TOOLS_DIR, 'build_win', 'apps', tool, tool + '.exe')
        if not os.path.exists(exe):
            exe = os.path.join(REPO, 'build_win', 'apps', tool, tool + '.exe')
        app_dir = os.path.join(usb, 'Apps', tool)
        driven = os.path.join(app_dir, tool + '-drive.exe')
        first_screen = {'abflashkit': 'GuiConfirm'}[tool]
        # this repo's own apps/<tool>/ went with the D5 split too - the resources are the sibling
        # checkout's, next to its exe.
        lang = os.path.join(CONSOLE_TOOLS_DIR, 'apps', tool, 'resources', 'lang')
    else:
        exe = os.path.join(REPO, 'build_win', 'autobleem-gui.exe')
        app_dir = os.path.join(usb, 'Autobleem', 'bin', 'autobleem')
        # under its own name, in a folder of its own: an extension imports from "autobleem-gui.exe" by name
        # (docs/extensions-plan.md), so a renamed copy could load none; the resources are the root's anyway
        os.makedirs(os.path.join(app_dir, 'drive'), exist_ok=True)
        driven = os.path.join(app_dir, 'drive', 'autobleem-gui.exe')
        first_screen = 'GuiLauncher'
        lang = os.path.join(REPO, 'src', 'resources', 'lang')
    # its own copy of the exe next to the resources: the owner's own instance may be running the other
    # - a moment after the previous run's `stop`, Windows can still hold the file open while that process
    # finishes exiting (SQLite/theme teardown), so a PermissionError here is retried briefly instead of
    # failing the whole capture
    for attempt in range(20):
        try:
            shutil.copy(exe, driven)
            break
        except PermissionError:
            if attempt == 19:
                raise
            time.sleep(0.25)
    # the language files change with the strings; the rest of the resources are make_usb.py's
    os.makedirs(os.path.join(app_dir, 'lang'), exist_ok=True)
    for name in os.listdir(lang):
        shutil.copy(os.path.join(lang, name), os.path.join(app_dir, 'lang', name))
    env = dict(os.environ)
    # `start` always talks to its own launch on 127.0.0.1 (the readiness Driver(port) below, with no --host).
    # AB_DEBUG_BIND left inherited would be able to make that address unreachable outright - a bind to one
    # specific non-loopback address (not 0.0.0.0) means nothing is listening on 127.0.0.1 at all, and the
    # readiness wait below would just time out with "the driver did not answer". Dropped unconditionally:
    # never what a *local* launch wants, whatever a `run --host ...` against a device left set.
    env.pop('AB_DEBUG_BIND', None)
    # AB_DEBUG_TOKEN is left as inherited on purpose (unlike AB_DEBUG_BIND above): "set = required" applies
    # on loopback too (see debug_driver.h), so if it is set this launch requires it as much as a remote one
    # would - and leaving it set is what keeps a `stop`/`screen`/... run straight after, from the same shell,
    # matching without a --token of its own (main() already reads AB_DEBUG_TOKEN for every command). What
    # changes here is only that start()'s own readiness commands below now authenticate with it too.
    token = env.get('AB_DEBUG_TOKEN')
    env['AB_DEBUG_PORT'] = str(port)
    env['AB_NO_SPLASH'] = '1'  # straight to the launcher (GuiSplash honours it on a dev host)
    # R29: headless by default - the window is created with SDL_WINDOW_HIDDEN from its very first frame
    # (never flashes visible, never takes focus) and audio goes to SDL's dummy driver, so a tester's run
    # never shows a window or a firewall prompt on the owner's PC. --show is the only way to get a visible
    # window (drop AB_HEADLESS entirely rather than set it to "0" - Platform::headlessRequested() only
    # treats exactly "1" as headless, but an unset variable is the least surprising "off").
    if show:
        env.pop('AB_HEADLESS', None)
    else:
        env['AB_HEADLESS'] = '1'
    env['PATH'] = r'C:\msys64\ucrt64\bin;' + env.get('PATH', '')
    proc = subprocess.Popen([driven, usb], cwd=app_dir, env=env,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    with open(pid_file(port), 'w') as f:
        f.write(str(proc.pid))
    # the driver comes up after the library is open (a few seconds on a first scan)
    for _ in range(300):
        time.sleep(0.1)
        try:
            d = Driver(port, token=token)
            break
        except OSError:
            if proc.poll() is not None:
                raise RuntimeError(f'the launcher exited with {proc.returncode}')
    else:
        raise RuntimeError('the driver did not answer')
    d.wait_screen(first_screen, 60)
    d.close()
    print(f'started pid {proc.pid} on port {port}' + ('' if show else ', headless'))


def _pid_alive(pid):
    # tasklist's own filter, not a plain grep of the full listing - a fast, single-process check
    r = subprocess.run(['tasklist', '/FI', 'PID eq %d' % pid], capture_output=True, text=True)
    return str(pid) in r.stdout


def stop(port, host=DEFAULT_HOST, token=None):
    try:
        d = Driver(port, host, token)
        d.cmd('quit')
        d.close()
    except OSError:
        pass
    if os.path.exists(pid_file(port)):
        pid = int(open(pid_file(port)).read().strip())
        # D24: give the process real time to unwind on its own before force-killing it - a clean Quit
        # closes every screen on the stack, including a running extension's (PSC-Bios's Network hub among
        # them), and only THAT unwind clears its crash-guard marker (ExtensionCatalog::clearActive(),
        # <runtime>/extensions.active). A flat 0.5s wait then an unconditional `taskkill /F` (the previous
        # behaviour here) routinely won the race against that unwind on this machine, which left the
        # marker in place - the next `start` read it back as "was running when AutoBleem stopped last
        # time" and auto-disabled the extension (System/Extensions/disabled.txt), breaking every capture
        # that opens PSC-Bios (pscbios-main and everything after it in the same run) until it was
        # re-enabled by hand. The old ~13s figure recorded here was this tool's own bug, not the app's:
        # `quit` injected a single one-shot Quit event, which only the innermost screen's poll() loop ever
        # consumed - a nested screen (GuiPadConfig under the System Menu's Network hub) left everything
        # above it with no reason to unwind, so the process sat there until this loop's budget ran out and
        # force-killed it (TOOLS-8). DebugDriver's `quit` now calls Input::requestQuit() instead - the same
        # persistent condition the real Power button sets, which every nested screen's own poll() sees in
        # turn - and a clean exit from GuiPadConfig now measures ~1.5s, repeatably, over three runs. The
        # loop below returns as soon as the pid is gone, so that clean exit still comes back in ~1.5s; the
        # budget stays a generous 30s (not counting each tasklist spawn's own overhead) as a safety margin
        # for a loaded machine, where a premature force-kill is the failure this comment describes, not a
        # tight guess against the common case.
        for _ in range(30):
            if not _pid_alive(pid):
                break
            time.sleep(1.0)
        else:
            subprocess.call(['taskkill', '/PID', str(pid), '/F'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
            # this force-kill is the test driver's own doing, not a real crash - the marker it left behind
            # (ExtensionCatalog::markActive(), never reached clearActive() because the process didn't get
            # to unwind) would otherwise make the NEXT `start` read it as "was running when AutoBleem
            # stopped last time" and auto-disable that extension (System/Extensions/disabled.txt) for
            # every run after this one. `stop` always targets DEFAULT_USB here (see main(), same as a
            # plain `start` with no --usb) - a remote device's own marker is its own business.
            marker = os.path.join(DEFAULT_USB, 'System', 'Runtime', 'extensions.active')
            if os.path.exists(marker):
                os.remove(marker)
        os.remove(pid_file(port))
    print('stopped')


def sheet(out, paths, columns=2, width=640):
    from PIL import Image, ImageDraw
    ims = [Image.open(p).convert('RGB') for p in paths]
    w, h = width, width * 9 // 16
    rows = (len(ims) + columns - 1) // columns
    img = Image.new('RGB', (columns * (w + 10), rows * (h + 22)), (30, 30, 30))
    d = ImageDraw.Draw(img)
    for i, (p, im) in enumerate(zip(paths, ims)):
        x, y = (i % columns) * (w + 10), (i // columns) * (h + 22)
        d.text((x + 4, y + 3), os.path.basename(p), fill=(255, 255, 0))
        img.paste(im.resize((w, h)), (x, y + 18))
    img.save(out)
    print(out)


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    cmd = argv[1]
    port = DEFAULT_PORT
    host = DEFAULT_HOST
    token = os.environ.get('AB_DEBUG_TOKEN')
    args = argv[2:]
    if '--port' in args:
        i = args.index('--port')
        port = int(args[i + 1])
        del args[i:i + 2]
    if '--host' in args:
        i = args.index('--host')
        host = args[i + 1]
        del args[i:i + 2]
    if '--token' in args:
        i = args.index('--token')
        token = args[i + 1]
        del args[i:i + 2]
    script_file = None
    if '--file' in args:
        i = args.index('--file')
        script_file = args[i + 1]
        del args[i:i + 2]
    if cmd == 'start':
        # always this PC: what starts the exe here, never a remote driver - --host/--token do not apply
        usb = DEFAULT_USB
        show = '--show' in args
        if '--usb' in args:
            usb = args[args.index('--usb') + 1]
        # made absolute now, relative to the caller's cwd: start() launches the exe with cwd inside the usb
        # tree itself (app_dir, several levels down), so a relative --usb (or a relative DEFAULT_USB, were
        # the caller's cwd not REPO) would be resolved against the wrong directory once passed to Popen and
        # made the launcher's own argv[1] - the process then can't find its own USB root and exits at once.
        usb = os.path.abspath(usb)
        tool = args[args.index('--tool') + 1] if '--tool' in args else None
        start(usb, port, show, tool)
        return 0
    if cmd == 'stop':
        stop(port, host, token)
        return 0
    if cmd == 'sheet':
        sheet(args[0], args[1:])
        return 0
    d = Driver(port, host, token)
    try:
        if cmd == 'run':
            if script_file:
                # one command per line, '#'-comments and blank lines dropped, so a long script reads like a
                # checklist instead of one ';'-joined shell string every quoting layer gets to mangle
                with open(script_file, encoding='utf-8') as f:
                    lines = [ln.strip() for ln in f]
                script = ';'.join(ln for ln in lines if ln and not ln.startswith('#'))
            else:
                script = ' '.join(args)
            try:
                replies = d.run(script)
            except RunFailed as e:
                # what was done before the failing step is not lost: its replies, then one error line
                for reply in e.replies:
                    print(reply)
                print(f'error: {e}', file=sys.stderr)
                return 1
            for reply in replies:
                print(reply)
        else:
            try:
                print(d.run(cmd + ' ' + ' '.join(args))[0])
            except RunFailed as e:
                print(f'error: {e}', file=sys.stderr)
                return 1
    finally:
        d.close()
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
