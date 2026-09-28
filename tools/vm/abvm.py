#!/usr/bin/env python3
"""abvm - a test VM on the test machine, driven from a dev PC: put a build in, restart it, press its pad, take its
screen. For the loop "code -> build -> install in the VM -> try it -> fix" without the owner's devices.

  python tools/vm/abvm.py status                         the VM, the guest, the launcher, padsim, the DebugDriver
  python tools/vm/abvm.py setup                          copy this tool to the test machine (needed once, and after
                                                         every change to it: `run` and `pad` execute there)
  python tools/vm/abvm.py install FILE [GUEST_PATH]      put a file in the guest; the original is kept once as
                                                         <path>.abvm-orig. GUEST_PATH: an absolute path, or
                                                         `launcher` (next to the running autobleem-gui) or
                                                         `ext:<name>` (<root>/Extensions/<name>/bin/<key>/)
  python tools/vm/abvm.py restore                        every file `install` replaced back as it was
  python tools/vm/abvm.py restart                        restart the launcher, wait until its DebugDriver answers
  python tools/vm/abvm.py shot OUT.png                   the VM's whole screen (the emulator and Apps included)
  python tools/vm/abvm.py pad "<script>"                 the virtual pad: press a; wait 300; dpad down; ...
  python tools/vm/abvm.py run "<script>" [--out DIR]     pad steps and screenshots in one go, timed on the test
                                                         machine; the shots land in DIR (default: the cwd)
  python tools/vm/abvm.py drive "<script>"               tools/ab_drive.py's script through a tunnel to the guest
  python tools/vm/abvm.py padsim-install                 build tools/vm/padsim.c in the guest and restart it
  python tools/vm/abvm.py guest "<command>"              a shell command in the guest (it is a sandbox)

Scripts: steps separated by ';'. Pad steps: press/release <btn>, hold <btn> <ms>, tap <btn> (a 120 ms hold),
stick <left|right> <x> <y>, trigger <l2|r2> <0..255>, dpad <dir|center>, reset; profile <x360|ds4|generic>
[usb|bt] (the pad replugged as that pad - ds4 over bt is a Bluetooth DualShock 4, generic one SDL has no mapping
for), unplug, plug, battery <0..100> | battery off, cable in|out. Up to four pads: `@2 profile ds4 bt` sends to pad
2 (no @ = pad 1; pads 2-4 start unplugged). Buttons: a b x y l1 r1 l2 r2
select start guide l3 r3 - the Xbox names on every profile (a = Cross, b = Circle, x = Square, y = Triangle).
`wait <ms>`, and in `run` also `shot <name.png>`. tools/vm/padsim.c's opening comment has the details.

Where things are comes from the environment, never from this file (no addresses in the repository):
  ABVM_HOST      the test machine's ssh name (default: bleemmachine - a Host entry in ~/.ssh/config)
  ABVM_DOMAIN    the libvirt VM (default: pcusb-test)
  ABVM_GUEST     user@address of the guest (default: autobleem@<the address libvirt reports for the VM>)
  ABVM_KEY       the guest's ssh key on the test machine (default: ~/.ssh/<domain>-vm_ed25519)
  ABVM_PADSIM    the padsim channel's socket on the test machine (default: /tmp/<domain>-padsim.sock)
  ABVM_DRIVER    the guest's DebugDriver port (default: 6900); ABVM_LOCAL_PORT the local end (default: 16900)
docs/pc-test-machine.md (autobleem-main) describes the machine, the VM and padsim.

Never pipes into ssh (on Windows the EOF never arrives): files go by scp, commands as arguments.
"""
import os
import re
import shlex
import shutil
import socket
import subprocess
import sys
import tempfile
import time
import uuid

HERE = os.path.dirname(os.path.abspath(__file__))
REMOTE_TOOL = '~/.local/share/abvm/abvm.py'

