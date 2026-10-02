#!/usr/bin/env python3
"""abvm - a test VM on the test machine, driven from a dev PC: put a build in, restart it, press its pad, take its
screen. For the loop "code -> build -> install in the VM -> try it -> fix" without the owner's devices.

  python tools/vm/abvm.py status                         the VM, who has it, the guest, the launcher, padsim, the driver
  python tools/vm/abvm.py lock take <task> [minutes]     the VM is shared: take it before anything that changes it
                                                         (default 30 min; taking it again renews it)
  python tools/vm/abvm.py lock status | release          who has it and until when / give it back
  python tools/vm/abvm.py setup                          copy this tool to the test machine (needed once, and after
                                                         every change to it: `run` and `pad` execute there)
  python tools/vm/abvm.py install SRC [GUEST_PATH]       put a file or a directory in the guest; the original is kept
                                                         once as <path>.abvm-orig. SRC: a local path, or
                                                         host:<path> for one on the test machine (a build's
                                                         dist/ there). GUEST_PATH: an absolute path, `launcher`
                                                         (next to the running autobleem-gui; a directory: over the
                                                         launcher's own - the launcher is restarted), `launcher-dir`,
                                                         or `ext:<name>` (<root>/Extensions/<name>/bin/<key>/)
  python tools/vm/abvm.py restore                        every file and directory `install` replaced back as it was
  python tools/vm/abvm.py clip SECONDS OUT.mp4           the VM's screen as a video (ffmpeg on the test machine);
                                                         in `run`: clip start <name.mp4>; ...steps...; clip stop
  python tools/vm/abvm.py restart                        restart the launcher, wait until its DebugDriver answers
  python tools/vm/abvm.py remount                        after a hard reset the stick's exFAT may not mount (udev made no
                                                         /dev/disk/by-uuid link, the mount unit failed "Dependency
                                                         failed"): finds the stick's partition from the mount unit,
                                                         `udevadm trigger` on it, starts the mount and the launcher;
                                                         `restart` does the same first; `status` only reports it.
                                                         Never formats or fsck-repairs.
  python tools/vm/abvm.py shot OUT.png                   the VM's whole screen (the emulator and Apps included)
  python tools/vm/abvm.py pad "<script>"                 the virtual pad: press a; wait 300; dpad down; ...
  python tools/vm/abvm.py run "<script>" [--out DIR]     pad steps and screenshots in one go, timed on the test
                                                         machine; the shots land in DIR (default: the cwd)
  python tools/vm/abvm.py drive "<script>"               tools/ab_drive.py's script through a tunnel to the guest
  python tools/vm/abvm.py padsim-install                 build tools/vm/padsim.c in the guest and restart it
  python tools/vm/abvm.py guest "<command>"              a shell command in the guest (it is a sandbox)

Sandboxes - headless launchers in the same VM, next to the stick's own, for testing the application while someone
else holds the VM; each one's whole root is a folder on the test machine (~/abvm/sandboxes/<name>, the VM's 9p share
at /mnt/abvm), never the stick. They need their own lease, not the VM's:
  python tools/vm/abvm.py sandbox setup                  once per VM (the VM's lease): the guest mounts the share at
                                                         boot, the cover DBs shared read-only (_shared/db)
  python tools/vm/abvm.py sandbox template               _template from the stick: its launcher, themes, extensions,
                                                         Apps; no games, empty databases
  python tools/vm/abvm.py sandbox take <name> <task> [min] / release <name>        the sandbox's lease
  python tools/vm/abvm.py sandbox new <name> [--build <dir>] [--ext <zip|dir>]...   made from the template (not started)
                                                         (with System/Extensions/store/{cache,downloads,staging,sources})
  python tools/vm/abvm.py sandbox start <name> [--build <dir>] [--ext <zip|dir>]... [--size WxH]   made from the
                                                         template when new; --build lays a build's dist/<target> (on
                                                         the test machine, e.g. ~/src/autobleem/dist/pcusb) over it;
                                                         --ext (repeatable) lays an extension over it: a zip is
                                                         unzipped at the root (it holds Extensions/<name>/...), a
                                                         directory .../extensions/<name>/ is copied to
                                                         Extensions/<name>/; the replaced one is kept once in
                                                         <sandbox>/.abvm/ext-backup/<name>; started headless
                                                         in a WxH window (1280x720; ABVM_SANDBOX_SIZE), ready when
                                                         its launcher screen shows
  python tools/vm/abvm.py sandbox drive <name> "<script>" [--out DIR]   an ab_drive.py script (@1 tap a; wait_idle
                                                         300; shot a.png; clip start b.mp4; ...; clip stop); shots,
                                                         grabs and clips (MP4, ffmpeg on the test machine) come back
                                                         to DIR - they are written into <sandbox>/.abvm/out/<run>/;
                                                         a failing step stops the run but the shots made before it
                                                         still come back, then one error line and exit code 1
  python tools/vm/abvm.py sandbox logs <name> [n]        the launcher's output
  python tools/vm/abvm.py sandbox stop|reset|rm <name>   quit it (the driver's `quit`, a kill after 5 s) / start it
                                                         afresh from the template / delete it
  python tools/vm/abvm.py sandbox list                   every sandbox, running or not, and its lease
At most 2 sandboxes run at once, a ceiling in the code (the VM has 5 vCPUs and an idle launcher uses a whole CPU - three
of them hung the VM twice, 2026-10-02). ABVM_SANDBOX_SLOTS may only lower it (1). `sandbox start` counts the launchers
that really run in the guest (state files and the guest's own process list) and refuses a third: exit 3, naming the
running ones - `sandbox stop <name>` (with its lease) frees a slot.

Scripts: steps separated by ';'. Pad steps: press/release <btn>, hold <btn> <ms>, tap <btn> (a 120 ms hold),
stick <left|right> <x> <y>, trigger <l2|r2> <0..255>, dpad <dir|center>, reset; profile <x360|ds4|generic>
[usb|bt] (the pad replugged as that pad - ds4 over bt is a Bluetooth DualShock 4, generic one SDL has no mapping
for), unplug, plug, battery <0..100> | battery off, cable in|out. Up to four pads: `@2 profile ds4 bt` sends to pad
2 (no @ = pad 1; pads 2-4 start unplugged). Buttons: a b x y l1 r1 l2 r2
select start guide l3 r3 - the Xbox names on every profile (a = Cross, b = Circle, x = Square, y = Triangle).
`wait <ms>`, and in `run` also `shot <name.png>` (the VM's whole screen) and the launcher's own DebugDriver words
(screen, wait_screen, wait_idle, key, text, grab, menu, quick, items, frames, window, down, up), sent to the stick's
launcher - so a script written with `@1` before each pad step runs the same in `run` and in `sandbox drive`, where
the sandbox launcher's DebugDriver plays padsim's pads itself (tools/ab_drive.py). A USB keyboard: kbd plug|unplug, kbd press|release <key>,
kbd tap <key> [ms], kbd combo <key>+<key>... (ctrl+alt+delete), kbd type <text> (keys: a-z 0-9 enter esc space
tab backspace up down left right f1-f12 home end pageup pagedown insert delete shift ctrl alt meta, ...).
tools/vm/padsim.c's opening comment has the details.

The lease: one tester on the VM at a time. Every command that changes the VM or its screen (install, restore,
restart, clip, pad, run, drive, padsim-install, guest) needs ABVM_WHO=<name> (or --who <name>) and that name's
lease (`lock take`); a lease held by someone else is exit 3 and "busy: <who> (<task>) ... n min left" - do
something else and come back. Every command of the holder keeps the lease at least 10 minutes ahead; a lease
nobody renews runs out by itself. status and shot need none. The lease lives on the test machine
(~/.local/state/abvm/lock.json, under flock).

Where things are comes from the environment, never from this file (no addresses in the repository):
  ABVM_HOST      the test machine's ssh name (default: bleemmachine - a Host entry in ~/.ssh/config)
  ABVM_DOMAIN    the libvirt VM (default: pcusb-test)
  ABVM_GUEST     user@address of the guest (default: autobleem@<the address libvirt reports for the VM>)
  ABVM_KEY       the guest's ssh key on the test machine (default: ~/.ssh/<domain>-vm_ed25519)
  ABVM_PADSIM    the padsim channel's socket on the test machine (default: /tmp/<domain>-padsim.sock)
  ABVM_DRIVER    the guest's DebugDriver port (default: 6900); ABVM_LOCAL_PORT the local end (default: 16900)
  ABVM_WHO       who is testing - the lease's holder
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
import zipfile

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

def host_run(command, check=True, full=False):
    """a shell command on the test machine; its stdout (full: the whole CompletedProcess, whatever its exit code)"""
    if LOCAL:
        args = ['bash', '-c', command]
    else:
        args = ['ssh', '-o', 'BatchMode=yes', HOST, command]
    r = subprocess.run(args, stdin=subprocess.DEVNULL, capture_output=True, text=True, encoding='utf-8',
                       errors='replace')
    if check and not full and r.returncode != 0:
        raise Fail(f'{command!r} on the test machine: exit {r.returncode}: {r.stderr.strip()}')
    return r if full else r.stdout


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
    """the running autobleem-gui's path in the guest - the stick's, never a sandbox's"""
    # the launcher runs as another user: its /proc/<pid>/exe is root's to read
    out = guest_run('for p in $(pidof autobleem-gui); do sudo readlink /proc/$p/exe; done', check=False).split()
    out = [p for p in out if not p.startswith(SB_GUEST + '/')]
    if not out:
        raise Fail('the launcher is not running in the guest')
    return out[0]


