#!/usr/bin/env python3
"""Drives the launcher through its DebugDriver (lib_ableem/include/ableem/ui/debug_driver.h) - a pad, a
keyboard and a screenshot over a socket, for automated looks at the UI without touching the user's screen.

  python tools/ab_drive.py start [--usb DIR] [--port N] [--show]   start the dev build on the usb tree with
                                                                   AB_DEBUG_PORT and no splash; hidden
                                                                   (--show: visible)
  python tools/ab_drive.py stop                                    a Quit event, then the process is killed
  python tools/ab_drive.py run "<script>"                          commands separated by ';', e.g.
                                   "down l2; press r2; up l2; wait 300; shot menu.png; press o"
  python tools/ab_drive.py <command> ...                           one command, e.g. `shot a.png`, `press x`
  python tools/ab_drive.py sheet OUT.png IN1.png IN2.png ...       a contact sheet of shots (Pillow)

The script language is the driver's: press <btn> [ms], down/up <btn>, key <name>, text <utf8>, wait <ms>,
shot <file>, frames, screen, window hide|show|min|restore, quit. Buttons: x o s t start select l1 r1 l2 r2
up down left right. A `shot` waits for a frame drawn after the last input, so "press x; shot a.png" shows
the result of the press. Shots land where the path says (relative to this process's cwd, made absolute).
Two of the client's own: `wait_screen <Name> [timeout s]` polls `screen` until that screen shows (a
GuiScreen class name: GuiLauncher, GuiOptions, GuiConfirm, GuiSystemMenu, ...) - `start` waits for
GuiLauncher itself, so a script may press at once - and `menu <item>` opens the L2+R2 system menu and
picks an item by its 0-based index.

The launcher is started with the exe from build_win/ copied into the usb tree first (the resources next to
it are what tools/make_usb.py staged).
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


def pid_file(port):
    return os.path.join(REPO, 'build_win', f'ab_drive-{port}.pid')


class Driver:
    def __init__(self, port=DEFAULT_PORT):
        self.sock = socket.create_connection(('127.0.0.1', port), timeout=10)
        self.buf = b''

    def cmd(self, line):
        self.sock.sendall((line + '\n').encode('utf-8'))
        while b'\n' not in self.buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise RuntimeError('driver closed the connection')
            self.buf += chunk
        reply, self.buf = self.buf.split(b'\n', 1)
        reply = reply.decode('utf-8', 'replace')
        if reply.startswith('err'):
            raise RuntimeError(f'{line!r}: {reply}')
        return reply

    def wait_screen(self, name, timeout=15.0):
        end = time.time() + timeout
        while time.time() < end:
            reply = self.cmd('screen')
            current = reply.split(' ', 1)[1] if ' ' in reply else ''
            if current == name:
                return 'ok ' + name
            time.sleep(0.05)
        raise RuntimeError(f'screen {name} did not show (now: {current})')

    def menu(self, index):
        self.cmd('down l2')
        self.cmd('press r2')
        self.cmd('up l2')
        self.wait_screen('GuiSystemMenu')
        for _ in range(int(index)):
            self.cmd('press down 40')
        self.cmd('press x')
        return 'ok'

    def run(self, script):
        out = []
        for part in script.split(';'):
            part = part.strip()
            if not part:
                continue
            words = part.split()
            if words[0] == 'shot':
                path = os.path.abspath(part.split(None, 1)[1].strip())
                part = 'shot ' + path
            elif words[0] == 'wait_screen':
                out.append(self.wait_screen(words[1], float(words[2]) if len(words) > 2 else 15.0))
                continue
            elif words[0] == 'menu':
                out.append(self.menu(words[1]))
                continue
            out.append(self.cmd(part))
        return out

    def close(self):
        self.sock.close()


def start(usb, port, show):
    exe = os.path.join(REPO, 'build_win', 'autobleem-gui.exe')
    app_dir = os.path.join(usb, 'Autobleem', 'bin', 'autobleem')
    shutil.copy(exe, os.path.join(app_dir, 'autobleem-gui.exe'))
    env = dict(os.environ)
    env['AB_DEBUG_PORT'] = str(port)
    env['AB_NO_SPLASH'] = '1'  # straight to the launcher (GuiSplash honours it on a dev host)
    env['PATH'] = r'C:\msys64\ucrt64\bin;' + env.get('PATH', '')
    proc = subprocess.Popen([os.path.join(app_dir, 'autobleem-gui.exe'), usb], cwd=app_dir, env=env,
                            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    with open(pid_file(port), 'w') as f:
        f.write(str(proc.pid))
    # the driver comes up after the library is open (a few seconds on a first scan)
    for _ in range(300):
        time.sleep(0.1)
        try:
            d = Driver(port)
            break
        except OSError:
            if proc.poll() is not None:
                raise RuntimeError(f'the launcher exited with {proc.returncode}')
    else:
        raise RuntimeError('the driver did not answer')
    if not show:
        d.cmd('window hide')
    d.wait_screen('GuiLauncher', 60)
    d.close()
    print(f'started pid {proc.pid} on port {port}' + ('' if show else ', hidden'))


def stop(port):
    try:
        d = Driver(port)
        d.cmd('quit')
        d.close()
        time.sleep(0.5)
    except OSError:
        pass
    if os.path.exists(pid_file(port)):
        pid = int(open(pid_file(port)).read().strip())
        subprocess.call(['taskkill', '/PID', str(pid), '/F'], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
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
    args = argv[2:]
    if '--port' in args:
        i = args.index('--port')
        port = int(args[i + 1])
        del args[i:i + 2]
    if cmd == 'start':
        usb = DEFAULT_USB
        show = '--show' in args
        if '--usb' in args:
            usb = args[args.index('--usb') + 1]
        start(usb, port, show)
        return 0
    if cmd == 'stop':
        stop(port)
        return 0
    if cmd == 'sheet':
        sheet(args[0], args[1:])
        return 0
    d = Driver(port)
    try:
        if cmd == 'run':
            for reply in d.run(' '.join(args)):
                print(reply)
        else:
            print(d.run(cmd + ' ' + ' '.join(args))[0])
    finally:
        d.close()
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
