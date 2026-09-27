#!/usr/bin/env python3
"""Drives a RetroArch 1.22.2 instance over its network command interface (command.c/command.h, UDP,
default port 55355) - the RetroArch-side counterpart to ab_drive.py's DebugDriver client, for automated
looks at RetroArch's own menu without touching a screen. Built for R22 (see
E:\\Programming\\_team\\r-items\\R22-research.md and R22-report.md); every command name and reply shape
below was checked against RetroArch's own source at tag v1.22.2 (command.h's `map`/`action_map` arrays,
command.c's command_get_status/command_version/... and verbosity.c's log file naming) - not just the
docs.libretro.com prose the research pass had to rely on.

  python tools/ra_drive.py start [--cfg DIR] [--retroarch PATH] [--core PATH] [--port N] [--display :99]
                                                                   write a test retroarch.cfg into DIR
                                                                   (network_cmd_enable/port, video gl,
                                                                   audio null, log_to_file, screenshot_
                                                                   directory) and launch RetroArch under
                                                                   Xvfb with LIBGL_ALWAYS_SOFTWARE=1 -
                                                                   Linux only, run this ON the Debian test
                                                                   machine (see "Where this runs" below)
  python tools/ra_drive.py stop [--host H] [--port N] [--cfg DIR] sends QUIT, then (for a local instance)
                                                                   reads DIR's pidfile (must be the same
                                                                   --cfg a matching `start` used) and
                                                                   os.killpg()s the whole process group -
                                                                   SIGTERM first, SIGKILL if still alive
                                                                   after ~5s
  python tools/ra_drive.py run "<script>" [--host H] [--port N] [--shots DIR] [--log FILE]
                                                                   commands separated by ';', e.g.
                                                                   "press toggle; wait 300; press down;
                                                                   press down; press a; wait_status PLAYING"
  python tools/ra_drive.py <command> ... [--host H] [--port N] [--shots DIR] [--log FILE]
                                                                   one command, e.g. `press a`, `GET_STATUS`
  python tools/ra_drive.py sheet OUT.png IN1.png IN2.png ...       a contact sheet of shots (Pillow) - same
                                                                   as ab_drive.py's

Every command but `start`/`sheet` takes `--host <address>` (default 127.0.0.1) and `--port <n>` (default
55355, RetroArch's own default network_cmd_port) to reach an instance on another machine - UDP has no
"connect" step, `--host`/`--port` is just where each packet is addressed. `--shots DIR` is
screenshot_directory (needed by `shot`/`wait_shot`), `--log FILE` is the retroarch.log path (needed by
`wait_log`).

The script language: `press <btn>` (up/down/left/right/a/b/toggle - the menu-navigation subset of
command.h's map[], see "Buttons" below), `wait <ms>`, `wait_status <PAUSED|PLAYING|CONTENTLESS> [timeout s]`
(polls GET_STATUS), `wait_log <pattern> [timeout s]` (polls --log FILE for a line matching the regex -
RetroArch has no "which menu is showing" query the way the launcher's DebugDriver has `screen`; see "Which
menu is showing" below), `shot <file>` (SCREENSHOT, then waits for a new file in --shots DIR and copies it
to <file> - RetroArch's own screenshot has no reply, unlike ab_drive.py's `shot`, so this is a directory
poll, not a socket wait), `wait_shot <ref.png> [timeout s] [threshold]` (screenshot-hashes against a
reference PNG - the practical "wait for this menu" substitute), `menu <n>` (MENU_TOGGLE, MENU_DOWN * n,
MENU_A - a 0-based row index, there is no item-name query on this side), and any exact command.h name bare
or after `cmd` (`QUIT`, `RESET`, `PAUSE_TOGGLE`, `GET_STATUS`, `LOAD_STATE_SLOT 2`, ...) - checked against
the confirmed map/action_map tables before it is sent, so a typo is caught locally instead of vanishing into
RetroArch's own [NetCMD] warning log.

Buttons (press <btn>): up down left right a b toggle - MENU_UP/DOWN/LEFT/RIGHT/A/B/TOGGLE. This is
menu-navigation only: the command port has no raw in-game RetroPad button commands (A/B/X/Y/L/R for
*content*, as opposed to the menu) - that needs the separate Remote RetroPad core, ruled out for R22 in the
research pass (network_remote_*, port 55400+, needs a second RetroArch instance as the sender).

Which menu is showing: RetroArch's GET_STATUS only ever says PAUSED/PLAYING/CONTENTLESS (content.c's
command_get_status - not which menu tab or row is open); there is no equivalent of the launcher's
GuiScreen-per-class `screen` reply. Two substitutes: `wait_log <pattern>` greps retroarch.log for a line -
exact when the right pattern is known, but *which* lines name a menu transition was not captured against a
running instance this session (verbosity.c confirms the log format itself: plain `[INFO] `/`[WARN]
`/`[ERROR] `/`[DEBUG] ` tags, no timestamp by default - see LogTail below - but not the menu driver's own
message text); and `wait_shot` (screenshot + average-hash distance against a reference image), which needs
no log-text guess and is what this session verified end to end (offline, against synthetic PNGs - see
tools/test_ra_drive.py). Capture real reference shots and, if useful, real log lines once RetroArch is
running on the Debian machine (R22-TEST.md has the follow-up steps for Nina).

Why `video_driver=gl`, never `sdl2`: `start`'s cfg template used to leave this an open choice, but
`video_driver=sdl2` segfaults RetroArch 1.22.2's official AppImage 100% of the time under Xvfb - an
`XScreenSaverQueryExtension()` call in its bundled `libXss.so.1`/`libXext.so.6` crashes inside the host's
`libX11.so.6` (an Xlib extension-registration ABI mismatch between the AppImage's bundled X11 client libs
and the host's `libX11`), independent of the core loaded or the Xvfb screen size. `gl`/`glcore` never crash.
This is an upstream RetroArch AppImage packaging issue, not anything `ra_drive.py` or this project can fix -
see `E:\\Programming\\_team\\r-items\\R22-phase1-root-cause.md` for the gdb backtrace and driver matrix.

Where this runs: `start` needs Xvfb, so it must run **on the Debian machine** (192.168.68.148) - there is
no Windows RetroArch in this project and this tool never launches one, per the owner's rule (no RetroArch
download to this PC, no window opened here). The simplest split, matching R22's plan: ssh onto that machine
and run this script there for everything - `start`, then `run "..."` in the same or another ssh session -
since screenshot_directory and retroarch.log are both local files on whichever machine RetroArch runs on,
driving it from elsewhere over `--host` would still need a second hop (sftp/scp) just to read those two
paths back. `--host`/`--port` remain for the rare case of sending a bare command (`press`, `QUIT`, ...) at
a RetroArch on the LAN from another machine, with no shot/log-dependent commands in the script.
"""
import os
import re
import shutil
import signal
import socket
import subprocess
import sys
import time

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
DEFAULT_HOST = '127.0.0.1'
DEFAULT_PORT = 55355  # command.h's RARCH_DEFAULT_PORT / network_cmd_port default