def resolve_target(name, target, is_dir=False):
    if target == 'launcher-dir' or (is_dir and target in (None, 'launcher')):
        return os.path.dirname(launcher_exe())
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


def source_to_guest(src, guest_path):
    """a local file, or `host:<path>` - one already on the test machine (a build's output there), sent straight on"""
    if src.startswith('host:'):
        host_run(f'scp -q -i {KEY} -o BatchMode=yes {shlex.quote(src[5:])} {guest_address()}:{shlex.quote(guest_path)}')
    else:
        to_guest(src, guest_path)


def install(src, target):
    on_host = src.startswith('host:')
    path = src[5:] if on_host else src
    if on_host:
        is_dir = host_run(f'test -d {shlex.quote(path)} && echo d || echo f').strip() == 'd'
    else:
        is_dir = os.path.isdir(path)
    name = os.path.basename(path.rstrip('/\\'))
    dest = resolve_target(name, target, is_dir)
    q = shlex.quote(dest)
    # the original once (a second install keeps the first original), and the list of what was replaced on the
    # root filesystem, which `restore` reads
    keep = (f'if [ -e {q} ] && [ ! -e {q}.abvm-orig ]; then cp -a {q} {q}.abvm-orig; '
            f'mkdir -p /var/lib/abvm; echo {q} >> /var/lib/abvm/installed; fi')
    stage = f'/tmp/abvm-{uuid.uuid4().hex[:8]}'
    if not is_dir:
        source_to_guest(src, stage)
        # moved into place, so a running program never sees half a file
        guest_run(f'sudo sh -c "set -e; {keep}; cp {stage} {q}.abvm-new; '
                  f'chmod --reference={q}.abvm-orig {q}.abvm-new 2>/dev/null || chmod 755 {q}.abvm-new; '
                  f'mv -f {q}.abvm-new {q}; rm -f {stage}; sync"')
        print(f'installed {dest}')
        return
    # a directory: packed where it is, unpacked in the guest and copied over the target's contents, with the
    # launcher stopped meanwhile (a launcher tree is the usual one; the data partition is exFAT, which has no
    # owners - hence --no-same-owner and a plain cp)
    archive = stage + '.tgz'
    if on_host:
        host_run(f'tar czf {archive} -C {shlex.quote(os.path.dirname(path.rstrip("/")))} {shlex.quote(name)}')
        source_to_guest('host:' + archive, archive)
        host_run(f'rm -f {archive}', check=False)
    else:
        import tarfile
        local = os.path.join(tempfile.mkdtemp(prefix='abvm-'), name + '.tgz')
        with tarfile.open(local, 'w:gz') as tar:
            tar.add(path, arcname=name)
        to_guest(local, archive)
        shutil.rmtree(os.path.dirname(local), ignore_errors=True)
    guest_run(f'sudo sh -c "set -e; systemctl stop autobleem.service; {keep}; mkdir -p {stage} {q}; '
              f'tar xzf {archive} --no-same-owner -C {stage}; cp -r {stage}/{shlex.quote(name)}/. {q}/; '
              f'rm -rf {stage} {archive}; sync; systemctl start autobleem.service"')
    print(f'installed {dest}/ (the launcher restarted)')


def restore():
    out = guest_run('cat /var/lib/abvm/installed 2>/dev/null', check=False).split()
    if out:
        guest_run('sudo systemctl stop autobleem.service')
    for path in dict.fromkeys(out):
        q = shlex.quote(path)
        # a directory goes back whole (what the install added to it goes with the new copy)
        guest_run(f'sudo sh -c "if [ -d {q}.abvm-orig ]; then rm -rf {q}; mv {q}.abvm-orig {q}; '
                  f'elif [ -e {q}.abvm-orig ]; then mv -f {q}.abvm-orig {q}; fi"')
        print(f'restored {path}')
    guest_run('sudo rm -f /var/lib/abvm/installed; sync')
    if out:
        guest_run('sudo systemctl start autobleem.service')
    else:
        print('nothing to restore')


def driver_answers():
    out = guest_run(f'ss -ltn 2>/dev/null | grep -c ":{DRIVER_PORT} " || true', check=False).strip()
    return out not in ('', '0')


STICK_MOUNT_UNIT = 'media-autobleem.mount'  # /media/autobleem, the stick's data partition


def stick_source(what):
    """the mount unit's What= as (kind, value): /dev/disk/by-uuid/X -> ('uuid', X), by-label -> ('label', ...),
    by-partuuid -> ('partuuid', ...), a plain /dev node -> ('dev', node)"""
    m = re.fullmatch(r'/dev/disk/by-(uuid|label|partuuid|partlabel)/(.+)', what.strip())
    if m:
        value = m.group(2)
        # the unit's own escaping of a label with a space
        return m.group(1), value.replace('\\x20', ' ')
    return ('dev', what.strip()) if what.strip().startswith('/dev/') else (None, what.strip())


def find_stick_partition(lsblk_out, kind, value):
    """the partition's /dev node from `lsblk -rno NAME,UUID,LABEL,PARTUUID,PARTLABEL` (raw: spaces are \\x20), or None"""
    column = {'uuid': 1, 'label': 2, 'partuuid': 3, 'partlabel': 4}.get(kind)
    for line in lsblk_out.splitlines():
        cols = [c.replace('\\x20', ' ') for c in line.split(' ')]
        cols += [''] * (5 - len(cols))
        if kind == 'dev':
            if '/dev/' + cols[0] == value:
                return value
        elif column and cols[column] and cols[column] == value:
            return '/dev/' + cols[0]
    return None