HOST = os.environ.get('ABVM_HOST', 'bleemmachine')
DOMAIN = os.environ.get('ABVM_DOMAIN', 'pcusb-test')
KEY = os.environ.get('ABVM_KEY', f'~/.ssh/{DOMAIN}-vm_ed25519')
PADSIM = os.environ.get('ABVM_PADSIM', f'/tmp/{DOMAIN}-padsim.sock')
DRIVER_PORT = int(os.environ.get('ABVM_DRIVER', '6900'))
LOCAL_PORT = int(os.environ.get('ABVM_LOCAL_PORT', '16900'))
VIRSH = 'virsh -c qemu:///system'

# on the test machine itself (`--local`, which `setup`'s copy is always called with) nothing goes through ssh
LOCAL = False


class Fail(Exception):
    pass


# ------------------------------------------------------------------ the test machine and the guest

def host_run(command, check=True):
    """a shell command on the test machine; its stdout"""
    if LOCAL:
        args = ['bash', '-c', command]
    else:
        args = ['ssh', '-o', 'BatchMode=yes', HOST, command]
    r = subprocess.run(args, stdin=subprocess.DEVNULL, capture_output=True, text=True)
    if check and r.returncode != 0:
        raise Fail(f'{command!r} on the test machine: exit {r.returncode}: {r.stderr.strip()}')
    return r.stdout


_guest = None


def guest_address():
    global _guest
    if _guest:
        return _guest
    _guest = os.environ.get('ABVM_GUEST')
    if not _guest:
        out = host_run(f'{VIRSH} domifaddr {shlex.quote(DOMAIN)}')
        found = re.findall(r'ipv4\s+(\d+\.\d+\.\d+\.\d+)', out)
        if not found:
            raise Fail(f'libvirt reports no address for {DOMAIN} - is it running? ({VIRSH} domstate {DOMAIN})')
        _guest = 'autobleem@' + found[0]
    return _guest


def guest_ssh():
    return f'ssh -i {KEY} -o BatchMode=yes -o ConnectTimeout=5 -o StrictHostKeyChecking=accept-new {guest_address()}'


def guest_run(command, check=True):
    return host_run(f'{guest_ssh()} {shlex.quote(command)}', check)


def to_host(local_path, host_path):
    if LOCAL:
        shutil.copy(local_path, os.path.expanduser(host_path))
    else:
        subprocess.run(['scp', '-q', '-o', 'BatchMode=yes', local_path, f'{HOST}:{host_path}'],
                       stdin=subprocess.DEVNULL, check=True)


def from_host(host_path, local_path, recursive=False):
    if LOCAL:
        if recursive:
            shutil.copytree(os.path.expanduser(host_path), local_path, dirs_exist_ok=True)
        else:
            shutil.copy(os.path.expanduser(host_path), local_path)
    else:
        args = ['scp', '-q', '-o', 'BatchMode=yes'] + (['-r'] if recursive else [])
        subprocess.run(args + [f'{HOST}:{host_path}', local_path], stdin=subprocess.DEVNULL, check=True)


def to_guest(local_path, guest_path):
    """local -> the test machine's /tmp -> the guest"""
    stage = f'/tmp/abvm-{uuid.uuid4().hex[:8]}-{os.path.basename(local_path)}'
    to_host(local_path, stage)
    try:
        host_run(f'scp -q -i {KEY} -o BatchMode=yes {stage} {guest_address()}:{shlex.quote(guest_path)}')
    finally:
        host_run(f'rm -f {stage}', check=False)


# ------------------------------------------------------------------ the launcher in the guest

def launcher_exe():
    """the running autobleem-gui's path in the guest"""
    # the launcher runs as another user: its /proc/<pid>/exe is root's to read
    out = guest_run('for p in $(pidof autobleem-gui); do sudo readlink /proc/$p/exe; done', check=False).split()
    if not out:
        raise Fail('the launcher is not running in the guest')
    return out[0]