# command.h's `map[]` - commands with no argument, no reply (command_parse_sub_msg just flips
# handle->state[map[i].id] = true; nothing calls cmd->replier). Confirmed against RetroArch v1.22.2
# command.h (this session, see the module docstring) - not just docs.libretro.com's prose list.
NO_ARG_COMMANDS = {
    'MENU_TOGGLE', 'QUIT', 'CLOSE_CONTENT', 'RESET',
    'FAST_FORWARD', 'FAST_FORWARD_HOLD', 'SLOWMOTION', 'SLOWMOTION_HOLD', 'REWIND',
    'PAUSE_TOGGLE', 'FRAMEADVANCE',
    'MUTE', 'VOLUME_UP', 'VOLUME_DOWN',
    'LOAD_STATE', 'SAVE_STATE', 'STATE_SLOT_PLUS', 'STATE_SLOT_MINUS',
    'PLAY_REPLAY', 'RECORD_REPLAY', 'HALT_REPLAY', 'SAVE_REPLAY_CHECKPOINT',
    'PREV_REPLAY_CHECKPOINT', 'NEXT_REPLAY_CHECKPOINT', 'REPLAY_SLOT_PLUS', 'REPLAY_SLOT_MINUS',
    'DISK_EJECT_TOGGLE', 'DISK_NEXT', 'DISK_PREV',
    'SHADER_TOGGLE', 'SHADER_HOLD', 'SHADER_NEXT', 'SHADER_PREV',
    'CHEAT_TOGGLE', 'CHEAT_INDEX_PLUS', 'CHEAT_INDEX_MINUS',
    'SCREENSHOT', 'RECORDING_TOGGLE', 'STREAMING_TOGGLE',
    'TURBO_FIRE_TOGGLE', 'GRAB_MOUSE_TOGGLE', 'GAME_FOCUS_TOGGLE', 'FULLSCREEN_TOGGLE',
    'UI_COMPANION_TOGGLE',
    'VRR_RUNLOOP_TOGGLE', 'RUNAHEAD_TOGGLE', 'PREEMPT_TOGGLE', 'FPS_TOGGLE', 'STATISTICS_TOGGLE',
    'AI_SERVICE',
    'NETPLAY_PING_TOGGLE', 'NETPLAY_HOST_TOGGLE', 'NETPLAY_GAME_WATCH', 'NETPLAY_PLAYER_CHAT',
    'NETPLAY_FADE_CHAT_TOGGLE',
    'MENU_UP', 'MENU_DOWN', 'MENU_LEFT', 'MENU_RIGHT', 'MENU_A', 'MENU_B',
    'OVERLAY_NEXT', 'OSK',
}