def stick_mount_state():
    """(unit state, the unit's What=) - read only"""
    out = guest_run(f'systemctl show -p ActiveState -p What {STICK_MOUNT_UNIT}; true', check=False)
    props = dict(line.split('=', 1) for line in out.splitlines() if '=' in line)
    return props.get('ActiveState', '?'), props.get('What', '')


def remount(apply=True):
    """a hard reset can leave the stick's exFAT unmounted: udev never made the /dev/disk/by-uuid link, so the mount
    unit failed "Dependency failed" and the launcher stays down. Read-only checks first; only when the unit has
    failed and the partition is there: udevadm trigger on that partition, start the mount, start the launcher.
    Never formats, never fsck-repairs. Returns what it found/did, one line per step (apply=False: report only)."""
    state, what = stick_mount_state()
    if state == 'active':
        return ['stick mount: active - nothing to do']
    if state != 'failed':
        return [f'stick mount: {state} (only a failed mount is recovered)']
    lines = [f'stick mount: failed ({STICK_MOUNT_UNIT} wants {what or "?"})']
    kind, value = stick_source(what)
    if not kind:
        return lines + ['cannot tell which partition the unit mounts - left alone']
    part = find_stick_partition(
        guest_run('lsblk -rno NAME,UUID,LABEL,PARTUUID,PARTLABEL; true', check=False), kind, value)
    if not part:
        return lines + [f'the partition ({kind} {value}) is not in the guest - nothing to trigger (is the stick attached?)']
    lines.append(f'partition present: {part}')
    if not apply:
        return lines + ['run `abvm.py remount` (or `restart`) to trigger udev, mount it and start the launcher']
    guest_run(f'sudo udevadm trigger --name-match={shlex.quote(part)} && sudo udevadm settle --timeout=10; true',
              check=False)
    lines.append(f'udevadm trigger --name-match={part}')
    guest_run(f'sudo systemctl reset-failed {STICK_MOUNT_UNIT}; sudo systemctl start {STICK_MOUNT_UNIT}; true', check=False)
    state = stick_mount_state()[0]
    lines.append(f'start {STICK_MOUNT_UNIT}: {state}')
    if state != 'active':
        raise Fail('\n'.join(lines + [f'the stick still does not mount - see `abvm.py guest "journalctl -u {STICK_MOUNT_UNIT} '
                                      f'-n 20 --no-pager; true"`; not formatting or repairing it']))
    guest_run('sudo systemctl start autobleem.service; true', check=False)
    lines.append('start autobleem.service')
    return lines


def restart(timeout=90):
    for line in remount():
        print(line)
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
        shutil.move(src, out)  # the temp dir may be on another drive than `out`
        return
    try:
        from PIL import Image
    except ImportError:
        os.replace(src, os.path.splitext(out)[0] + '.ppm')
        raise Fail('the screenshot is a PPM and Pillow is not installed: saved as .ppm')
    Image.open(src).save(out)
    os.remove(src)


# ------------------------------------------------------------------ clips (on the test machine)

class VncClip:
    """The VM's screen as an MP4: a minimal VNC client (RFB 3.8, no password - QEMU's display listens on the test
    machine's loopback only) keeps the frame current from the changed rectangles, and a second thread hands it to
    ffmpeg at a steady rate - frames repeated while nothing changes, so the clip plays in real time. A screenshot
    through virsh takes ~600 ms (QEMU encodes a PNG); this keeps up with the UI."""

    def __init__(self, out, fps=25):
        import threading
        self.out, self.fps = out, fps
        self.threading = threading
        if not shutil.which('ffmpeg'):
            raise Fail('ffmpeg is not installed on the test machine (sudo apt-get install -y ffmpeg)')
        port = re.search(r':(\d+)', host_run(f'{VIRSH} domdisplay {shlex.quote(DOMAIN)}'))
        self.sock = socket.create_connection(('127.0.0.1', 5900 + int(port.group(1)) if port else 5900), timeout=10)
        self.handshake()
        self.lock = threading.Lock()
        self.stop_flag = False
        self.error = None
        self.ffmpeg = subprocess.Popen(
            ['ffmpeg', '-y', '-loglevel', 'error', '-f', 'rawvideo', '-pix_fmt', 'bgr0', '-s', f'{self.w}x{self.h}',
             '-r', str(fps), '-i', '-', '-c:v', 'libx264', '-preset', 'veryfast', '-crf', '26', '-pix_fmt', 'yuv420p',
             '-movflags', '+faststart', out], stdin=subprocess.PIPE)
        self.reader = threading.Thread(target=self.read_loop, daemon=True)
        self.writer = threading.Thread(target=self.write_loop, daemon=True)
        self.reader.start()
        self.writer.start()

    def exact(self, n):
        data = bytearray()
        while len(data) < n:
            chunk = self.sock.recv(min(n - len(data), 1 << 20))
            if not chunk:
                raise Fail('the VNC server closed the connection')
            data += chunk
        return bytes(data)

    def handshake(self):
        import struct
        self.struct = struct
        self.exact(12)
        self.sock.sendall(b'RFB 003.008\n')
        types = self.exact(self.exact(1)[0])
        if 1 not in types:
            raise Fail(f'the VNC server wants a password (security types {list(types)})')
        self.sock.sendall(b'\x01')
        if struct.unpack('>I', self.exact(4))[0] != 0:
            raise Fail('the VNC server refused the connection')
        self.sock.sendall(b'\x01')  # shared: virt-viewer on the panel stays connected
        self.w, self.h = struct.unpack('>HH', self.exact(4))
        self.exact(16)
        self.exact(struct.unpack('>I', self.exact(4))[0])
        # 32 bits, true colour, little endian B G R X - ffmpeg's bgr0
        self.sock.sendall(struct.pack('>BxxxBBBBHHHBBBxxx', 0, 32, 24, 0, 1, 255, 255, 255, 16, 8, 0))
        self.sock.sendall(struct.pack('>BxHi', 2, 1, 0))  # raw only
        self.fb = bytearray(self.w * self.h * 4)

    def request(self, incremental):
        self.sock.sendall(self.struct.pack('>BBHHHH', 3, incremental, 0, 0, self.w, self.h))

    def read_loop(self):
        st = self.struct
        try:
            self.sock.settimeout(None)
            self.request(0)
            last_full = time.time()
            while not self.stop_flag:
                kind = self.exact(1)[0]
                if kind == 0:
                    rects = st.unpack('>xH', self.exact(3))[0]
                    for _ in range(rects):
                        x, y, w, h, enc = st.unpack('>HHHHi', self.exact(12))
                        if enc != 0:
                            raise Fail(f'unexpected VNC encoding {enc}')
                        data = self.exact(w * h * 4)
                        with self.lock:
                            for row in range(h):
                                if y + row >= self.h:
                                    break
                                cols = max(0, min(w, self.w - x))
                                start = ((y + row) * self.w + x) * 4
                                self.fb[start:start + cols * 4] = data[row * w * 4:row * w * 4 + cols * 4]
                    # QEMU's incremental updates can miss a change (a closed overlay stayed on screen in the
                    # first clip), so ask for the whole screen again a few times a second
                    now = time.time()
                    full = now - last_full >= 0.25
                    if full:
                        last_full = now
                    self.request(0 if full else 1)
                elif kind == 2:
                    pass  # bell
                elif kind == 3:
                    self.exact(st.unpack('>xxxI', self.exact(7))[0])
                else:
                    raise Fail(f'unexpected VNC message {kind}')
        except Exception as e:  # noqa: BLE001 - reported by stop()
            if not self.stop_flag:
                self.error = e

    def write_loop(self):
        start = time.time()
        written = 0
        while not self.stop_flag:
            due = int((time.time() - start) * self.fps) + 1
            while written < due:
                with self.lock:
                    frame = bytes(self.fb)
                self.ffmpeg.stdin.write(frame)
                written += 1
            time.sleep(max(0.0, start + written / self.fps - time.time()))
        self.frames = written

    def stop(self):
        self.stop_flag = True
        self.writer.join()
        try:
            self.sock.close()
        except OSError:
            pass
        self.ffmpeg.stdin.close()
        self.ffmpeg.wait()
        if self.error:
            raise Fail(f'clip: {self.error}')
        return self.frames