def resolve_target(name, target):
    if target is None or target == 'launcher':
        return os.path.join(os.path.dirname(launcher_exe()), name).replace('\\', '/')
    if target.startswith('ext:'):
        exe = launcher_exe()  # <root>/Autobleem/bin/autobleem/autobleem-gui
        root = exe.rsplit('/Autobleem/', 1)[0]
        key = guest_run('ls ' + shlex.quote(f'{root}/Extensions/{target[4:]}/bin')).split()
        if len(key) != 1:
            raise Fail(f'{root}/Extensions/{target[4:]}/bin has {key}, not one platform folder')
        return f'{root}/Extensions/{target[4:]}/bin/{key[0]}/{name}'
    if target.endswith('/'):
        return target + name
    return target


def install(path, target):
    dest = resolve_target(os.path.basename(path), target)
    stage = f'/tmp/abvm-{uuid.uuid4().hex[:8]}'
    to_guest(path, stage)
    q = shlex.quote(dest)
    # the original once (a second install keeps the first original), the list of what was replaced on the root
    # filesystem, the new file moved into place so a running program never sees half a file
    guest_run(f'sudo sh -c "set -e; if [ -e {q} ] && [ ! -e {q}.abvm-orig ]; then cp -p {q} {q}.abvm-orig; '
              f'mkdir -p /var/lib/abvm; echo {q} >> /var/lib/abvm/installed; fi; '
              f'cp {stage} {q}.abvm-new; chmod --reference={q}.abvm-orig {q}.abvm-new 2>/dev/null || chmod 755 {q}.abvm-new; '
              f'mv -f {q}.abvm-new {q}; rm -f {stage}; sync"')
    print(f'installed {dest}')


def restore():
    out = guest_run('cat /var/lib/abvm/installed 2>/dev/null', check=False).split()
    for path in dict.fromkeys(out):
        q = shlex.quote(path)
        guest_run(f'sudo sh -c "if [ -e {q}.abvm-orig ]; then mv -f {q}.abvm-orig {q}; fi"')
        print(f'restored {path}')
    guest_run('sudo rm -f /var/lib/abvm/installed; sync')
    if not out:
        print('nothing to restore')


def driver_answers():
    out = guest_run(f'ss -ltn 2>/dev/null | grep -c ":{DRIVER_PORT} " || true', check=False).strip()
    return out not in ('', '0')


def restart(timeout=90):
    guest_run('sudo systemctl restart autobleem.service')
    end = time.time() + timeout
    time.sleep(2)
    while time.time() < end:
        if driver_answers():
            print('launcher up, DebugDriver listening')
            return
        time.sleep(1)
    raise Fail(f'the launcher did not open its DebugDriver in {timeout} s (journalctl -u autobleem in the guest)')


# ------------------------------------------------------------------ the screen

def shot(out):
    stage = f'/tmp/abvm-{uuid.uuid4().hex[:8]}.img'
    host_run(f'{VIRSH} screenshot {shlex.quote(DOMAIN)} {stage} >/dev/null')
    try:
        tmp = out + '.part'
        from_host(stage, tmp)
    finally:
        host_run(f'rm -f {stage}', check=False)
    save_png(tmp, out)
    print(f'ok {os.path.abspath(out)}')


def save_png(src, out):
    """virsh writes PNG or PPM depending on the QEMU version; always PNG out (Pillow for a PPM)"""
    with open(src, 'rb') as f:
        head = f.read(8)
    if head.startswith(b'\x89PNG'):
        os.replace(src, out)
        return
    try:
        from PIL import Image
    except ImportError:
        os.replace(src, os.path.splitext(out)[0] + '.ppm')
        raise Fail('the screenshot is a PPM and Pillow is not installed: saved as .ppm')
    Image.open(src).save(out)
    os.remove(src)


# ------------------------------------------------------------------ the pad (on the test machine)