# command.h's `action_map[]` - commands that reply (some take an argument after a space; None = no
# argument). command_get_arg() in command.c is what actually splits "NAME arg" - a name found here is
# always routed through request()/recvfrom, one whose name is in NO_ARG_COMMANDS through send().
REPLY_COMMANDS = {
    'VERSION': None,
    'GET_STATUS': None,
    'GET_CONFIG_PARAM': '<param name>',
    'SHOW_MSG': None,
    'READ_CORE_MEMORY': '<address> <number of bytes>',
    'WRITE_CORE_MEMORY': '<address> <byte1> <byte2> ...',
    'LOAD_STATE_SLOT': '<slot number>',
    'PLAY_REPLAY_SLOT': '<slot number>',
    'SEEK_REPLAY': '<frame number>',
    'SAVE_FILES': None,
    'LOAD_FILES': None,
    'LOAD_CORE': '<core path>',
    # HAVE_CHEEVOS-gated in command.h; harmless to list even off a cheevos build - RetroArch would just
    # answer "not recognized" if a target build lacks them, same as any other absent command.
    'READ_CORE_RAM': '<address> <number of bytes>',
    'WRITE_CORE_RAM': '<address> <byte1> <byte2> ...',
}

# The menu-navigation subset of NO_ARG_COMMANDS, under the launcher's own button-name style
# (ab_drive.py: x o s t start select l1 r1 l2 r2 up down left right) - RetroArch's command port has no
# raw in-game RetroPad buttons (see the module docstring's "Buttons" paragraph), only these.
BUTTONS = {
    'up': 'MENU_UP', 'down': 'MENU_DOWN', 'left': 'MENU_LEFT', 'right': 'MENU_RIGHT',
    'a': 'MENU_A', 'b': 'MENU_B', 'toggle': 'MENU_TOGGLE',
}

_GET_STATUS_RE = re.compile(r'^GET_STATUS (PAUSED|PLAYING) ([^,]*),([^,]*),crc32=([0-9a-fA-F]+)')


class RaError(RuntimeError):
    pass