def clip(seconds, out):
    if not LOCAL:
        stage = f'/tmp/abvm-clip-{uuid.uuid4().hex[:8]}.mp4'
        try:
            print(remote_tool(['clip', str(seconds), stage]), end='')
            from_host(stage, out)
        finally:
            host_run(f'rm -f {stage}', check=False)
        print(f'ok {os.path.abspath(out)}')
        return
    recorder = VncClip(out)
    time.sleep(float(seconds))
    print(f'clip {os.path.basename(out)}: {recorder.stop()} frames')


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
             'battery', 'cable', 'kbd'}
# the launcher's own DebugDriver words (tools/ab_drive.py): in `run` they go to the stick's launcher through the
# test machine's forward, so one script works here and in a sandbox (`sandbox drive`)
DRIVER_WORDS = {'screen', 'wait_screen', 'wait_idle', 'key', 'text', 'grab', 'menu', 'quick', 'items', 'frames',
                'window', 'down', 'up', 'home'}


def steps(script):
    return [s.strip() for s in script.split(';') if s.strip()]


def run_local(script, out_dir):
    """on the test machine: every step at its own time; shots through virsh into out_dir"""
    pad = None
    recorder = None
    driver = None
    try:
        for step in steps(script):
            words = step.split()
            if words[0] in DRIVER_WORDS:
                if driver is None:
                    driver = import_ab_drive().Driver(DRIVER_PORT)
                if words[0] == 'grab':
                    os.makedirs(out_dir, exist_ok=True)
                    step = 'grab ' + os.path.join(out_dir, os.path.basename(words[1]))
                for reply in driver.run(step):
                    print(reply)
            elif words[0] == 'wait':
                time.sleep(int(words[1]) / 1000)
            elif words[0] == 'clip':
                # clip start <name.mp4> ... clip stop: the screen recorded while the steps between run
                if words[1] == 'start':
                    os.makedirs(out_dir, exist_ok=True)
                    recorder = VncClip(os.path.join(out_dir, os.path.basename(words[2])))
                elif recorder:
                    print(f'clip {os.path.basename(recorder.out)}: {recorder.stop()} frames')
                    recorder = None
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
    except RuntimeError as e:  # a driver step that failed: what ran before it stays printed / on disk
        raise Fail(str(e)) from None
    finally:
        if recorder:
            print(f'clip {os.path.basename(recorder.out)}: {recorder.stop()} frames')
        if pad:
            pad.close()
        if driver:
            driver.close()


def remote_error(msg):
    # the test machine's own message, without the ssh command around it; a busy one stays exit 3 here too
    msg = msg.rsplit('abvm: ', 1)[-1]
    return (Busy if msg.startswith('busy:') else Fail)(msg)


def remote_tool(args, partial=False):
    """the tool's output from the test machine. partial: (stdout, the error or None) instead of raising - for a run
    that fails after it made shots, which still have to be fetched"""
    who = ['--who', WHO] if WHO else []
    key = ['--lease-key', LEASE_KEY] if LEASE_KEY != 'vm' else []
    # the sandbox settings given here count there too
    env = [f'{k}={os.environ[k]}' for k in ('ABVM_SANDBOX_SLOTS', 'ABVM_SANDBOX_SIZE', 'ABVM_SANDBOX_ENV')
           if k in os.environ]
    prefix = ('env ' + ' '.join(shlex.quote(e) for e in env) + ' ') if env else ''
    command = prefix + f'python3 {REMOTE_TOOL} --local ' + ' '.join(shlex.quote(a) for a in who + key + args)
    if partial:
        r = host_run(command, full=True)
        return r.stdout, (remote_error(r.stderr.strip() or f'exit {r.returncode}') if r.returncode else None)
    try:
        return host_run(command)
    except Fail as e:
        raise remote_error(str(e)) from None


# ------------------------------------------------------------------ the lease: one tester on the VM at a time

LOCK_FILE = '~/.local/state/abvm/lock.json'
LEASE_KEY = 'vm'       # 'vm' - the VM itself; 'sb-<name>' - one sandbox (see "sandboxes" below)


def lock_path():
    if LEASE_KEY == 'vm':
        return os.path.expanduser(LOCK_FILE)
    return os.path.expanduser(f'~/.local/state/abvm/lock-{LEASE_KEY}.json')


def lease_subject():
    return 'the VM' if LEASE_KEY == 'vm' else f'sandbox {LEASE_KEY[3:]}'
LOCK_MINUTES = 30      # a lease's default length
LOCK_KEEPALIVE = 10    # every command of the holder keeps the lease at least this many minutes ahead
# what changes the VM or its screen; status, shot and lock itself never need the lease
NEEDS_LEASE = {'install', 'restore', 'restart', 'remount','clip', 'pad', 'run', 'drive', 'padsim-install', 'guest'}
WHO = os.environ.get('ABVM_WHO', '')


class Busy(Fail):
    pass


def lease_describe(lease, now):
    left = int((lease['until'] - now + 59) // 60)
    since = time.strftime('%H:%M', time.localtime(lease['since']))
    return f"{lease['who']} ({lease['task']}) since {since}, {left} min left"


def lease_op(op, who='', task='', minutes=LOCK_MINUTES):
    """on the test machine: take / check / release / show the lease, under flock so two callers never race"""
    import fcntl
    import json
    path = lock_path()
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path + '.flock', 'w') as guard:
        fcntl.flock(guard, fcntl.LOCK_EX)
        now = time.time()
        try:
            with open(path) as f:
                lease = json.load(f)
            if lease['until'] <= now:
                lease = None  # expired: a holder that died frees the VM by itself
        except (OSError, ValueError, KeyError):
            lease = None

        def save(value):
            with open(path + '.tmp', 'w') as f:
                json.dump(value, f)
            os.replace(path + '.tmp', path)

        if op == 'show':
            return f'held by {lease_describe(lease, now)}' if lease else 'free'
        if not who:
            raise Fail('who is testing? set ABVM_WHO=<name> (or --who <name>) - the VM is shared')
        if lease and lease['who'] != who:
            raise Busy(f'busy: {lease_subject()} - {lease_describe(lease, now)} - do something else and try again '
                       'later')
        if op == 'take':
            if not task:
                raise Fail(f'take <task> [minutes]: say what {lease_subject()} is for')
            since = lease['since'] if lease else now
            save({'who': who, 'task': task, 'since': since, 'until': now + minutes * 60})
            return f"taken by {who} ({task}) for {minutes} min"
        if op == 'check':
            if not lease:
                how = 'lock take' if LEASE_KEY == 'vm' else f'sandbox take {LEASE_KEY[3:]}'
                raise Fail(f'take {lease_subject()} first: abvm.py {how} <task> [minutes] (ABVM_WHO={who})')
            lease['until'] = max(lease['until'], now + LOCK_KEEPALIVE * 60)
            save(lease)
            return 'ok'
        if op == 'release':
            if lease:
                os.remove(path)
                return f'released by {who}'
            return 'free'
        raise Fail(f'lock: unknown {op!r}')