class Padsim:
    def __init__(self, path=PADSIM):
        self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.sock.settimeout(10)
        self.sock.connect(os.path.expanduser(path))
        self.buf = b''

    def cmd(self, line):
        self.sock.sendall((line + '\n').encode())
        while b'\n' not in self.buf:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise Fail('padsim closed the connection')
            self.buf += chunk
        reply, self.buf = self.buf.split(b'\n', 1)
        reply = reply.decode(errors='replace')
        if not reply.startswith('ok'):
            raise Fail(f'padsim: {line!r}: {reply}')
        return reply

    def close(self):
        self.sock.close()


PAD_WORDS = {'press', 'release', 'hold', 'stick', 'trigger', 'dpad', 'reset', 'ping', 'profile', 'plug', 'unplug',
             'battery'}


def steps(script):
    return [s.strip() for s in script.split(';') if s.strip()]


def run_local(script, out_dir):
    """on the test machine: every step at its own time; shots through virsh into out_dir"""
    pad = None
    try:
        for step in steps(script):
            words = step.split()
            if words[0] == 'wait':
                time.sleep(int(words[1]) / 1000)
            elif words[0] == 'shot':
                os.makedirs(out_dir, exist_ok=True)
                name = os.path.basename(step.split(None, 1)[1].strip())
                path = os.path.join(out_dir, name)
                subprocess.run(VIRSH.split() + ['screenshot', DOMAIN, path + '.part'], check=True,
                               stdout=subprocess.DEVNULL)
                os.replace(path + '.part', path)
                print(f'shot {name}')
            elif re.fullmatch(r'@[1-4]', words[0]) or words[0] in PAD_WORDS or words[0] == 'tap':
                # "@2 press a": the pad's number stays in front of whatever the step becomes
                target = words.pop(0) + ' ' if words[0].startswith('@') else ''
                if not words or not (words[0] in PAD_WORDS or words[0] == 'tap'):
                    raise Fail(f'unknown step {step!r}')
                if pad is None:
                    pad = Padsim()
                    pad.cmd('ping')
                if words[0] == 'tap':
                    step = f'hold {words[1]} {words[2] if len(words) > 2 else 120}'
                else:
                    step = ' '.join(words)
                print(pad.cmd(target + step))
            else:
                raise Fail(f'unknown step {step!r}')
    finally:
        if pad:
            pad.close()


def remote_tool(args):
    return host_run(f'python3 {REMOTE_TOOL} --local ' + ' '.join(shlex.quote(a) for a in args))


def run(script, out_dir):
    if LOCAL:
        run_local(script, out_dir)
        return
    stage = f'/tmp/abvm-run-{uuid.uuid4().hex[:8]}'
    try:
        print(remote_tool(['run', script, '--out', stage]), end='')
        if 'shot ' in script:
            os.makedirs(out_dir, exist_ok=True)
            tmp = tempfile.mkdtemp(prefix='abvm-')
            from_host(stage + '/.', tmp, recursive=True)
            for name in os.listdir(tmp):
                save_png(os.path.join(tmp, name), os.path.join(out_dir, name))
            shutil.rmtree(tmp, ignore_errors=True)
    finally:
        host_run(f'rm -rf {stage}', check=False)


# ------------------------------------------------------------------ the DebugDriver through a tunnel

def drive(script):
    sys.path.insert(0, os.path.join(HERE, '..'))
    import ab_drive
    # the guest's driver listens on its own loopback; the test machine's permanent user service
    # (<domain>-debugdriver) forwards its 127.0.0.1:<port> there - on the test machine that is used as it is,
    # from a PC one more forward to it (nothing listens on the LAN)
    tunnel = None
    port = DRIVER_PORT
    if not LOCAL:
        port = LOCAL_PORT
        tunnel = subprocess.Popen(['ssh', '-o', 'BatchMode=yes', '-N', '-L',
                                   f'{LOCAL_PORT}:127.0.0.1:{DRIVER_PORT}', HOST], stdin=subprocess.DEVNULL)
    try:
        d = None
        for _ in range(50):
            try:
                d = ab_drive.Driver(port)
                break
            except OSError:
                time.sleep(0.2)
        if d is None:
            raise Fail('the DebugDriver did not answer through the tunnel')
        try:
            for reply in d.run(script):
                print(reply)
        finally:
            d.close()
    finally:
        if tunnel:
            tunnel.terminate()