class RaClient:
    """One UDP socket addressed at a RetroArch instance's command port. Fire-and-forget (`send`) for
    map[] commands (network_command_reply() in command.c: "Respond (fire and forget since it's UDP)" -
    every map[] command has no replier call at all, so there is nothing to fire-and-forget in the first
    place); request/reply (`request`) for action_map[] commands, which do call cmd->replier(cmd, ...)."""

    def __init__(self, host=DEFAULT_HOST, port=DEFAULT_PORT, timeout=2.0):
        self.host = host
        self.port = port
        self.timeout = timeout
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.settimeout(timeout)

    def send(self, line):
        self.sock.sendto(line.encode('utf-8'), (self.host, self.port))

    def request(self, line, timeout=None):
        """Send a command known to reply and return its decoded text with any trailing newline stripped -
        command.c is not consistent about appending one (VERSION does, LOAD_STATE_SLOT does not).

        Nothing listening on (host, port) shows up two different ways, and neither is RaError's usual
        shape unless caught here: a plain timeout (nothing ever answers - the common case when RetroArch
        just is not running, or a firewall drops the packet) and, on some platforms/routes, an ICMP port-
        unreachable turning into a connection-refused error on the *next* socket call - Windows surfaces a
        UDP send to a closed port as WSAECONNRESET on the following recv (ConnectionResetError), Linux
        typically as ECONNREFUSED (ConnectionRefusedError) on the recv or occasionally the sendto itself.
        Both are OSError subclasses; left uncaught they would raise as a raw socket traceback with no hint
        of what to check, instead of the same clear RaError every other bad-input case here raises."""
        effective_timeout = timeout if timeout is not None else self.timeout
        self.sock.settimeout(effective_timeout)
        try:
            self.sock.sendto(line.encode('utf-8'), (self.host, self.port))
            data, _ = self.sock.recvfrom(4096)
        except socket.timeout:
            raise RaError('no response from {}:{} (timeout {}s) - is RetroArch running?'.format(
                self.host, self.port, effective_timeout))
        except OSError as e:
            raise RaError('{}:{} refused ({}) - is RetroArch running?'.format(self.host, self.port, e))
        return data.decode('utf-8', 'replace').rstrip('\n')

    def command(self, name, arg=None):
        """Any exact command.h name (case-insensitive on the way in, sent upper-cased as RetroArch spells
        it), routed through request() or send() by which table it is in. Raises RaError for a name in
        neither - the same check command_verify() does in command.c, just done locally first."""
        name = name.upper()
        if name in REPLY_COMMANDS:
            line = name if arg is None else '{} {}'.format(name, arg)
            return self.request(line)
        if name in NO_ARG_COMMANDS:
            if arg is not None:
                raise RaError('{} takes no argument'.format(name))
            self.send(name)
            return None
        raise RaError('not a RetroArch command port command: {!r}'.format(name))

    def press(self, button):
        name = BUTTONS.get(button.lower())
        if not name:
            raise RaError('unknown button {!r} (have: {})'.format(button, ', '.join(sorted(BUTTONS))))
        self.send(name)

    def version(self):
        return self.request('VERSION')

    def get_status(self):
        """GET_STATUS's reply, parsed - command_get_status() in command.c has exactly two shapes:
        "GET_STATUS CONTENTLESS" (no content loaded) or "GET_STATUS PAUSED|PLAYING sys,name,crc32=XXXX"."""
        reply = self.request('GET_STATUS')
        if reply.startswith('GET_STATUS CONTENTLESS'):
            return {'state': 'CONTENTLESS'}
        m = _GET_STATUS_RE.match(reply)
        if not m:
            raise RaError('unexpected GET_STATUS reply: {!r}'.format(reply))
        return {'state': m.group(1), 'system_id': m.group(2), 'content': m.group(3), 'crc32': m.group(4)}

    def wait_status(self, want, timeout=15.0, poll=0.1):
        want = want.upper()
        end = time.time() + timeout
        last = None
        while time.time() < end:
            try:
                last = self.get_status()
            except RaError:
                # request() below already turned a timeout/refused-connection into RaError; a single failed
                # poll (RetroArch briefly not answering, e.g. still starting up) should not end the wait -
                # only running out of `timeout` here should.
                last = None
            if last and last['state'] == want:
                return last
            time.sleep(poll)
        raise RaError('status never reached {} (last: {})'.format(want, last))

    def screenshot(self, directory, timeout=5.0, poll=0.1):
        """SCREENSHOT is a map[] command - no reply, so the only way to know one landed is to notice a new
        file in screenshot_directory (the same directory-poll shape ab_drive.py's AB_SHOT convention uses
        on the launcher's own side)."""
        before = _dir_snapshot(directory)
        self.send('SCREENSHOT')
        end = time.time() + timeout
        while time.time() < end:
            after = _dir_snapshot(directory)
            new = [p for p, st in after.items() if before.get(p) != st]
            if new:
                return max(new, key=lambda p: after[p][0])
            time.sleep(poll)
        raise RaError('no new screenshot appeared in {} within {}s'.format(directory, timeout))

    def wait_shot(self, ref_path, directory, timeout=15.0, threshold=6, poll=0.5):
        """Takes screenshots until one average-hashes within `threshold` bits of ref_path, or times out -
        the practical "wait for this menu" substitute (see the module docstring)."""
        ref_hash = average_hash(ref_path)
        end = time.time() + timeout
        last_shot = last_dist = None
        while time.time() < end:
            last_shot = self.screenshot(directory, timeout=max(1.0, end - time.time()))
            last_dist = hash_distance(ref_hash, average_hash(last_shot))
            if last_dist <= threshold:
                return last_shot
            time.sleep(poll)
        raise RaError('no shot within {} bits of {} in {}s (closest: {}, distance {})'.format(
            threshold, ref_path, timeout, last_shot, last_dist))

    def close(self):
        self.sock.close()