def lease(op, *args):
    if LOCAL:
        return lease_op(op, WHO, *args)
    return remote_tool(['lock', op] + [str(a) for a in args]).strip()


def lock_command(args):
    op = args[0] if args else 'status'
    if op in ('status', 'show'):
        print(lease('show'))
    elif op == 'take':
        if len(args) < 2:
            raise Fail('lock take <task> [minutes]')
        minutes = int(args[2]) if len(args) > 2 else LOCK_MINUTES
        print(lease_op('take', WHO, args[1], minutes) if LOCAL else lease('take', args[1], minutes))
    elif op in ('release', 'check'):
        print(lease(op))
    else:
        raise Fail('lock status | take <task> [minutes] | release')


def run(script, out_dir):
    if LOCAL:
        run_local(script, out_dir)
        return
    stage = f'/tmp/abvm-run-{uuid.uuid4().hex[:8]}'
    try:
        out, failure = remote_tool(['run', script, '--out', stage], partial=True)
        print(out, end='')
        # a failed run keeps the shots it made before the failing step
        if re.search(r'\b(shot|grab)\s|clip start', script) and host_run(f'test -d {stage} && echo yes',
                                                                          check=False).strip():
            os.makedirs(out_dir, exist_ok=True)
            tmp = tempfile.mkdtemp(prefix='abvm-')
            from_host(stage + '/.', tmp, recursive=True)
            for name in os.listdir(tmp):
                if name.endswith('.mp4'):
                    shutil.move(os.path.join(tmp, name), os.path.join(out_dir, name))
                else:
                    save_png(os.path.join(tmp, name), os.path.join(out_dir, name))
            shutil.rmtree(tmp, ignore_errors=True)
        if failure:
            raise failure
    finally:
        host_run(f'rm -rf {stage}', check=False)


# ------------------------------------------------------------------ the DebugDriver through a tunnel

def _script_from_args(parts):
    """the literal script if given as words, or --file SCRIPT.txt's contents (one command per line,
    '#'-comments and blank lines dropped, joined with ';' the same as ab_drive.py's own --file)"""
    if '--file' in parts:
        i = parts.index('--file')
        path = parts[i + 1]
        with open(path, encoding='utf-8') as f:
            lines = [ln.strip() for ln in f]
        return ';'.join(ln for ln in lines if ln and not ln.startswith('#'))
    return ' '.join(parts)


def import_ab_drive():
    # the repository's tools/ab_drive.py, or the copy `setup` puts next to this tool on the test machine
    sys.path[:0] = [HERE, os.path.join(HERE, '..')]
    import ab_drive
    return ab_drive


def drive(script):
    ab_drive = import_ab_drive()
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
            try:
                replies = d.run(script)
            except ab_drive.RunFailed as e:
                for reply in e.replies:
                    print(reply)
                raise Fail(str(e)) from None
            for reply in replies:
                print(reply)
        finally:
            d.close()
    finally:
        if tunnel:
            tunnel.terminate()


# ------------------------------------------------------------------ sandboxes: headless launchers, roots on the host
#
# A sandbox is a launcher started headless inside the VM, next to the stick's own, with its whole root in a folder on
# the test machine's disk (<SB_HOST>/<name>, the VM's 9p share mounted at <SB_GUEST>) - never the stick. Its own
# runtime dir, DebugDriver port and lease; the VM's lease is not needed. Everything below runs on the test machine.

SB_HOST = os.environ.get('ABVM_SANDBOXES', '~/abvm/sandboxes')
SB_GUEST = os.environ.get('ABVM_SANDBOX_MOUNT', '/mnt/abvm')
SB_SHARE = 'abvm-sandboxes'   # the <filesystem> target in the VM's domain XML
SB_PORTS = range(6910, 6920)
SB_MAX_SLOTS = 2  # headless launchers running at once, never more (an idle one takes a whole CPU; 5 vCPUs; the VM hung at 3)
# the environment can only lower it: a missing, garbled or too-high value is the ceiling
try:
    SB_SLOTS = max(1, min(SB_MAX_SLOTS, int(os.environ.get('ABVM_SANDBOX_SLOTS', SB_MAX_SLOTS))))
except ValueError:
    SB_SLOTS = SB_MAX_SLOTS
STICK = '/media/autobleem'
MOUNT_UNIT = 'mnt-abvm.mount'


def sb_host(name=''):
    return os.path.join(os.path.expanduser(SB_HOST), name) if name else os.path.expanduser(SB_HOST)


def sb_name(name):
    if not re.fullmatch(r'[a-z0-9][a-z0-9-]{0,30}', name or ''):
        raise Fail(f'a sandbox name is lowercase letters, digits and dashes: {name!r}')
    return name


def sb_state(name):
    import json
    try:
        with open(os.path.join(sb_host(name), '.abvm', 'state.json')) as f:
            return json.load(f)
    except (OSError, ValueError):
        return {}


def sb_save_state(name, state):
    import json
    d = os.path.join(sb_host(name), '.abvm')
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, 'state.json'), 'w') as f:
        json.dump(state, f)


def sb_names():
    root = sb_host()
    if not os.path.isdir(root):
        return []
    return sorted(n for n in os.listdir(root) if not n.startswith(('_', '.')) and os.path.isdir(os.path.join(root, n)))


def sb_guest_pid_alive(name, pid):
    """the pid is still the launcher of this sandbox (never trust a bare pid - it may be reused)"""
    if not pid:
        return False
    out = guest_run(f'cat /proc/{int(pid)}/cmdline 2>/dev/null | tr "\\0" " "', check=False)
    return f'{SB_GUEST}/{name}' in out and 'autobleem-gui' in out


def sb_running(name):
    st = sb_state(name)
    return st if st.get('pid') and sb_guest_pid_alive(name, st['pid']) else None


def sb_names_in_cmdlines(text):
    """the sandbox names whose launcher the guest's process list shows: one cmdline per line, the launcher's last
    argument is its root <SB_GUEST>/<name>"""
    found = set()
    for line in text.splitlines():
        if 'autobleem-gui' not in line:
            continue
        m = re.search(re.escape(SB_GUEST) + r'/([a-z0-9][a-z0-9-]*)(?:/|\s|$)', line)
        if m:
            found.add(m.group(1))
    return found


def sb_running_names(exclude=''):
    """every sandbox that really runs, from the VM's own state: the launchers in the guest's process list (a launcher
    whose state file was lost or reset still takes its CPU) and the state files whose pid is still that launcher"""
    out = guest_run('for p in $(pidof autobleem-gui); do tr "\\0" " " < /proc/$p/cmdline 2>/dev/null; echo; done; true',
                    check=False)
    names = sb_names_in_cmdlines(out) | {n for n in sb_names() if sb_running(n)}
    names.discard(exclude)
    return sorted(names)


def sb_open_modes(path):
    """the guest checks permissions itself against the host's owner, so a sandbox tree is open to everyone - it is a
    disposable test tree on a test machine; the ACLs give this user and QEMU the rest"""
    subprocess.run(['chmod', '-R', 'a+rwX', path], check=False)