# ------------------------------------------------------------------ status, setup, padsim

def status():
    state = host_run(f'{VIRSH} domstate {shlex.quote(DOMAIN)}', check=False).strip() or 'unknown'
    print(f'vm        {DOMAIN}: {state}')
    if state != 'running':
        return 1
    print(f'guest     {guest_address()}')
    units = guest_run('systemctl is-active autobleem.service padsim.service', check=False).split()
    print(f'launcher  {units[0] if units else "?"}' + (f' ({launcher_exe()})' if units[:1] == ['active'] else ''))
    print(f'padsim    {units[1] if len(units) > 1 else "?"}')
    print(f'driver    {"listening" if driver_answers() else "not listening"} on the guest\'s :{DRIVER_PORT}')
    tool = host_run(f'test -f {REMOTE_TOOL} && echo yes || echo no', check=False).strip() if not LOCAL else 'yes'
    print(f'abvm      {"on the test machine" if tool == "yes" else "not on the test machine - run `abvm.py setup`"}')
    return 0


def setup():
    host_run('mkdir -p ~/.local/share/abvm')
    to_host(os.path.abspath(__file__), REMOTE_TOOL)
    print(f'copied to {HOST}:{REMOTE_TOOL}')


BATTERY_DROPIN = '/etc/systemd/system/autobleem.service.d/padsim-battery.conf'


def padsim_install():
    src = os.path.join(HERE, 'padsim.c')
    to_guest(src, '/tmp/padsim.c')
    # the launcher reads pad batteries from padsim's fake power_supply tree instead of /sys (the VM has no real
    # wireless pad) - a drop-in on the guest's root filesystem, like the DebugDriver's
    guest_run('set -e; gcc -O2 -Wall -o /tmp/padsim /tmp/padsim.c; '
              'sudo install -m 755 /tmp/padsim /usr/local/bin/padsim; '
              'sudo install -D -m 644 /tmp/padsim.c /usr/local/src/padsim/padsim.c; '
              f'printf "[Service]\\nEnvironment=AB_PAD_BATTERY_DIR=/run/padsim/power_supply\\n" > /tmp/pb.conf; '
              f'sudo install -D -m 644 /tmp/pb.conf {BATTERY_DROPIN}; rm -f /tmp/pb.conf; '
              'sudo systemctl daemon-reload; sudo systemctl restart padsim.service; '
              'rm -f /tmp/padsim /tmp/padsim.c; sleep 1; systemctl is-active padsim.service')
    print('padsim built and restarted; the launcher reads its battery after `abvm.py restart`')


def main(argv):
    global LOCAL
    if '--local' in argv:
        LOCAL = True
        argv = [a for a in argv if a != '--local']
    out_dir = '.'
    if '--out' in argv:
        i = argv.index('--out')
        out_dir = argv[i + 1]
        del argv[i:i + 2]
    if not argv:
        print(__doc__)
        return 2
    cmd, args = argv[0], argv[1:]
    try:
        if cmd == 'status':
            return status()
        elif cmd == 'setup':
            setup()
        elif cmd == 'install':
            install(args[0], args[1] if len(args) > 1 else None)
        elif cmd == 'restore':
            restore()
        elif cmd == 'restart':
            restart()
        elif cmd == 'shot':
            shot(args[0])
        elif cmd == 'pad':
            run(' '.join(args), out_dir)
        elif cmd == 'run':
            run(' '.join(args), out_dir)
        elif cmd == 'drive':
            drive(' '.join(args))
        elif cmd == 'padsim-install':
            padsim_install()
        elif cmd == 'guest':
            print(guest_run(' '.join(args)), end='')
        else:
            print(__doc__)
            return 2
    except (Fail, subprocess.CalledProcessError) as e:
        print(f'abvm: {e}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