def _dir_snapshot(directory):
    try:
        names = os.listdir(directory)
    except OSError:
        return {}
    out = {}
    for name in names:
        path = os.path.join(directory, name)
        try:
            st = os.stat(path)
        except OSError:
            continue
        out[path] = (st.st_mtime, st.st_size)
    return out


def average_hash(path, hash_size=8):
    """A simple perceptual hash (ahash): greyscale, downsize to hash_size x hash_size, one bit per pixel
    (>= the mean or not), as a hex string. Good enough to tell "the same menu screen" from "a different
    one" without needing exact pixels - antialiasing/cursor-blink jitter between two shots of the same
    screen should land within a few bits, which is what `threshold` is for."""
    from PIL import Image
    img = Image.open(path).convert('L').resize((hash_size, hash_size))
    pixels = list(img.getdata())
    avg = sum(pixels) / len(pixels)
    bits = ''.join('1' if p >= avg else '0' for p in pixels)
    return '{:0{}x}'.format(int(bits, 2), (hash_size * hash_size + 3) // 4)


def hash_distance(h1, h2):
    return bin(int(h1, 16) ^ int(h2, 16)).count('1')


class LogTail:
    """Tails a retroarch.log written with log_to_file=true (see the module docstring's "Which menu is
    showing" paragraph). verbosity.c's RARCH_LOG_V (the non-Qt/WinRT/Apple branch, i.e. a plain Linux
    build) writes each line as "<TAG> <message>" with TAG one of "[INFO]"/"[WARN]"/"[ERROR]"/"[DEBUG]"
    (verbosity.h's FILE_PATH_LOG_* - confirmed against v1.22.2 source) and no timestamp by default. This
    class only assumes lines are appended to a growing text file - it does not depend on which exact text
    a build logs, so it works for any regex a caller already knows to expect."""

    def __init__(self, path, from_start=False):
        self.path = path
        self._pos = 0
        if not from_start and os.path.exists(path):
            self._pos = os.path.getsize(path)

    def new_lines(self):
        if not os.path.exists(self.path):
            return []
        with open(self.path, 'r', encoding='utf-8', errors='replace') as f:
            f.seek(self._pos)
            text = f.read()
            self._pos = f.tell()
        return text.splitlines()

    def wait_for(self, pattern, timeout=15.0, poll=0.2):
        rx = re.compile(pattern)
        end = time.time() + timeout
        seen = []
        while time.time() < end:
            for line in self.new_lines():
                seen.append(line)
                if rx.search(line):
                    return line
            time.sleep(poll)
        raise RaError('{!r} not seen in {} within {}s (last lines: {})'.format(
            pattern, self.path, timeout, seen[-5:]))


_NUMBER_RE = re.compile(r'^\d+(\.\d+)?$')


def run_script(client, script, shots=None, log=None):
    """Runs a ';'-separated script against `client`, returning one reply string per command - the same
    shape as ab_drive.py's Driver.run(). See the module docstring for the command list."""
    out = []
    for part in script.split(';'):
        part = part.strip()
        if not part:
            continue
        words = part.split()
        head = words[0].lower()
        if head == 'press':
            client.press(words[1])
            out.append('ok press ' + words[1])
        elif head == 'wait':
            time.sleep(float(words[1]) / 1000.0)
            out.append('ok wait')
        elif head == 'wait_status':
            timeout = float(words[2]) if len(words) > 2 else 15.0
            out.append(str(client.wait_status(words[1], timeout)))
        elif head == 'shot':
            if shots is None:
                raise RaError('shot needs --shots DIR')
            dest = os.path.abspath(part.split(None, 1)[1].strip())
            src = client.screenshot(shots)
            os.makedirs(os.path.dirname(dest), exist_ok=True)
            shutil.copy(src, dest)
            out.append('ok ' + dest)
        elif head == 'wait_shot':
            if shots is None:
                raise RaError('wait_shot needs --shots DIR')
            rest = part.split(None, 1)[1].split()
            ref, timeout, threshold = rest[0], 15.0, 6
            if len(rest) > 1:
                timeout = float(rest[1])
            if len(rest) > 2:
                threshold = int(rest[2])
            out.append(client.wait_shot(ref, shots, timeout, threshold))
        elif head == 'wait_log':
            if log is None:
                raise RaError('wait_log needs --log FILE')
            rest = part.split(None, 1)[1].split()
            if len(rest) > 1 and _NUMBER_RE.match(rest[-1]):
                timeout, pattern = float(rest[-1]), ' '.join(rest[:-1])
            else:
                timeout, pattern = 15.0, ' '.join(rest)
            out.append(log.wait_for(pattern, timeout))
        elif head == 'menu':
            n = int(words[1])
            client.command('MENU_TOGGLE')
            for _ in range(n):
                client.press('down')
            client.press('a')
            out.append('ok menu ' + str(n))
        elif head == 'cmd':
            rest = part.split(None, 1)[1] if len(words) > 1 else ''
            name, _, arg = rest.partition(' ')
            result = client.command(name, arg or None)
            out.append('ok' if result is None else result)
        else:
            name, _, arg = part.partition(' ')
            result = client.command(name, arg or None)
            out.append('ok' if result is None else result)
    return out


# ---------------------------------------------------------------------------------------------------------
# `start`: Linux only (Xvfb). See the module docstring's "Where this runs".

CFG_TEMPLATE = """network_cmd_enable = "true"
network_cmd_port = "{port}"
video_driver = "gl"
video_fullscreen = "false"
# video_driver stays "gl" (never "sdl2"): RetroArch 1.22.2's AppImage, driven by ra_drive.py under Xvfb,
# segfaults 100% of the time with video_driver=sdl2 - an XScreenSaverQueryExtension() call in its bundled
# libXss.so.1/libXext.so.6 crashes inside the host's libX11.so.6 (Xlib extension-registration ABI mismatch),
# core- and screen-size-independent. gl/glcore never crash. See R22-phase1-root-cause.md for the gdb
# backtrace and the driver matrix.
video_windowed_fullscreen = "false"
audio_driver = "null"
log_to_file = "true"
log_to_file_timestamp = "false"
log_dir = "{log_dir}"
screenshot_directory = "{screenshots}"
menu_driver = "rgui"
"""
# log_to_file_timestamp="false" + log_dir set is what makes RetroArch write a stable "<log_dir>/
# retroarch.log" (verbosity.c's rarch_log_file_init - confirmed this session: log_dir empty means no log
# file at all, "true" timestamps the name per-run, "retroarch__%Y_%m_%d__%H_%M_%S.log"). screenshot_
# directory must already exist as a directory or configuration.c silently ignores it (confirmed:
# configuration.c's parse_config() - "is not an existing directory, ignoring...") - start() below creates
# both directories before writing the cfg.


def cmd_start(args):
    if sys.platform not in ('linux', 'linux2'):
        print('ra_drive.py start needs Xvfb (Linux only) - run this on the Debian test machine '
              '(192.168.68.148), not here. See the module docstring, "Where this runs".')
        return 1

    def opt(name, default):
        return args[args.index(name) + 1] if name in args else default

    port = int(opt('--port', str(DEFAULT_PORT)))
    work_dir = os.path.abspath(opt('--cfg', os.path.join(REPO, 'build_ra_drive')))
    retroarch_bin = opt('--retroarch', 'retroarch')
    core = opt('--core', None)
    display = opt('--display', ':99')

    screenshots_dir = os.path.join(work_dir, 'screenshots')
    os.makedirs(screenshots_dir, exist_ok=True)
    cfg_path = os.path.join(work_dir, 'retroarch.cfg')
    with open(cfg_path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(CFG_TEMPLATE.format(port=port, log_dir=work_dir, screenshots=screenshots_dir))

    log_path = os.path.join(work_dir, 'retroarch.log')
    if os.path.exists(log_path):
        os.remove(log_path)  # log_to_file_timestamp=false overwrites on launch anyway; start clean either way

    env = dict(os.environ)
    env['LIBGL_ALWAYS_SOFTWARE'] = '1'
    # --verbose is required, not optional: R22 phase 2 (2026-09-27) found that without it, RetroArch 1.22.2
    # opens log_dir's retroarch.log but never writes anything past its startup banner into it, no matter how
    # long the instance runs - wait_log (and any script reading --log) would poll an effectively-empty file
    # forever. Confirmed live: a --verbose instance's log grows with real INFO/WARN lines from the first
    # second; without it, the file exists but stays static.
    cmd = ['xvfb-run', '-a', '--server-args=-screen 0 1280x720x24 -displayfd 1',
           retroarch_bin, '-c', cfg_path, '--verbose']
    if core:
        cmd += ['-L', core]
    # start_new_session=True (POSIX setsid()): R22 phase 2 (2026-09-27) found that stop's SIGTERM to
    # proc.pid never reached the actual retroarch process - proc.pid is xvfb-run's own wrapper shell,
    # and killing just that PID leaves Xvfb and retroarch (both separate PIDs the wrapper spawned)
    # running. Starting a new session makes proc.pid the process GROUP id too, so cmd_stop can
    # os.killpg() the whole tree (wrapper + Xvfb + retroarch) in one signal.
    proc = subprocess.Popen(cmd, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL,
                             start_new_session=True)

    with open(os.path.join(work_dir, 'ra_drive.pid'), 'w', encoding='utf-8', newline='\n') as f:
        f.write(str(proc.pid))

    client = RaClient(DEFAULT_HOST, port)
    for _ in range(100):
        time.sleep(0.2)
        if proc.poll() is not None:
            raise RaError('retroarch exited with {} before answering - check {}'.format(
                proc.returncode, log_path))
        try:
            # VERSION, never GET_STATUS: R22 phase 2 (2026-09-27) found GET_STATUS reliably segfaults this
            # RetroArch 1.22.2 build the instant it is answered while a core is actively running (PLAYING) -
            # reproduced with three unrelated cores (2048, mrboom, a from-scratch NES ROM under fceumm), gdb-
            # confirmed as the same crash address inside RetroArch's own binary each time (not a core bug),
            # independent of contentless vs real ROM content. See R22-phase2-report.md. VERSION answers just
            # as well for "is the command port up yet" and never touches that code path.
            client.command('VERSION')
            break
        except RaError:
            # request() already turns a timeout/refused-connection into RaError; keep polling until the
            # process either answers or exits (checked above) or the retry budget below runs out.
            continue
    else:
        raise RaError('the command port never answered (cfg {}, log {})'.format(cfg_path, log_path))
    client.close()
    print('started pid {} on port {}, cfg {}, log {}, screenshots {}'.format(
        proc.pid, port, cfg_path, log_path, screenshots_dir))
    return 0


def cmd_stop(host, port, work_dir=None):
    # R22 phase 2 (2026-09-27) found two bugs here, both fixed below: (1) QUIT alone does not terminate
    # this RetroArch 1.22.2 build under Xvfb (confirmed: a fresh instance sent bare QUIT was still running
    # 5+ seconds later) - it's sent anyway, on the chance a future build honours it, but it is never relied
    # on by itself; (2) the pidfile path was hardcoded to build_ra_drive, so it never matched a --cfg DIR
    # session - work_dir (== start's own --cfg) is now required to find the right one. proc.pid recorded by
    # cmd_start is a process GROUP id (start_new_session=True) - os.killpg() reaches the whole
    # xvfb-run/Xvfb/retroarch tree, where os.kill() on that one pid alone only ever reached xvfb-run's own
    # wrapper shell and left Xvfb/retroarch running.
    try:
        client = RaClient(host, port)
        client.command('QUIT')
        client.close()
    except OSError:
        pass

    if host not in ('127.0.0.1', 'localhost'):
        print('stopped')
        return

    pid_path = os.path.join(work_dir or os.path.join(REPO, 'build_ra_drive'), 'ra_drive.pid')
    if not os.path.exists(pid_path):
        print('stopped (no pidfile at {} - nothing else to do)'.format(pid_path))
        return

    pgid = int(open(pid_path, encoding='utf-8').read().strip())

    def group_alive():
        try:
            os.killpg(pgid, 0)  # signal 0: no-op, just checks the group still exists
            return True
        except OSError:
            return False

    time.sleep(0.5)  # give QUIT above a moment, in case some future build does honour it
    for _ in range(10):  # up to ~5s for a clean SIGTERM exit
        if not group_alive():
            break
        try:
            os.killpg(pgid, signal.SIGTERM)
        except OSError:
            break
        time.sleep(0.5)
    if group_alive():
        try:
            os.killpg(pgid, signal.SIGKILL)
        except OSError:
            pass
        time.sleep(0.5)
    os.remove(pid_path)
    print('stopped ({})'.format('group still reported alive after SIGKILL - check ps' if group_alive()
                                 else 'group exited'))


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
    args = argv[2:]
    host = DEFAULT_HOST
    port = DEFAULT_PORT
    shots = None
    logpath = None
    # --cfg is peeked (not stripped from args): cmd_start does its own --cfg parsing straight out of args,
    # so it must stay in the list for that path. 'stop' has no other use for args, so cmd_stop is handed
    # the value directly instead.
    cfg_dir = os.path.abspath(args[args.index('--cfg') + 1]) if '--cfg' in args else None
    if '--host' in args:
        i = args.index('--host')
        host = args[i + 1]
        del args[i:i + 2]
    if '--port' in args:
        i = args.index('--port')
        port = int(args[i + 1])
        del args[i:i + 2]
    if '--shots' in args:
        i = args.index('--shots')
        shots = args[i + 1]
        del args[i:i + 2]
    if '--log' in args:
        i = args.index('--log')
        logpath = args[i + 1]
        del args[i:i + 2]

    try:
        if cmd == 'start':
            return cmd_start(args)
        if cmd == 'stop':
            cmd_stop(host, port, cfg_dir)
            return 0
        if cmd == 'sheet':
            sheet(args[0], args[1:])
            return 0

        client = RaClient(host, port)
        log = LogTail(logpath) if logpath else None
        try:
            script = ' '.join(args) if cmd == 'run' else (cmd + (' ' + ' '.join(args) if args else ''))
            for line in run_script(client, script, shots, log):
                print(line)
        finally:
            client.close()
        return 0
    except RaError as e:
        # every "nothing answered"/"bad input" case (see request()'s docstring) lands here as one clear
        # line on stderr and exit code 1 - never a raw socket traceback.
        print('error: {}'.format(e), file=sys.stderr)
        return 1


if __name__ == '__main__':
    sys.exit(main(sys.argv))