def sb_setup():
    """once per VM (the VM's lease): the guest mounts the share at boot; the covers shared read-only"""
    unit = (f'[Unit]\\nDescription=abvm sandboxes (a 9p share from the test machine)\\n'
            f'[Mount]\\nWhat={SB_SHARE}\\nWhere={SB_GUEST}\\nType=9p\\n'
            f'Options=trans=virtio,version=9p2000.L,msize=262144,access=any,cache=none\\n'
            f'[Install]\\nWantedBy=multi-user.target\\n')
    os.makedirs(sb_host('_shared'), exist_ok=True)
    guest_run(f'set -e; printf "{unit}" > /tmp/{MOUNT_UNIT}; sudo install -m 644 /tmp/{MOUNT_UNIT} '
              f'/etc/systemd/system/{MOUNT_UNIT}; rm /tmp/{MOUNT_UNIT}; sudo systemctl daemon-reload; '
              f'sudo systemctl enable {MOUNT_UNIT}; mountpoint -q {SB_GUEST} || sudo systemctl start {MOUNT_UNIT}; '
              f'systemctl is-enabled {MOUNT_UNIT}; mountpoint {SB_GUEST}')
    # the cover DBs: 280 MB, the same for every sandbox - one copy, linked from each
    guest_run(f'set -e; [ -d {SB_GUEST}/_shared/db ] || sudo cp -r {STICK}/Autobleem/bin/db {SB_GUEST}/_shared/db')
    print(f'sandboxes: {SB_GUEST} in the guest = {sb_host()} here; covers in _shared/db')


def sb_template():
    """_template: what every new sandbox starts from - the stick's launcher, themes, extensions and Apps, no games,
    empty databases; the covers linked from _shared"""
    t = f'{SB_GUEST}/_template'
    guest_run(f'set -e; mountpoint -q {SB_GUEST} || {{ echo "{SB_GUEST} is not mounted: abvm.py sandbox setup" >&2; '
              f'exit 1; }}; sudo rm -rf {t}.new; sudo mkdir -p {t}.new/Autobleem/bin {t}.new/System/Databases '
              f'{t}.new/System/Logs {t}.new/Games; '
              f'sudo cp -r {STICK}/Autobleem/bin/autobleem {STICK}/Autobleem/bin/abpad {t}.new/Autobleem/bin/; '
              f'sudo cp -r {STICK}/Autobleem/rc {t}.new/Autobleem/; '
              f'sudo cp -r {STICK}/Themes {STICK}/Extensions {STICK}/Apps {t}.new/; sudo cp {STICK}/VERSION {t}.new/; '
              f'sudo ln -s ../../../_shared/db {t}.new/Autobleem/bin/db; sudo rm -rf {t}; sudo mv {t}.new {t}')
    sb_open_modes(sb_host('_template'))
    print(f'template made from the stick ({STICK})')


def sb_copy_build(name, build):
    """a build's dist/<target> (its Autobleem/ tree) laid over the sandbox - the build under test, used where it lies"""
    src = os.path.expanduser(build[5:] if build.startswith('host:') else build)
    if not os.path.isdir(os.path.join(src, 'Autobleem')):
        raise Fail(f'{src} has no Autobleem/ - give a build\'s dist/<target> directory (on the test machine)')
    subprocess.run(['cp', '-r', os.path.join(src, 'Autobleem'), sb_host(name)], check=True)
    sb_open_modes(os.path.join(sb_host(name), 'Autobleem'))


def sb_copy_ext(name, ext):
    """an extension laid into the sandbox's Extensions/: a zip (the extension zips hold Extensions/<name>/...) is
    unzipped at the sandbox root, a directory (.../extensions/<name>/) is copied to Extensions/<name>/. What it
    replaces is kept once in <sandbox>/.abvm/ext-backup/<name> (never inside Extensions/, which the launcher scans)"""
    src = os.path.expanduser(ext[5:] if ext.startswith('host:') else ext).rstrip('/')
    root = sb_host(name)
    if os.path.isdir(src):
        names = [os.path.basename(src)]
    elif zipfile.is_zipfile(src):
        with zipfile.ZipFile(src) as z:
            names = sorted({p.split('/')[1] for p in z.namelist() if p.startswith('Extensions/') and p.count('/') > 1})
        if not names:
            raise Fail(f'{src} holds no Extensions/<name>/ - an extension zip is expected')
    else:
        raise Fail(f'--ext {ext}: give an extension zip or a directory .../extensions/<name>/ (on the test machine)')
    for n in names:
        target = os.path.join(root, 'Extensions', n)
        backup = os.path.join(root, '.abvm', 'ext-backup', n)
        if os.path.isdir(target):
            if not os.path.exists(backup):
                os.makedirs(os.path.dirname(backup), exist_ok=True)
                shutil.copytree(target, backup)
            shutil.rmtree(target)  # replaced whole, so nothing of the old version stays behind
    if os.path.isdir(src):
        shutil.copytree(src, os.path.join(root, 'Extensions', names[0]))
    else:
        with zipfile.ZipFile(src) as z:
            z.extractall(root)
    sb_open_modes(os.path.join(root, '.abvm'))
    for n in names:
        sb_open_modes(os.path.join(root, 'Extensions', n))
    print(f'sandbox {name}: extension {", ".join(names)} laid from {os.path.basename(src)}')


STORE_STATE_DIRS = ('cache', 'downloads', 'staging', 'sources')


def sb_make_store_dirs(name):
    """the Store's state directories (System/Extensions/store/...): the Store makes none of them itself, so without
    them it cannot cache its catalog and its screens come out empty. Opened like the rest of the tree (the guest
    checks permissions against the host's owner, see sb_open_modes), so the launcher's user can write them"""
    base = os.path.join(sb_host(name), 'System', 'Extensions', 'store')
    for d in STORE_STATE_DIRS:
        os.makedirs(os.path.join(base, d), exist_ok=True)
    sb_open_modes(os.path.join(sb_host(name), 'System', 'Extensions'))


def sb_new(name, build=None, exts=()):
    if os.path.exists(sb_host(name)):
        raise Fail(f'sandbox {name} exists - `sandbox reset {name}` starts it afresh')
    if not os.path.isdir(sb_host('_template')):
        raise Fail('no template yet: abvm.py sandbox template')
    subprocess.run(['cp', '-r', sb_host('_template'), sb_host(name)], check=True)
    sb_open_modes(sb_host(name))
    sb_make_store_dirs(name)
    if build:
        sb_copy_build(name, build)
    for ext in exts:
        sb_copy_ext(name, ext)
    print(f'sandbox {name} made' + (f' with {build}' if build else ''))


def sb_forward(port):
    """the test machine's 127.0.0.1:<port> -> the guest's own loopback (the driver never listens on a network)"""
    r = subprocess.run(['ssh', '-f', '-N', '-o', 'ExitOnForwardFailure=yes', '-o', 'BatchMode=yes', '-i',
                        os.path.expanduser(KEY), '-L', f'{port}:127.0.0.1:{port}', guest_address()],
                       stdin=subprocess.DEVNULL, capture_output=True, text=True)
    if r.returncode != 0:
        raise Fail(f'the forward to port {port}: {r.stderr.strip()}')
    # no leading '-' in the pattern: pgrep would take it for an option
    out = subprocess.run(['pgrep', '-n', '-f', f'[-]L {port}:127.0.0.1:{port} '], capture_output=True, text=True)
    return int(out.stdout.split()[0]) if out.stdout.strip() else 0


def sb_stop_forward(st):
    pid = st.get('forward')
    if not pid:
        return
    try:
        with open(f'/proc/{int(pid)}/cmdline') as f:
            args = f.read()
    except OSError:
        return
    if f'{st.get("port")}:127.0.0.1:{st.get("port")}' in args:  # still our ssh, not a reused pid
        os.kill(int(pid), 15)


def sb_full_message(running):
    return (f'busy: {len(running)} sandboxes already run ({", ".join(running)}); at most {SB_SLOTS} may - an idle '
            f'launcher takes a whole CPU and a third one hangs the VM. Stop one you own '
            f'(abvm.py sandbox stop <name>, with its lease) or try again later')


def sb_start(name, build=None, size=None, exts=()):
    size = size or os.environ.get('ABVM_SANDBOX_SIZE', '1280x720')
    if not sb_running(name):  # refuse before the sandbox is made or a build is laid over it
        others = sb_running_names(exclude=name)
        if len(others) >= SB_SLOTS:
            raise Busy(sb_full_message(others))
    if not os.path.isdir(sb_host(name)):
        sb_new(name, build, exts)
    else:
        if build:
            sb_copy_build(name, build)
        for ext in exts:
            sb_copy_ext(name, ext)
    if sb_running(name):
        print(f'sandbox {name} already runs on port {sb_state(name)["port"]}')
        return
    sb_stop_forward(sb_state(name))  # a launcher that ended without `sandbox stop` (a crash, a SIGTERM) left it
    running = sb_running_names(exclude=name)
    if len(running) >= SB_SLOTS:
        raise Busy(sb_full_message(running))
    used = {sb_state(n).get('port') for n in running}

    def free_here(p):
        s = socket.socket()
        try:
            s.bind(('127.0.0.1', p))
            return True
        except OSError:
            return False
        finally:
            s.close()

    port = next((p for p in SB_PORTS if p not in used and free_here(p)), None)
    if port is None:
        raise Busy(f'busy: no free sandbox port in {SB_PORTS.start}-{SB_PORTS.stop - 1} on this machine')
    g = f'{SB_GUEST}/{name}'
    rt = f'/tmp/abvm-sb/{name}'  # the guest's own, no sudo needed
    if not re.fullmatch(r'\d{2,5}x\d{2,5}', size or ''):
        raise Fail(f'--size {size!r}: <width>x<height>, e.g. 1280x720')
    # shots and clips land in the sandbox's own .abvm/out (the test machine's folder), its pads' batteries in
    # .abvm/power_supply; the window is `size` (the offscreen driver's own is 1024x768)
    env = (f'AB_ROOT={g} AB_RUNTIME_DIR={rt} AB_LOG_DIR={g}/System/Logs AB_DEBUG_PORT={port} AB_NO_SPLASH=1 '
           f'AB_HEADLESS=1 AB_INPUT_ISOLATED=1 SDL_VIDEODRIVER=offscreen SDL_AUDIODRIVER=dummy '
           f'AB_WINDOW_SIZE={size} AB_DEBUG_OUT={g}/.abvm/out AB_PAD_BATTERY_DIR={g}/.abvm/power_supply AB_MAX_FPS=30')
    extra = os.environ.get('ABVM_SANDBOX_ENV', '')  # more for the launcher, e.g. AB_FRAME_STATS=1
    if extra:
        env += ' ' + ' '.join(shlex.quote(w) for w in extra.split())
    t0 = time.time()
    # made here, open to the guest (a folder the guest makes through the share is QEMU's, and closed to this user)
    for sub in ('out', 'power_supply'):
        os.makedirs(os.path.join(sb_host(name), '.abvm', sub), exist_ok=True)
    sb_open_modes(os.path.join(sb_host(name), '.abvm'))
    out = guest_run(f'set -e; mountpoint -q {SB_GUEST}; mkdir -p {rt}; '
                    f'cd {g}/Autobleem/bin/autobleem; umask 000; '
                    f'{env} setsid nohup ./autobleem-gui {g} > {g}/System/Logs/abvm-out.txt 2>&1 < /dev/null & '
                    f'echo pid $!; for i in $(seq 100); do ss -ltn | grep -q "127.0.0.1:{port} " && '
                    f'{{ echo ready; break; }}; sleep 0.1; done')
    pid = int(re.search(r'pid (\d+)', out).group(1))
    if 'ready' not in out:
        raise Fail(f'sandbox {name} did not open its driver port in 10 s - see {sb_host(name)}/System/Logs/'
                   'abvm-out.txt')
    st = {'pid': pid, 'port': port, 'since': time.time()}
    sb_save_state(name, st)  # saved before the forward, so a failure below never leaves an orphan behind
    try:
        st['forward'] = sb_forward(port)
    except Fail:
        sb_stop(name, use_driver=False)  # the port on this side is someone else's - never talk to it
        raise
    sb_save_state(name, st)
    # ready = the launcher's screen shows, so the first script's first shot is of it
    try:
        d = import_ab_drive().Driver(port)
        try:
            d.wait_screen('GuiLauncher', 60)
        finally:
            d.close()
    except (OSError, RuntimeError) as e:
        raise Fail(f'sandbox {name} runs, but its launcher screen did not show: {e} - see '
                   f'{sb_host(name)}/System/Logs/abvm-out.txt')
    print(f'sandbox {name} runs on port {port}, {size} (ready in {time.time() - t0:.1f} s)')


def sb_driver(name, **kwargs):
    st = sb_running(name)
    if not st:
        raise Fail(f'sandbox {name} is not running: abvm.py sandbox start {name}')
    ab_drive = import_ab_drive()
    return ab_drive.Driver(st['port'], **kwargs)


def sb_drive(name, script, out_dir):
    """on the test machine: the script through the sandbox's driver. Its shots, clips and grabs land in the
    sandbox's own <sandbox>/.abvm/out/<run>/ - the launcher writes shots and clip frames there itself (its
    AB_DEBUG_OUT, this machine's folder through the share), grabs come over the socket; a clip becomes an MP4"""
    run = uuid.uuid4().hex[:8]
    run_dir = os.path.join(sb_host(name), '.abvm', 'out', run)
    os.makedirs(run_dir, exist_ok=True)
    guest_root = f'{SB_GUEST}/{name}/'

    def to_host(path):  # the guest's name for a sandbox file -> this machine's
        return os.path.join(sb_host(name), path[len(guest_root):]) if path.startswith(guest_root) else None

    d = sb_driver(name, out_prefix=run + '/', path_map=to_host)
    cwd = os.getcwd()
    os.chdir(run_dir)
    failure = None
    try:
        try:
            replies = d.run(script)
        except import_ab_drive().RunFailed as e:
            replies, failure = e.replies, e  # a failed run keeps what it made: the replies and the run's folder
        for reply in replies:
            # the guest's paths as this machine names them
            print(re.sub(re.escape(guest_root) + r'\S*', lambda m: to_host(m.group(0)) or m.group(0), reply))
    except RuntimeError as e:
        raise Fail(str(e))
    finally:
        os.chdir(cwd)
        d.close()
    sb_open_modes(run_dir)
    print(f'out {run_dir}')
    if failure:
        raise Fail(str(failure))


def sb_stop(name, use_driver=True):
    st = sb_state(name)
    if st.get('pid') and sb_guest_pid_alive(name, st['pid']):
        try:
            if use_driver and st.get('forward'):
                d = sb_driver(name)
                d.cmd('quit')  # the clean way: the screens unwind, the databases close
                d.close()
        except (OSError, Fail):
            pass
        def gone(seconds):
            for _ in range(int(seconds * 10)):
                if not sb_guest_pid_alive(name, st['pid']):
                    return True
                time.sleep(0.1)
            return False

        if not gone(5):
            # SIGTERM is a clean quit too (the screens unwind); a kill only when even that does not end it
            guest_run(f'kill -TERM {int(st["pid"])}', check=False)
            if not gone(5):
                guest_run(f'kill -KILL {int(st["pid"])}', check=False)
                print(f'sandbox {name}: did not quit in 10 s - killed')
    sb_stop_forward(st)
    if st:
        sb_save_state(name, {})
    print(f'sandbox {name} stopped')


def sb_remove(name):
    root = sb_host(name)
    r = subprocess.run(['rm', '-rf', root], capture_output=True, text=True)
    if os.path.exists(root):
        # a file the guest made without write access for us - the guest's root can remove it
        guest_run(f'sudo rm -rf {SB_GUEST}/{name}', check=False)
    if os.path.exists(root):
        raise Fail(f'cannot remove {root}: {r.stderr.strip()}')


def sb_list():
    names = sb_names()
    if not names:
        print('no sandboxes')
    global LEASE_KEY
    for n in names:
        st = sb_running(n)
        LEASE_KEY = f'sb-{n}'
        holder = lease_op('show')
        print(f'{n:16} {"running on " + str(st["port"]) if st else "stopped":18} lease: {holder}')
    LEASE_KEY = 'vm'


def sandbox_command(args, out_dir):
    global LEASE_KEY
    sub = args[0] if args else 'list'
    name = sb_name(args[1]) if len(args) > 1 and sub not in ('list', 'setup', 'template') else ''
    if not LOCAL:
        if sub == 'drive':
            sb_drive_remote(name, _script_from_args(args[2:]), out_dir)
        else:
            print(remote_tool(['sandbox'] + args), end='')
        return
    if sub == 'list':
        sb_list()
    elif sub == 'setup':
        lease_op('check', WHO)  # changes the VM's own root filesystem
        sb_setup()
    elif sub == 'template':
        LEASE_KEY = 'sb-_template'
        lease_op('take', WHO, 'the template', 10)
        try:
            sb_template()
        finally:
            lease_op('release', WHO)
    elif sub in ('take', 'release'):
        if not name:
            raise Fail(f'sandbox {sub} <name>' + (' <task> [minutes]' if sub == 'take' else ''))
        LEASE_KEY = f'sb-{name}'
        if sub == 'take':
            minutes = int(args[3]) if len(args) > 3 else LOCK_MINUTES
            print(lease_op('take', WHO, args[2] if len(args) > 2 else '', minutes))
        else:
            print(lease_op('release', WHO))
    elif sub == 'logs':
        path = os.path.join(sb_host(name), 'System', 'Logs', 'abvm-out.txt')
        n = int(args[2]) if len(args) > 2 else 40
        with open(path, errors='replace') as f:
            print(''.join(f.readlines()[-n:]), end='')
    elif sub in ('new', 'start', 'drive', 'stop', 'reset', 'rm'):
        if not name:
            raise Fail(f'sandbox {sub} <name>')
        LEASE_KEY = f'sb-{name}'
        lease_op('check', WHO)
        build = None
        if '--build' in args:
            build = args[args.index('--build') + 1]
        size = args[args.index('--size') + 1] if '--size' in args else None
        exts = [args[i + 1] for i, a in enumerate(args) if a == '--ext' and i + 1 < len(args)]
        if sub == 'new':
            sb_new(name, build, exts)
        elif sub == 'start':
            sb_start(name, build, size, exts)
        elif sub == 'drive':
            sb_drive(name, _script_from_args(args[2:]), out_dir)
        elif sub == 'stop':
            sb_stop(name)
        elif sub == 'reset':
            sb_stop(name)
            sb_remove(name)
            sb_new(name, build, exts)
        elif sub == 'rm':
            sb_stop(name)
            sb_remove(name)
            lease_op('release', WHO)
            print(f'sandbox {name} removed')
    else:
        raise Fail('sandbox list | setup | template | take <name> <task> [min] | release <name> | '
                   'new|start <name> [--build <dist dir>] [--ext <zip|dir>]... | drive <name> "<script>" | logs <name> [n] | '
                   'stop|reset|rm <name>')


def sb_drive_remote(name, script, out_dir):
    """from a PC: the script runs on the test machine (its timing is not stretched by ssh), the grabs come back"""
    out, failure = remote_tool(['sandbox', 'drive', name, script], partial=True)
    print(out, end='')
    m = re.search(r'^out (\S+)$', out, re.M)
    if m and re.search(r'\b(grab|shot)\s|clip start', script):
        os.makedirs(out_dir, exist_ok=True)
        tmp = tempfile.mkdtemp(prefix='abvm-')
        from_host(m.group(1) + '/.', tmp, recursive=True)
        for f in os.listdir(tmp):
            shutil.move(os.path.join(tmp, f), os.path.join(out_dir, f))
        shutil.rmtree(tmp, ignore_errors=True)
        host_run(f'rm -rf {shlex.quote(m.group(1))}', check=False)
    if failure:
        raise failure


# ------------------------------------------------------------------ status, setup, padsim

def status():
    state = host_run(f'{VIRSH} domstate {shlex.quote(DOMAIN)}', check=False).strip() or 'unknown'
    print(f'vm        {DOMAIN}: {state}')
    print(f'lease     {lease("show")}')
    if state != 'running':
        return 1
    print(f'guest     {guest_address()}')
    units = guest_run('systemctl is-active autobleem.service padsim.service', check=False).split()
    print(f'launcher  {units[0] if units else "?"}' + (f' ({launcher_exe()})' if units[:1] == ['active'] else ''))
    stick = remount(apply=False)  # read only: status holds no lease
    print(f'stick     {stick[0]}')
    for line in stick[1:]:
        print(f'          {line}')
    print(f'padsim    {units[1] if len(units) > 1 else "?"}')
    print(f'driver    {"listening" if driver_answers() else "not listening"} on the guest\'s :{DRIVER_PORT}')
    tool = host_run(f'test -f {REMOTE_TOOL} && echo yes || echo no', check=False).strip() if not LOCAL else 'yes'
    print(f'abvm      {"on the test machine" if tool == "yes" else "not on the test machine - run `abvm.py setup`"}')
    return 0


def setup():
    host_run('mkdir -p ~/.local/share/abvm')
    to_host(os.path.abspath(__file__), REMOTE_TOOL)
    # the DebugDriver's client, for `sandbox drive` there
    to_host(os.path.join(HERE, '..', 'ab_drive.py'), os.path.dirname(REMOTE_TOOL) + '/ab_drive.py')
    print(f'copied to {HOST}:{REMOTE_TOOL} (and ab_drive.py)')


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
    global LOCAL, WHO, LEASE_KEY
    if '--local' in argv:
        LOCAL = True
        argv = [a for a in argv if a != '--local']
    if '--who' in argv:
        i = argv.index('--who')
        WHO = argv[i + 1]
        del argv[i:i + 2]
    if '--lease-key' in argv:
        i = argv.index('--lease-key')
        LEASE_KEY = argv[i + 1]
        del argv[i:i + 2]
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
        if cmd in NEEDS_LEASE:
            lease('check')
        if cmd == 'status':
            return status()
        elif cmd == 'lock':
            lock_command(args)
        elif cmd == 'sandbox':
            sandbox_command(args, out_dir)
        elif cmd == 'setup':
            setup()
        elif cmd == 'install':
            install(args[0], args[1] if len(args) > 1 else None)
        elif cmd == 'restore':
            restore()
        elif cmd == 'restart':
            restart()
        elif cmd == 'remount':
            print('\n'.join(remount()))
        elif cmd == 'shot':
            shot(args[0])
        elif cmd == 'clip':
            clip(args[0], args[1])
        elif cmd == 'pad':
            run(_script_from_args(args), out_dir)
        elif cmd == 'run':
            run(_script_from_args(args), out_dir)
        elif cmd == 'drive':
            drive(_script_from_args(args))
        elif cmd == 'padsim-install':
            padsim_install()
        elif cmd == 'guest':
            print(guest_run(' '.join(args)), end='')
        else:
            print(__doc__)
            return 2
    except Busy as e:
        print(f'abvm: {e}', file=sys.stderr)
        return 3
    except (Fail, subprocess.CalledProcessError) as e:
        print(f'abvm: {e}', file=sys.stderr)
        return 1
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
