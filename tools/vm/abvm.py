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
  python tools/vm/abvm.py update [--version V]           the stick to the newest pcusb nightly of the site (or V; a
                                                         given V is always installed): downloaded into the guest's
                                                         System/Updates/, sha256 checked, pending.json, then the
                                                         package update path (autobleem-update); prints VERSION
                                                         before and after, says "already on <v>" when it is
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
  python tools/vm/abvm.py sandbox template --package <P> the same from a pcusb release package instead of the stick (which
                                                         is older than the product): P = a .tar.gz or folder on the test
                                                         machine, a URL, or `nightly` (the site's newest pcusb nightly,
                                                         sha256 checked). _template/TEMPLATE_SOURCE records source,
                                                         version and date; `sandbox list` prints it
ONLY the pool sb2..sb4 exists (ABVM_SB_POOL); any other name is refused, except `rm` of an old folder:
  python tools/vm/abvm.py sandbox take-free <task> [min] [--who W]   the first free slot, laid fresh from the template;
                                                         last line `slot: <name>`; none free: exit 3 "busy: ..."
  python tools/vm/abvm.py sandbox rm-legacy [--dry-run]  remove the stopped, unleased folders outside the pool
  python tools/vm/abvm.py sandbox take <name> <task> [min] / release <name>        the slot's lease (take lays it fresh; release stops a launcher left running)
  python tools/vm/abvm.py sandbox new <name> [--build <dir>] [--ext <zip|dir>]...   made from the template (not started)
                                                         (with Mods/ and System/Extensions/store/{cache,cache/pictures,downloads,staging,sources})
  python tools/vm/abvm.py sandbox start <name> [--build <dir>] [--ext <zip|dir>]... [--size WxH]   made from the
                                                         template when new; --build lays a build's dist/<target> (on
                                                         the test machine, e.g. ~/src/autobleem/dist/pcusb) over it;
                                                         --ext (repeatable) lays an extension over it: a zip is
                                                         unzipped at the root (it holds Extensions/<name>/...), a
                                                         directory .../extensions/<name>/ is copied to
                                                         Extensions/<name>/; the replaced one is kept once in
                                                         <sandbox>/.abvm/ext-backup/<name>; started headless
                                                         in a WxH window (1280x720; ABVM_SANDBOX_SIZE), ready when
                                                         its launcher screen shows. Both also take:
                                                         --package <tar.gz|dir>: a release package (e.g.
                                                         autobleem-pcusb-i386-<ver>.tar.gz) laid over it - its
                                                         launcher, Themes/, extensions/ and VERSION, the look a
                                                         user gets (the template is only as new as the stick);
                                                         --games N: N fake PS1 games, 3 memory card sets, Apps
                                                         (make_usb.py's), the covers from the shared DBs;
                                                         --games test: the test machine's game set laid like a
                                                         stick (~/abvm/testgames: Games/, RetroArch/roms/;
                                                         ABVM_TESTGAMES), big files hard-linked;
                                                         --set Key=Value (repeatable): a line in the launcher's
                                                         config.ini, laid last (e.g. --set Language=Polski)
  python tools/vm/abvm.py sandbox drive <name> "<script>" [--out DIR]   an ab_drive.py script (@1 tap a; wait_idle
                                                         300; shot a.png; clip start b.mp4; ...; clip stop); shots,
                                                         grabs and clips (MP4, ffmpeg on the test machine) come back
                                                         to DIR - they are written into <sandbox>/.abvm/out/<run>/;
                                                         a failing step stops the run but the shots made before it
                                                         still come back, then one error line and exit code 1;
                                                         `probe <path>` (relative to the sandbox, no ..) is read-only
                                                         in the guest: id autobleem, owner/mode of the path and its
                                                         parent, ls -la - the output is among the results
                                                         --valgrind: the launcher under valgrind memcheck (slow; the
                                                         log is System/Logs/valgrind.txt). ABVM_SANDBOX_ENV (words
                                                         NAME=value: AB_* keys, MALLOC_CHECK_, MALLOC_PERTURB_,
                                                         MALLOC_ARENA_MAX, ASAN/UBSAN/LSAN/MSAN_OPTIONS, G_DEBUG,
                                                         G_SLICE, LD_DEBUG) goes to the launcher; ABVM_SANDBOX_VALGRIND=1
                                                         is --valgrind. Every run's end is recorded in <sandbox>/.abvm/
                                                         exit-status, a core kept in <sandbox>/.abvm/core/
  python tools/vm/abvm.py sandbox exit-status <name>     JSON {launcher_exit, launcher_signal, finished_at} of the last
                                                         run (null while it runs / never ran); exit 134 = signal 6
  python tools/vm/abvm.py sandbox crash-report <name>    for a death by signal: a gdb backtrace of the kept core with
                                                         the build's symbols, as text; says "no core" / "gdb is not
                                                         installed" when so
  python tools/vm/abvm.py sandbox signal <name> <SIG>    send ABRT, SEGV, TERM, KILL or INT to the running launcher
                                                         (anything else is refused); then exit-status / crash-report
  python tools/vm/abvm.py sandbox logs <name> [n]        the launcher's output
  python tools/vm/abvm.py sandbox stop|reset|rm <name>   quit it (the driver's `quit`, a kill after 5 s) / start it
                                                         afresh from the template / delete it
  python tools/vm/abvm.py sandbox list                   every sandbox, running or not, and its lease
At most 2 sandboxes run at once, a ceiling in the code (the VM has 5 vCPUs and an idle launcher uses a whole CPU - three
of them hung the VM twice, 2026-10-02). ABVM_SANDBOX_SLOTS may only lower it (1). `sandbox start` counts the launchers
that really run in the guest (state files and the guest's own process list) and refuses a third: exit 3, naming the
running ones - `sandbox stop <name>` (with its lease) frees a slot.

The VM sleeps when nobody uses it (it and the launchers in it kept ~2 CPUs busy for nothing):
  python tools/vm/abvm.py vm idle-check [--minutes N] [--dry-run]   (N default 20) SUSPENDS the VM (`virsh suspend`: the
                                                         vCPUs stop, memory stays, `virsh resume` is instant) when ALL
                                                         hold: no pool slot has a lease, the VM's own lease (the stick
                                                         launcher) is free, no launcher of a sandbox runs in the guest
                                                         (orphans count), and nothing used abvm for N minutes. One line:
                                                         `suspended: ...` | `would suspend: ...` (--dry-run) |
                                                         `not idle: <the first reason>` | `already suspended` |
                                                         `not running: <virsh state>`. Exit 0 whatever it decided
                                                         (a timer must not see a failure for "not idle"); 1 = it could
                                                         not tell (virsh/ssh error; the VM is then left alone); 2 = usage.
                                                         The first call with no last-use record only starts the clock.
  python tools/vm/abvm.py vm ensure-running              resumes a suspended VM ("resuming the VM...") and waits up to
                                                         60 s for the guest to answer ssh; one line: `running` |
                                                         `resumed (guest answered after N s)`. Exit 0; 1 = the VM is
                                                         not there (shut off) or the guest did not answer in 60 s.
                                                         Every command that needs the guest (status, install, restore,
                                                         restart, update, remount, clip, pad, run, drive,
                                                         padsim-install, guest, every sandbox one) calls it first.
  python tools/vm/abvm.py vm status                      the VM's state, when it was suspended, the last use
The last use (epoch seconds) is ~/.local/state/abvm/last-use on the test machine, written by every command that takes a
lease or drives (lock take, the commands needing the lease, every sandbox one but list/logs) and by a resume; the
suspend is recorded in ~/.local/state/abvm/vm-suspended.json. Run by a timer on the test machine every 5 minutes:
  ABVM_WHO=keeper python3 ~/.local/share/abvm/abvm.py --local vm idle-check

Scripts: steps separated by ';'. Pad steps: press/release <btn>, hold <btn> <ms>, tap <btn> (a 120 ms hold),
stick <left|right> <x> <y>, trigger <l2|r2> <0..255>, dpad <dir|center>, reset; profile
<x360|ds4|ds3|ps3pad|psc|generic> [usb|bt] (the pad replugged as that pad - ds4 over bt is a Bluetooth DualShock 4,
ds3 a wired DualShock 3 (d-pad as buttons), ps3pad the owner's third-party PS3-mode pad (0c12:0e16, hat), psc the
PlayStation Classic's pad (054c:0cda, 10 buttons, d-pad on axes), generic one SDL has no mapping for), unplug, plug,
battery <0..100> | battery off, cable in|out. Up to four pads: `@2 profile ds4 bt` sends to pad
2 (no @ = pad 1; pads 2-4 start unplugged). Buttons: a b x y l1 r1 l2 r2
select start guide l3 r3 - the Xbox names on every profile (a = Cross, b = Circle, x = Square, y = Triangle); cross
circle square triangle and up down left right (the d-pad's directions, on any profile) work too (not in the sandbox
driver's own pads, which have only x360, ds4 and generic).
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
  ABVM_REMOTE_TOOL  this tool's copy on the test machine (default: ~/.local/share/abvm/abvm.py) - another path
                 tries a branch's abvm there (`setup` copies to it) without touching the copy everyone uses.
                 The copy there (this tool, ab_drive.py, make_usb.py) is compared by content hash with the PC's files
                 before the first remote call of every run, and refreshed (atomically) when it differs.
docs/pc-test-machine.md (autobleem-main) describes the machine, the VM and padsim.

Never pipes into ssh (on Windows the EOF never arrives): files go by scp, commands as arguments.
"""
import hashlib
import json
import os
import posixpath  # the guest's paths, whatever this machine is
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
REMOTE_TOOL = os.environ.get('ABVM_REMOTE_TOOL', '~/.local/share/abvm/abvm.py')

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


SITE = os.environ.get('ABVM_SITE', 'https://autobleem.retromenele.pl')
STICK_ROOT = '/media/autobleem'


def update(version=None):
    """the stick to the newest pcusb nightly (or `version`) by the package update path - what the launcher's own
    Software Update does: the package into System/Updates/ with pending.json, then autobleem-update (install.sh
    --update: games, saves, settings, Home/ and System/Processors/ are kept). Never `install ... launcher`."""
    before = guest_run(f'cat {STICK_ROOT}/VERSION', check=False).strip()
    print(f'VERSION before: {before or "(none)"}')
    published = ''
    if not version:
        latest = json.loads(guest_run(f'curl -fsS -m 30 {SITE}/nightly/latest.json'))
        version = latest['version']
        published = (latest.get('files', {}).get('pcusb') or {}).get('sha256', '')
        if version == before:
            print(f'already on {version}')
            return
    name = f'autobleem-pcusb-i386-{version}.tar.gz'
    url = f'{SITE}/nightly/{version}/{name}'
    sums = guest_run(f'curl -fsS -m 30 {url}.sha256', check=False).split()
    if not sums:
        raise Fail(f'the site publishes no {name}.sha256 - is {version} a pcusb nightly?')
    sha = sums[0]
    if published and published != sha:
        raise Fail(f'the site disagrees with itself: latest.json says {published}, {name}.sha256 says {sha}')
    updates = f'{STICK_ROOT}/System/Updates'
    print(f'downloading {name}')
    got = guest_run(f'set -e; mkdir -p {updates}; curl -fsS -m 600 -o {updates}/{name} {url}; '
                    f'sha256sum {updates}/{name}', check=False).split()
    if not got or got[0] != sha:
        guest_run(f'rm -f {updates}/{name}', check=False)
        raise Fail(f'sha256 of the download ({got[0] if got else "nothing downloaded"}) differs from the published {sha}')
    pending = json.dumps({'autobleem_version': version, 'autobleem_file': name})
    guest_run(f'printf %s {shlex.quote(pending)} > {updates}/pending.json')
    print('sha256 ok, applying (autobleem-update)')
    out = guest_run('sudo systemctl stop autobleem.service; sudo autobleem-update >/tmp/abvm-update.log 2>&1; '
                    'echo rc=$?; tail -n 3 /tmp/abvm-update.log | cat -v; sudo rm -f /tmp/abvm-update.log', check=False)
    print(out.strip())
    ok = re.search(r'^rc=0$', out, re.M) is not None
    restart()
    after = guest_run(f'cat {STICK_ROOT}/VERSION', check=False).strip()
    print(f'VERSION after: {after}')
    if not ok or after != version:
        raise Fail(f'the update did not end on {version} (System/Logs/update.log on the stick; the download stays in '
                   f'System/Updates for another try)')


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


def local_sources():
    """(name on the test machine, this side's file) of everything the test machine's copy consists of; the tool last"""
    tools = os.path.join(HERE, '..')
    return [('ab_drive.py', os.path.join(tools, 'ab_drive.py')),
            ('make_usb.py', os.path.join(tools, 'make_usb.py')),
            (os.path.basename(REMOTE_TOOL), os.path.abspath(__file__))]


def _normalised(path):
    # a Windows checkout may be CRLF; the copy sent (and so its hash) is always LF
    with open(path, 'rb') as f:
        return f.read().replace(b'\r\n', b'\n')


def tool_version():
    """this side's version: name -> sha256 of each file the test machine's copy consists of"""
    return {name: hashlib.sha256(_normalised(path)).hexdigest() for name, path in local_sources()}


def remote_version():
    """the same on the test machine, from the files themselves (a file that is missing is simply not in the dict)"""
    names = ' '.join(shlex.quote(n) for n, _ in local_sources())
    out = host_run(f'cd {os.path.dirname(REMOTE_TOOL)} 2>/dev/null && sha256sum {names} 2>/dev/null', check=False)
    found = {}
    for line in out.splitlines():
        parts = line.split(None, 1)
        if len(parts) == 2:
            found[parts[1].strip().lstrip('*')] = parts[0]
    return found


def push_remote():
    """put this side's copy on the test machine. Every file goes under a temp name first and is renamed into place
    (the tool last), so a caller running right now sees the old file or the new one, never half of one - and a process
    already running keeps its old file (the rename leaves the old inode alone). Leases and sandboxes are not touched."""
    folder = os.path.dirname(REMOTE_TOOL)
    host_run(f'mkdir -p {folder}')
    tag = uuid.uuid4().hex[:8]
    stage = tempfile.mkdtemp(prefix='abvm-setup-')
    moves = []
    try:
        for name, path in local_sources():
            local = os.path.join(stage, name)
            with open(local, 'wb') as f:
                f.write(_normalised(path))
            temp = f'{folder}/.{name}.{tag}.new'
            to_host(local, temp)
            moves.append(f'mv -f {temp} {folder}/{name}')
    finally:
        shutil.rmtree(stage, ignore_errors=True)
    host_run(' && '.join(moves))


_remote_checked = False


def ensure_remote_current():
    """once per invocation, before the first call of the test machine's copy: if it differs from this side's files,
    refresh it (what `setup` does) and say so on stderr"""
    global _remote_checked
    if LOCAL or _remote_checked:
        return
    _remote_checked = True
    mine, theirs = tool_version(), remote_version()
    stale = sorted(n for n in mine if theirs.get(n) != mine[n])
    if stale:
        print(f'abvm: the copy on the test machine is stale ({", ".join(stale)}) - refreshing it', file=sys.stderr)
        push_remote()


def remote_tool(args, partial=False):
    """the tool's output from the test machine. partial: (stdout, the error or None) instead of raising - for a run
    that fails after it made shots, which still have to be fetched"""
    ensure_remote_current()
    who =['--who', WHO] if WHO else []
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
NEEDS_LEASE = {'install', 'restore', 'restart', 'update', 'remount','clip', 'pad', 'run', 'drive', 'padsim-install', 'guest'}
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
# the ONLY sandboxes there are: a fixed pool, laid fresh from the template on every take (ABVM_SB_POOL: comma list)
SB_POOL = tuple(n.strip() for n in os.environ.get('ABVM_SB_POOL', 'sb2,sb3,sb4').split(',')
                if re.fullmatch(r'[a-z0-9][a-z0-9-]{0,30}', n.strip())) or ('sb2', 'sb3', 'sb4')
SB_PROTECTED = ('_template', '_template.old', '_template.new', '_shared', 'TEMPLATE_SOURCE')
STICK = '/media/autobleem'
MOUNT_UNIT = 'mnt-abvm.mount'


def sb_host(name=''):
    return os.path.join(os.path.expanduser(SB_HOST), name) if name else os.path.expanduser(SB_HOST)


def sb_name(name):
    if not re.fullmatch(r'[a-z0-9][a-z0-9-]{0,30}', name or ''):
        raise Fail(f'a sandbox name is lowercase letters, digits and dashes: {name!r}')
    return name


def sb_pool_text():
    return f'{SB_POOL[0]}..{SB_POOL[-1]}' if len(SB_POOL) > 1 else SB_POOL[0]


def sb_pool_check(name):
    if name not in SB_POOL:
        raise Fail(f'sandbox {name}: not in the pool {sb_pool_text()} - use take-free')
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


def sb_guest_ports():
    """the TCP ports something in the guest listens on (a launcher without a state file is not in any state file)"""
    out = guest_run('ss -ltn', check=False) or ''
    return {int(p) for p in re.findall(r':(\d+)\s', out)}


def sb_guest_launchers():
    """{sandbox name: [launcher pids]} from the guest's process list"""
    out = guest_run('for p in $(pidof autobleem-gui); do echo "$p $(tr "\\0" " " < /proc/$p/cmdline 2>/dev/null)"; '
                    'done; true', check=False) or ''
    found = {}
    for line in out.splitlines():
        m = re.match(r'\s*(\d+) (.*)', line)
        if m:
            for n in sb_names_in_cmdlines(m.group(2)):
                found.setdefault(n, []).append(int(m.group(1)))
    return found


def sb_orphans():
    """{name: [pids]}: launchers in the guest whose sandbox has no state file naming a live pid"""
    return {n: pids for n, pids in sb_guest_launchers().items() if not sb_running(n)}


def sb_port_owner(name, pid, out):
    """the pid of THIS sandbox's launcher that owns the listener the start script saw (`listen <ss -ltnp line>`), else
    None: a listener of someone else's launcher (an orphan on the same port) is not this one's"""
    m = re.search(r'^listen (.*)$', out, re.M)
    if not m:
        return None
    for p in (int(x) for x in re.findall(r'pid=(\d+)', m.group(1))):
        if sb_guest_pid_alive(name, p):
            return p
    return None


def sb_kill_pids(pids):
    pids = sorted({int(p) for p in pids})
    if not pids:
        return
    ps = ' '.join(map(str, pids))
    guest_run(f'kill -TERM {ps} 2>/dev/null; for i in $(seq 30); do '
              f'n=0; for p in {ps}; do kill -0 $p 2>/dev/null && n=1; done; [ $n = 0 ] && break; sleep 0.1; done; '
              f'kill -KILL {ps} 2>/dev/null; true', check=False)


def sb_kill_launcher(name, pid=None):
    """kill the launcher(s) of `name`: the launched pid and whatever the process list shows for the sandbox's root"""
    pids = set(sb_guest_launchers().get(name, []))
    if pid and sb_guest_pid_alive(name, pid):
        pids.add(pid)
    sb_kill_pids(pids)
    return sorted(pids)


def sb_open_modes(path):
    """the guest checks permissions itself against the host's owner, so a sandbox tree is open to everyone - it is a
    disposable test tree on a test machine; the ACLs give this user and QEMU the rest"""
    subprocess.run(['chmod', '-R', 'a+rwX', path], check=False)


def sb_open_tree(name, acl=False):
    """every folder and file in sandbox `name` writable for the guest's user. A folder the guest makes through the
    share is the host's QEMU user's (libvirt-qemu, 775: QEMU's umask, not the launcher's) and the guest checks
    permissions against that owner, so the launcher's user `autobleem` could not write in it (seen: System/Updates,
    Games/!SaveStates). The host user cannot chmod what QEMU owns, so the pass runs in the guest, as root, on the
    share (the chmod goes through 9p to the owner): only what is not yet open is touched. The host's own part gets
    the plain chmod. acl=True (at start): also a default ACL on the host's folders, best effort, so folders made
    from then on are born 777 whatever QEMU's umask - the passes before each drive step close what that misses."""
    root = sb_host(name)
    sb_open_modes(root)
    if acl:
        try:
            subprocess.run(['setfacl', '-R', '-d', '-m', 'u::rwx,g::rwx,o::rwx', root], check=False,
                           stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except OSError:
            pass  # no setfacl here: the passes do it
    g = f'{SB_GUEST}/{name}'
    guest_run(f'sudo find {g} \\( -type d ! -perm -0777 -o -type f ! -perm -0666 \\) -exec chmod a+rwX {{}} +; true',
              check=False)


SB_SHARE_GROUP = 'abvmshare'  # the guest group made when no group of the share's gid is there yet
SB_QEMU_GROUP = 'libvirt-qemu'


def sb_share_gid():
    """the gid the share's folders carry on this machine - the group of QEMU's user, read now, never hard-coded
    (libvirt-qemu is 64055 on the test laptop). The group by name, else the group of a folder in the share that is
    not this user's (one QEMU made). None when neither is found."""
    try:
        import grp  # not on Windows, where the share is not made
        return grp.getgrnam(os.environ.get('ABVM_SHARE_GROUP', SB_QEMU_GROUP)).gr_gid
    except (ImportError, KeyError):
        pass
    root = sb_host()
    try:
        for entry in sorted(os.listdir(root)):
            st = os.stat(os.path.join(root, entry))
            if st.st_uid != os.getuid():
                return st.st_gid
    except OSError:
        pass
    return None


def sb_group_script(gid):
    """the guest's commands (run as root through sudo): a group with that gid (abvmshare when none is), and
    `autobleem` a member of it - both only when missing, so a second run changes nothing"""
    g = int(gid)
    return (f'g=$(getent group {g} | cut -d: -f1); '
            f'[ -n "$g" ] || {{ sudo groupadd -g {g} {SB_SHARE_GROUP}; g={SB_SHARE_GROUP}; }}; '
            f'id -G autobleem | tr " " "\\n" | grep -qx {g} || sudo usermod -aG "$g" autobleem; '
            f'id autobleem')


def sb_share_group(strict=False):
    """The folders the guest makes on the share are QEMU's (libvirt-qemu:libvirt-qemu, 775: QEMU's 9p backend
    chmods the guest's mode over any default ACL), and the guest checks permissions against that owner, so
    `autobleem` was "other" to them and could not write in them (curl exit 23 into System/Updates). Making it a
    member of the share's group is the fix; sb_open_tree stays as the safety net. The launcher is started by
    `sandbox start` in a fresh ssh session, so the new membership applies to it (a launcher already running keeps
    its old groups until it is restarted)."""
    gid = sb_share_gid()
    if gid is None:
        if strict:
            raise Fail(f'no group {SB_QEMU_GROUP} and no folder of QEMU\'s in {sb_host()}: the share\'s gid is unknown')
        print('warning: the share\'s group is unknown; the guest user may not write in folders it makes', file=sys.stderr)
        return None
    guest_run(f'set -e; {sb_group_script(gid)}', check=strict)
    return gid


DEBUG_TOOLS = ('valgrind', 'gdb')  # `sandbox start --valgrind` and `sandbox crash-report` need them in the guest


def sb_install_debug_tools():
    """the guest has free sudo: install what is missing of DEBUG_TOOLS (apt), nothing when all are there"""
    checks = ' '.join(f'command -v {t} >/dev/null || missing="$missing {t}";' for t in DEBUG_TOOLS)
    guest_run(f'missing=; {checks} [ -z "$missing" ] || {{ sudo apt-get update -qq; '
              f'sudo DEBIAN_FRONTEND=noninteractive apt-get install -y $missing; }}; '
              + ' && '.join(f'command -v {t}' for t in DEBUG_TOOLS))


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
    sb_share_group(strict=True)
    print(f'sandboxes: {SB_GUEST} in the guest = {sb_host()} here; covers in _shared/db')
    sb_core_setup()
    try:
        sb_install_debug_tools()
    except Fail as e:  # offline guest, mirror down: the rest of the setup stands
        print(f'WARNING: valgrind/gdb not installed: {e}; --valgrind and crash-report will say so')


def sb_core_setup():
    """the guest's cores go to CORE_LOCAL on its own disk, for good (sysctl.d + tmpfiles.d survive a reboot)"""
    line = shlex.quote(f'kernel.core_pattern={CORE_PATTERN}')   # through %s: the pattern's own % must not reach printf
    guest_run(f'set -e; printf "%s\\n" {line} | sudo tee /etc/sysctl.d/60-abvm-core.conf '
              f'>/dev/null; printf "d {CORE_LOCAL} 1777 root root 7d\\n" | sudo tee /etc/tmpfiles.d/abvm-core.conf '
              f'>/dev/null; sudo systemd-tmpfiles --create /etc/tmpfiles.d/abvm-core.conf; '
              f'sudo sysctl -q -w kernel.core_pattern={shlex.quote(CORE_PATTERN)}; cat /proc/sys/kernel/core_pattern')


def sb_template_source_write(source, version):
    with open(os.path.join(sb_host('_template'), 'TEMPLATE_SOURCE'), 'w') as f:
        f.write(f'source: {source}\nversion: {version}\nmade: {time.strftime("%Y-%m-%d %H:%M")}\n')


def sb_template_source():
    """the one-line "source, version, date" _template recorded, or '' when it has none"""
    try:
        with open(os.path.join(sb_host('_template'), 'TEMPLATE_SOURCE')) as f:
            return ', '.join(line.split(': ', 1)[-1].strip() for line in f if line.strip())
    except OSError:
        return ''


def sb_nightly_package(dest):
    """the newest pcusb nightly of the site (what `update` installs on the stick) downloaded into dest, sha256
    checked; returns the file"""
    import urllib.request
    latest = json.loads(urllib.request.urlopen(f'{SITE}/nightly/latest.json', timeout=30).read())
    version = latest['version']
    name = f'autobleem-pcusb-i386-{version}.tar.gz'
    path = os.path.join(dest, name)
    urllib.request.urlretrieve(f'{SITE}/nightly/{version}/{name}', path)
    sha = ((latest.get('files', {}).get('pcusb') or {}).get('sha256')
           or urllib.request.urlopen(f'{SITE}/nightly/{version}/{name}.sha256', timeout=30).read().decode().split()[0])
    with open(path, 'rb') as f:
        got = hashlib.sha256(f.read()).hexdigest()
    if got != sha:
        raise Fail(f'sha256 of the download ({got}) differs from the published {sha}')
    return path


def sb_clear_leftover(name):
    """remove _template.old / _template.new left by an earlier run: here first, then (root-owned files the guest
    made) by guest sudo"""
    path = sb_host(name)
    if not os.path.lexists(path):
        return
    shutil.rmtree(path, ignore_errors=True)
    if os.path.lexists(path):
        guest_run(f'sudo rm -rf {SB_GUEST}/{name}')


def sb_template_from_package(package):
    """_template from a pcusb release package (a .tar.gz path or URL, the folder it unpacks to, or `nightly`) in
    place of the stick: the same parts, built here on the share's host side, swapped in whole"""
    for leftover in ('_template.old', '_template.new'):
        sb_clear_leftover(leftover)
    work = tempfile.mkdtemp(prefix='abvm-template-')
    try:
        src = os.path.expanduser(package[5:] if package.startswith('host:') else package)
        if package == 'nightly':
            src = sb_nightly_package(work)
        elif re.match(r'https?://', package):
            import urllib.request
            src = os.path.join(work, os.path.basename(package))
            urllib.request.urlretrieve(package, src)
        if os.path.isfile(src):
            unpacked = os.path.join(work, 'unpacked')
            os.mkdir(unpacked)
            subprocess.run(['tar', 'xzf', src, '-C', unpacked], check=True)
            tops = os.listdir(unpacked)
            src_dir = os.path.join(unpacked, tops[0]) if len(tops) == 1 else unpacked
        else:
            src_dir = src
        bin_dir = os.path.join(src_dir, 'Autobleem', 'bin')
        if not os.path.isdir(os.path.join(bin_dir, 'autobleem')):
            raise Fail(f'--package {package}: no Autobleem/bin/autobleem in it - give a pcusb release package')
        t = sb_host('_template')
        new = t + '.new'
        shutil.rmtree(new, ignore_errors=True)
        for d in ('System/Databases', 'System/Logs', 'Games', 'Autobleem/bin'):
            os.makedirs(os.path.join(new, d))
        for part in ('autobleem', 'abpad'):
            if os.path.exists(os.path.join(bin_dir, part)):
                shutil.copytree(os.path.join(bin_dir, part), os.path.join(new, 'Autobleem', 'bin', part), symlinks=True)
        for rel, to in (('Autobleem/rc', 'Autobleem/rc'), ('Themes', 'Themes'), ('extensions', 'Extensions'),
                        ('Apps', 'Apps')):
            if os.path.isdir(os.path.join(src_dir, rel)):
                shutil.copytree(os.path.join(src_dir, rel), os.path.join(new, to), symlinks=True)
        version = ''
        if os.path.isfile(os.path.join(src_dir, 'VERSION')):
            shutil.copy(os.path.join(src_dir, 'VERSION'), os.path.join(new, 'VERSION'))
            with open(os.path.join(new, 'VERSION')) as f:
                version = f.read().strip()
        os.symlink('../../../_shared/db', os.path.join(new, 'Autobleem', 'bin', 'db'))
        old = t + '.old'
        if os.path.lexists(t):
            os.rename(t, old)
        os.rename(new, t)
        shutil.rmtree(old, ignore_errors=True)
        if os.path.lexists(old):
            print(f'old template left at _template.old - remove it with: sudo rm -rf {SB_GUEST}/_template.old')
        sb_template_source_write(os.path.basename(src.rstrip('/')), version or '(no VERSION)')
        sb_open_modes(t)
    finally:
        shutil.rmtree(work, ignore_errors=True)
    print(f'template made from the package {package} ({sb_template_source()})')


def sb_template(package=None):
    """_template: what every new sandbox starts from - the stick's launcher, themes, extensions and Apps (or, with
    `package`, a pcusb release package's: the stick is older than the product), no games, empty databases; the
    covers linked from _shared"""
    if package:
        return sb_template_from_package(package)
    t = f'{SB_GUEST}/_template'
    guest_run(f'set -e; mountpoint -q {SB_GUEST} || {{ echo "{SB_GUEST} is not mounted: abvm.py sandbox setup" >&2; '
              f'exit 1; }}; sudo rm -rf {t}.new; sudo mkdir -p {t}.new/Autobleem/bin {t}.new/System/Databases '
              f'{t}.new/System/Logs {t}.new/Games; '
              f'sudo cp -r {STICK}/Autobleem/bin/autobleem {STICK}/Autobleem/bin/abpad {t}.new/Autobleem/bin/; '
              f'sudo cp -r {STICK}/Autobleem/rc {t}.new/Autobleem/; '
              f'sudo cp -r {STICK}/Themes {STICK}/Extensions {STICK}/Apps {t}.new/; sudo cp {STICK}/VERSION {t}.new/; '
              f'sudo ln -s ../../../_shared/db {t}.new/Autobleem/bin/db; sudo rm -rf {t}; sudo mv {t}.new {t}')
    sb_open_modes(sb_host('_template'))
    try:
        with open(os.path.join(sb_host('_template'), 'VERSION')) as f:
            stick_version = f.read().strip()
    except OSError:
        stick_version = '(no VERSION)'
    sb_template_source_write(f'stick {STICK}', stick_version)
    print(f'template made from the stick ({STICK})')


def sb_copy_build(name, build):
    """a build's dist/<target> (its Autobleem/ tree) laid over the sandbox - the build under test, used where it lies"""
    src = os.path.expanduser(build[5:] if build.startswith('host:') else build)
    if not os.path.isdir(os.path.join(src, 'Autobleem')):
        raise Fail(f'{src} has no Autobleem/ - give a build\'s dist/<target> directory (on the test machine)')
    subprocess.run(['cp', '-r', os.path.join(src, 'Autobleem'), sb_host(name)], check=True)
    sb_open_modes(os.path.join(sb_host(name), 'Autobleem'))


def sb_copy_package(name, package):
    """a release package (autobleem-<target>-<arch>-<ver>.tar.gz, or the folder it unpacks to) laid over the
    sandbox: its launcher (Autobleem/), each theme in Themes/, each extension in extensions/ and VERSION - what a
    user of that release gets, where the template is only as new as the stick it was made from"""
    src = os.path.expanduser(package[5:] if package.startswith('host:') else package)
    unpacked = None
    if os.path.isfile(src):
        unpacked = tempfile.mkdtemp(prefix='abvm-package-')
        subprocess.run(['tar', 'xzf', src, '-C', unpacked], check=True)
        tops = os.listdir(unpacked)
        src = os.path.join(unpacked, tops[0]) if len(tops) == 1 else unpacked
    root = sb_host(name)
    try:
        if not os.path.isdir(os.path.join(src, 'Autobleem')):
            raise Fail(f'--package {package}: no Autobleem/ in it - give a release package (on the test machine)')
        subprocess.run(['cp', '-r', os.path.join(src, 'Autobleem'), root], check=True)
        sb_open_modes(os.path.join(root, 'Autobleem'))
        themes = os.path.join(src, 'Themes')
        for theme in sorted(os.listdir(themes)) if os.path.isdir(themes) else []:
            target = os.path.join(root, 'Themes', theme)
            if os.path.isdir(target):
                shutil.rmtree(target)  # replaced whole, so nothing of the old version stays behind
            shutil.copytree(os.path.join(themes, theme), target, symlinks=True)
        if os.path.isdir(themes):
            sb_open_modes(os.path.join(root, 'Themes'))
        extensions = os.path.join(src, 'extensions')
        for ext in sorted(os.listdir(extensions)) if os.path.isdir(extensions) else []:
            if os.path.isdir(os.path.join(extensions, ext)):
                sb_copy_ext(name, os.path.join(extensions, ext))
        if os.path.isfile(os.path.join(src, 'VERSION')):
            shutil.copy(os.path.join(src, 'VERSION'), os.path.join(root, 'VERSION'))
    finally:
        if unpacked:
            shutil.rmtree(unpacked, ignore_errors=True)
    print(f'sandbox {name}: release package {os.path.basename(package.rstrip("/"))} laid')


def import_make_usb():
    # the repository's tools/make_usb.py, or the copy `setup` puts next to this tool on the test machine
    sys.path[:0] = [HERE, os.path.join(HERE, '..')]
    import make_usb
    return make_usb


def sb_add_games(name, count):
    """make_usb.py's fake PS1 games (count, over a few folders), its three memory card sets (from the launcher's own
    blank cards) and fake Apps in the sandbox; the launcher's scan names the games from the shared cover DBs. Kept
    on a restart (make_usb skips what is there)"""
    make_usb = import_make_usb()
    games = os.path.join(sb_host(name), 'Games')
    make_usb.make_fake_games(games, count)
    make_usb.make_fake_memcards(games, os.path.join(sb_host(name), 'Autobleem', 'bin', 'autobleem', 'memcard'))
    make_usb.make_fake_apps(sb_host(name))
    for d in ('Games', 'Apps'):
        sb_open_modes(os.path.join(sb_host(name), d))


TEST_GAMES = 'test'   # `--games test`: the test machine's own set of real games (kept outside the template)
LINK_MIN_BYTES = 1 << 20   # files from 1 MB (the discs) are hard-linked, smaller ones (Game.ini, covers) copied
TEST_SET_PARTS = ('Games', 'RetroArch')   # what the set lays: a stick's PS1 games and RetroArch/roms/<system>/


def test_games_dir():
    return os.path.expanduser(os.environ.get('ABVM_TESTGAMES', '~/abvm/testgames'))


def parse_games(value):
    """--games N (fake games) or --games test; 0 when not given"""
    if value in (None, ''):
        return 0
    if value == TEST_GAMES:
        return TEST_GAMES
    if not re.fullmatch(r'\d{1,3}', value):
        raise Fail(f'--games {value!r}: a number of fake games, or "{TEST_GAMES}" for the test machine\'s game set')
    return int(value)


def sb_add_test_games(name):
    """the test set (test_games_dir(), laid out like a stick: Games/<game>/, RetroArch/roms/<system>/) laid over the
    sandbox: files of 1 MB and up hard-linked (no space taken; the launcher only reads a disc or a ROM), the small ones
    copied so the launcher's own writes (Game.ini, covers) stay in this sandbox. A file already there is kept"""
    src = test_games_dir()
    if not os.path.isdir(src):
        raise Fail(f'--games {TEST_GAMES}: no game set at {src} on the test machine (ABVM_TESTGAMES)')
    dest_root, laid = sb_host(name), 0
    for part in TEST_SET_PARTS:
        for root, dirs, files in os.walk(os.path.join(src, part)):
            dirs[:] = sorted(d for d in dirs if not d.startswith('.'))
            to = os.path.join(dest_root, os.path.relpath(root, src))
            os.makedirs(to, exist_ok=True)
            for f in sorted(files):
                a, b = os.path.join(root, f), os.path.join(to, f)
                if f.startswith('.') or os.path.lexists(b):
                    continue
                laid += 1
                if os.path.getsize(a) >= LINK_MIN_BYTES:
                    try:
                        os.link(a, b)
                        continue
                    except OSError:
                        pass   # another file system: a copy
                shutil.copy2(a, b)
        if os.path.isdir(os.path.join(dest_root, part)):
            sb_open_modes(os.path.join(dest_root, part))

    def names(*rel):
        d = os.path.join(src, *rel)
        return ', '.join(sorted(os.listdir(d))) if os.path.isdir(d) else 'none'
    print(f'sandbox {name}: test set laid ({laid} new files); PS1: {names("Games")}; '
          f'RetroArch: {names("RetroArch", "roms")}')


def sb_set_config(name, sets):
    """Key=Value lines in the sandbox launcher's config.ini - a line with that key replaced, else added at the end.
    Laid after --build/--package, which bring their own config.ini (a dist's has no Language= line)"""
    path = os.path.join(sb_host(name), 'Autobleem', 'bin', 'autobleem', 'config.ini')
    lines = open(path, encoding='utf-8').read().splitlines() if os.path.exists(path) else []
    for s in sets:
        key, sep, value = s.partition('=')
        if not sep or not key.strip():
            raise Fail(f'--set {s!r}: Key=Value, e.g. --set Language=Polski')
        key = key.strip()
        hits = [i for i, line in enumerate(lines) if line.split('=', 1)[0].strip().lower() == key.lower()]
        if hits:
            lines[hits[0]] = f'{key}={value}'
        else:
            lines.append(f'{key}={value}')
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(lines) + '\n')
    print(f'sandbox {name}: config.ini ' + ', '.join(sets))


def sb_lay(name, build=None, exts=(), package=None, games=0, sets=()):
    """what `new`/`start` lay over the sandbox, in this order: a release package, a build, extensions, fake games,
    config lines - so a build under test wins over the package, and --set over both"""
    if package:
        sb_copy_package(name, package)
    if build:
        sb_copy_build(name, build)
    for ext in exts:
        sb_copy_ext(name, ext)
    if games == TEST_GAMES:
        sb_add_test_games(name)
    elif games:
        sb_add_games(name, games)
    if sets:
        sb_set_config(name, sets)


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


STORE_STATE_DIRS = ('cache', os.path.join('cache', 'pictures'), 'downloads', 'staging', 'sources')
# every directory the launcher and the Store write to, relative to the sandbox root. The launcher makes some of them
# itself inside the guest, where they end up owned by the guest's user (libvirt-qemu on the host) and cannot be
# chmod'ed from here - so they are made, by the host's owner, before the first start
WRITTEN_DIRS = ('Mods', 'Apps')


def sb_make_store_dirs(name):
    """the directories the launcher and the Store write to: Mods/ (the Store's .mod install), Apps/ and the Store's
    state dirs System/Extensions/store/{cache,cache/pictures,downloads,staging,sources} (the Store makes none of them
    itself, so without them it cannot cache its catalog or its pictures - curl exit 23 - and its screens come out
    empty). Made here by the host's owner and opened like the rest of the tree (the guest checks permissions against
    the host's owner, see sb_open_modes), so the launcher's user can write them"""
    root = sb_host(name)
    base = os.path.join(root, 'System', 'Extensions', 'store')
    for d in STORE_STATE_DIRS:
        os.makedirs(os.path.join(base, d), exist_ok=True)
    for d in WRITTEN_DIRS:
        os.makedirs(os.path.join(root, d), exist_ok=True)
        sb_open_modes(os.path.join(root, d))
    sb_open_modes(os.path.join(root, 'System', 'Extensions'))


def sb_new(name, **lay):
    if os.path.exists(sb_host(name)):
        raise Fail(f'sandbox {name} exists - `sandbox reset {name}` starts it afresh')
    if not os.path.isdir(sb_host('_template')):
        raise Fail('no template yet: abvm.py sandbox template')
    subprocess.run(['cp', '-r', sb_host('_template'), sb_host(name)], check=True)
    sb_open_modes(sb_host(name))
    sb_make_store_dirs(name)
    sb_lay(name, **lay)
    print(f'sandbox {name} made' + (f' with {lay["build"]}' if lay.get('build') else ''))


def sb_relay(name):
    """the slot laid FRESH from the current template - what `new` does, after the launcher is stopped and the old
    folder is gone: a slot never carries the last user's state. Keeps the slot's name."""
    if not os.path.isdir(sb_host('_template')):
        raise Fail('no template yet: abvm.py sandbox template')
    sb_stop(name)  # also kills an orphan launcher of the slot
    if os.path.exists(sb_host(name)):
        sb_remove(name)
    sb_new(name)


def sb_take(name, task, minutes):
    """a pool slot's lease, and the slot laid fresh from the template. A holder who takes its own slot again only
    extends the lease: its work is not wiped."""
    global LEASE_KEY
    LEASE_KEY = f'sb-{name}'
    if not os.path.isdir(sb_host('_template')):
        raise Fail('no template yet: abvm.py sandbox template')
    mine = bool(WHO) and lease_op('show').startswith(f'held by {WHO} (')
    print(lease_op('take', WHO, task, minutes))
    if not mine:
        try:
            sb_relay(name)
        except BaseException:
            lease_op('release', WHO)
            raise


def sb_take_free(task, minutes):
    """the first pool slot with a free lease, laid fresh and leased to WHO; Busy when every slot is held"""
    global LEASE_KEY
    held = []
    for n in SB_POOL:
        LEASE_KEY = f'sb-{n}'
        holder = lease_op('show')
        if holder != 'free':
            held.append(f'{n} {holder.replace("held by ", "", 1)}')
            continue
        try:
            sb_take(n, task, minutes)  # a launcher left running there (orphan or not) is killed by the re-lay
        except Busy:  # taken by someone else between the check and now
            held.append(f'{n} taken just now')
            continue
        print(f'slot: {n}')
        return
    raise Busy(f'busy: no free sandbox slot ({sb_pool_text()}: {"; ".join(held)})')


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


def sb_reap_unleased(names):
    """stop the launchers among `names` whose lease is gone (expired, or released without a stop): their holder died
    or forgot them, and each takes a whole CPU and a place under the cap. One someone holds is never touched; one
    with no state file (no pid to stop) stays and is still counted. Returns the names still running."""
    global LEASE_KEY
    kept, saved = [], LEASE_KEY
    try:
        for n in names:
            if not sb_running(n):  # seen only in the guest's process list: no pid of ours to stop
                kept.append(n)
                continue
            LEASE_KEY = f'sb-{n}'
            if lease_op('show') == 'free':
                print(f'sandbox {n}: its launcher runs with no lease - stopping it')
                sb_stop(n)
            if sb_running(n):
                kept.append(n)
    finally:
        LEASE_KEY = saved
    return kept


RUN_SCRIPT = 'run-launcher.sh'
EXIT_FILE = 'exit-status'
CORE_DIR = 'core'
# the launcher's environment words a caller may pass (ABVM_SANDBOX_ENV): its own AB_* keys and the allocator/sanitizer
# debug keys; never the loader's (LD_PRELOAD, LD_LIBRARY_PATH)
DEBUG_ENV_KEYS = ('MALLOC_CHECK_', 'MALLOC_PERTURB_', 'MALLOC_ARENA_MAX', 'ASAN_OPTIONS', 'UBSAN_OPTIONS',
                  'LSAN_OPTIONS', 'MSAN_OPTIONS', 'G_DEBUG', 'G_SLICE', 'LD_DEBUG')

# where the kernel writes a core: the guest's own disk (a core dumped onto the 9p share comes out 0 bytes); %p is the
# launcher's pid, so two sandboxes never take each other's core. sandbox setup makes it permanent, the run script
# sets it again when it differs (a VM set up earlier)
CORE_LOCAL = '/var/tmp/abvm-core'
CORE_PATTERN = f'{CORE_LOCAL}/core.%e.%p'
UPX_MARK = 'memfd:upx'   # what gdb names the mapping of a UPX-packed binary

# runs in the guest in place of the bare launcher: cores allowed, the wait status and the time recorded when it ends,
# the launcher's core copied from CORE_LOCAL to <sandbox>/.abvm/core/ once it has ended. The launcher runs in the
# FOREGROUND (exec'd by a small sh that first writes its pid): a background job of a non-interactive sh would start
# with SIGINT ignored, and `sandbox signal INT` would then do nothing
RUN_SCRIPT_TEXT = '''#!/bin/sh
# run-launcher.sh <sandbox root> [valgrind]   (written by abvm.py)
root=$1
ab=$root/.abvm
local=${ABVM_CORE_LOCAL:-@CORE_LOCAL@}   # the variable only for the tests
ulimit -c unlimited 2>/dev/null
mkdir -p "$local" 2>/dev/null || sudo -n mkdir -p "$local"
sudo -n chmod 1777 "$local" 2>/dev/null
[ "$(cat /proc/sys/kernel/core_pattern)" = "@CORE_PATTERN@" ] || sudo -n sysctl -q -w kernel.core_pattern="@CORE_PATTERN@" >/dev/null 2>&1
rm -f "$ab/launcher.pid"
if [ "$2" = valgrind ]; then
    sh -c 'echo $$ > "$0"; exec valgrind --log-file="$1/System/Logs/valgrind.txt" --error-limit=no --num-callers=40 ./autobleem-gui "$1"' "$ab/launcher.pid" "$root"
else
    sh -c 'echo $$ > "$0"; exec ./autobleem-gui "$1"' "$ab/launcher.pid" "$root"
fi
s=$?
pid=$(cat "$ab/launcher.pid" 2>/dev/null)
mkdir -p "$ab/core"
for c in ${pid:+"$local"/core.*."$pid"} core core.*; do
    [ -s "$c" ] && { cp -f "$c" "$ab/core/" 2>/dev/null || sudo -n cp -f "$c" "$ab/core/"; }   # an empty core is no core
    [ -f "$c" ] && { rm -f "$c" 2>/dev/null || sudo -n rm -f "$c"; }
done
chmod a+rw "$ab/core"/* 2>/dev/null   # the kernel writes a core 0600: open it to the other readers of the share
echo "$s $(date +%s)" > "$ab/exit-status.tmp" && mv -f "$ab/exit-status.tmp" "$ab/exit-status"
exit "$s"
'''.replace('@CORE_LOCAL@', CORE_LOCAL).replace('@CORE_PATTERN@', CORE_PATTERN)


def sb_check_env_words(words):
    for w in words:
        key = w.split('=', 1)[0]
        if '=' not in w or not (re.fullmatch(r'AB_[A-Z0-9_]+', key) or key in DEBUG_ENV_KEYS):
            raise Fail(f'ABVM_SANDBOX_ENV {w!r}: NAME=value with NAME an AB_* key or one of {", ".join(DEBUG_ENV_KEYS)}')
    return words


def sb_write_run_script(name):
    d = os.path.join(sb_host(name), '.abvm')
    os.makedirs(d, exist_ok=True)
    with open(os.path.join(d, RUN_SCRIPT), 'w', newline='\n') as f:
        f.write(RUN_SCRIPT_TEXT)
    sb_open_modes(d)


def sb_clear_run_record(name):
    d = os.path.join(sb_host(name), '.abvm')
    for f in (EXIT_FILE, EXIT_FILE + '.tmp'):
        try:
            os.remove(os.path.join(d, f))
        except OSError:
            pass
    shutil.rmtree(os.path.join(d, CORE_DIR), ignore_errors=True)


def parse_exit_record(text):
    """'<shell status> <epoch>' as the run script writes it -> {launcher_exit, launcher_signal, finished_at}. A status
    above 128 is the shell's way to say "killed by signal status-128" (134 = SIGABRT = 6); a launcher exit code in
    129..192 would read the same, the launcher has none."""
    m = re.fullmatch(r'\s*(\d+)\s+(\d+)\s*', text or '')
    if not m:
        return {'launcher_exit': None, 'launcher_signal': None, 'finished_at': None}
    status, at = int(m.group(1)), int(m.group(2))
    return {'launcher_exit': status, 'launcher_signal': status - 128 if 128 < status <= 192 else None,
            'finished_at': at}


def sb_exit_status(name):
    try:
        with open(os.path.join(sb_host(name), '.abvm', EXIT_FILE)) as f:
            return parse_exit_record(f.read())
    except OSError:
        return parse_exit_record('')


def sb_core_files(name):
    d = os.path.join(sb_host(name), '.abvm', CORE_DIR)
    return sorted(os.path.join(d, f) for f in os.listdir(d)) if os.path.isdir(d) else []


def sb_crash_report(name):
    """text: the last run's end, and for a death by signal a gdb backtrace (all threads) of the kept core, with the
    symbols of the sandbox's launcher binary; says so when there is no core or no gdb"""
    st = sb_exit_status(name)
    if sb_running(name):
        return f'sandbox {name}: the launcher runs - no crash report'
    if st['launcher_exit'] is None:
        return f'sandbox {name}: no finished run recorded'
    head = f'launcher exit {st["launcher_exit"]}'
    if st['launcher_signal'] is None:
        return f'{head}: ended normally, no crash'
    head += f', signal {st["launcher_signal"]}'
    found = sb_core_files(name)
    cores = [c for c in found if os.path.getsize(c) > 0]
    if found and not cores:
        return (f'{head}\nthe kept core is empty (0 bytes): {found[0]} - the kernel could not write it there (a core '
                f'dumped onto the 9p share comes out empty); run `sandbox setup` so cores go to {CORE_LOCAL}')
    if not cores:
        pattern = guest_run('cat /proc/sys/kernel/core_pattern', check=False).strip()
        return (f'{head}\nno core file: the guest\'s core_pattern is {pattern!r}'
                + (' (piped - no core lands in the sandbox; `sandbox setup` sets it to ' + CORE_PATTERN + ')'
                   if pattern.startswith('|') else f' (a core is kept only when the kernel writes it as {CORE_PATTERN}'
                                                    f' - `sandbox setup` sets that)'))
    if guest_run('command -v gdb', check=False).strip() == '':
        return f'{head}\ncore kept: {cores[0]}\ngdb is not installed in the guest - no backtrace'
    g = f'{SB_GUEST}/{name}'
    core = f'{g}/.abvm/{CORE_DIR}/{os.path.basename(cores[0])}'
    exe = f'{g}/Autobleem/bin/autobleem/autobleem-gui'
    # the kernel writes a core 0600 for the crashing process's user; the guest's free sudo opens the kept core to
    # every reader and gdb runs as root, so the owner of the core no longer matters
    cdir = shlex.quote(f'{g}/.abvm/{CORE_DIR}')
    bt = guest_run(f'sudo chmod -R a+rX {cdir} 2>/dev/null; sudo gdb -batch -nx -ex "set pagination off" '
                   f'-ex "thread apply all bt full" {shlex.quote(exe)} {shlex.quote(core)} 2>&1', check=False)
    if 'Permission denied' in bt or 'sudo:' in bt:
        return (f'{head}\ncore kept: {cores[0]}\nno backtrace: the guest could not read the core even through sudo '
                f'(the share keeps it closed to root); gdb said:\n{bt.rstrip()}')
    if not re.search(r'^#\d+ ', bt, re.M):
        return f'{head}\ncore: {cores[0]}\nno backtrace: gdb gave no frames; it said:\n{bt.rstrip()}'
    if UPX_MARK in bt:   # a package's launcher (the template, a nightly) is UPX-packed: gdb sees no symbol in it
        head += ('\nNOTE: this launcher is UPX-packed (a package binary) - the frames have no names; start the sandbox '
                 'with --build <a pcusb build dist> for a backtrace with symbols')
    return f'{head}\ncore: {cores[0]}\n{bt.rstrip()}'


def sb_start(name, size=None, valgrind=False, **lay):
    size = size or os.environ.get('ABVM_SANDBOX_SIZE', '1280x720')
    if not sb_running(name):  # refuse before the sandbox is made or a build is laid over it
        others = sb_running_names(exclude=name)
        if len(others) >= SB_SLOTS:
            others = sb_reap_unleased(others)
        if len(others) >= SB_SLOTS:
            raise Busy(sb_full_message(others))
    if not os.path.isdir(sb_host(name)):
        sb_new(name, **lay)
    else:
        sb_lay(name, **lay)
    if sb_running(name):
        print(f'sandbox {name} already runs on port {sb_state(name)["port"]}')
        return
    sb_stop_forward(sb_state(name))  # a launcher that ended without `sandbox stop` (a crash, a SIGTERM) left it
    running = sb_running_names(exclude=name)
    if len(running) >= SB_SLOTS:
        raise Busy(sb_full_message(running))
    used = {sb_state(n).get('port') for n in running} | sb_guest_ports()  # an orphan has no state file

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
           f'AB_WINDOW_SIZE={size} AB_DEBUG_OUT={g}/.abvm/out AB_PAD_BATTERY_DIR={g}/.abvm/power_supply AB_MAX_FPS=30 AB_AMBIENT_FPS=5')
    extra = os.environ.get('ABVM_SANDBOX_ENV', '')  # more for the launcher, e.g. AB_FRAME_STATS=1
    if extra:
        env += ' ' + ' '.join(shlex.quote(w) for w in sb_check_env_words(extra.split()))
    valgrind = valgrind or os.environ.get('ABVM_SANDBOX_VALGRIND') == '1'
    t0 = time.time()
    # made here, open to the guest (a folder the guest makes through the share is QEMU's, and closed to this user)
    for sub in ('out', 'power_supply'):
        os.makedirs(os.path.join(sb_host(name), '.abvm', sub), exist_ok=True)
    sb_open_modes(os.path.join(sb_host(name), '.abvm'))
    sb_share_group()  # a VM set up before the group existed gets it here
    sb_open_tree(name, acl=True)  # the whole tree, what an earlier run's guest made too
    # the redirect below fails (and no launcher starts, no log is written) when System/Logs is missing
    os.makedirs(os.path.join(sb_host(name), 'System', 'Logs'), exist_ok=True)
    sb_open_modes(os.path.join(sb_host(name), 'System'))
    sb_write_run_script(name)
    sb_clear_run_record(name)  # this run's record replaces the last one's
    out = guest_run(f'set -e; mountpoint -q {SB_GUEST}; mkdir -p {rt}; '
                    f'cd {g}/Autobleem/bin/autobleem; umask 000; '
                    f'{env} setsid nohup sh {g}/.abvm/{RUN_SCRIPT} {g}{" valgrind" if valgrind else ""} '
                    f'> {g}/System/Logs/abvm-out.txt 2>&1 < /dev/null & '
                    f'echo pid $!; for i in $(seq {600 if valgrind else 100}); do '
                    f'l=$(ss -ltnp | grep "127.0.0.1:{port} " || true); '
                    f'if [ -n "$l" ]; then echo "listen $l"; break; fi; sleep 0.1; done')
    pid = int(re.search(r'pid (\d+)', out).group(1))
    owner = sb_port_owner(name, pid, out)
    if not owner:
        sb_kill_launcher(name, pid)  # never an orphan: no state file would ever name it
        sb_save_state(name, {})
        raise Fail(f'sandbox {name}: its launcher did not open its driver port {port} in 10 s (or the port is held by '
                   f'another process) - launcher stopped, see {sb_host(name)}/System/Logs/abvm-out.txt')
    pid = owner
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
            d.wait_screen('GuiLauncher', 600 if valgrind else 60)
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


class ProbeReply(str):
    """a probe's output: the guest's own view, its paths not mapped to this machine's"""


def sb_probe_path(name, rel):
    """the guest's path of `rel` inside sandbox `name`; Fail for an absolute path or a `..`"""
    rel = rel.strip()
    if not rel or rel.startswith(('/', '\\')) or re.match(r'^[A-Za-z]:', rel) or '..' in re.split(r'[\\/]+', rel):
        raise Fail(f'probe {rel!r}: a path inside the sandbox, relative, with no ..')
    return posixpath.normpath(f'{SB_GUEST}/{name}/{rel}')


def sb_probe(name, rel):
    """read-only, in the guest: who the launcher's user is and who owns the path and its parent, with what mode"""
    path = sb_probe_path(name, rel)
    root = f'{SB_GUEST}/{name}'
    parent = posixpath.dirname(path) if path != root else root
    q, qp = shlex.quote(path), shlex.quote(parent)
    out = guest_run(f"id autobleem; stat -c '%U:%G %a %n' {q} {qp}; ls -la {q}; true", check=False)
    return ProbeReply(f'probe {rel.strip()}:\n{out.rstrip()}')


def sb_split_probes(name, script):
    """[('drive', 'step; step'), ('probe', 'rel/path'), ...] in order: a `probe <path>` step is the abvm's own and
    never goes to the DebugDriver. Every probe path is checked here, before anything runs."""
    segments, batch = [], []
    for part in script.split(';'):
        part = part.strip()
        if not part:
            continue
        if part.split(None, 1)[0] == 'probe':
            rel = part[len('probe'):].strip()
            sb_probe_path(name, rel)
            if batch:
                segments.append(('drive', '; '.join(batch)))
                batch = []
            segments.append(('probe', rel))
        else:
            batch.append(part)
    if batch:
        segments.append(('drive', '; '.join(batch)))
    return segments


def sb_drive(name, script, out_dir):
    """on the test machine: the script through the sandbox's driver. Its shots, clips and grabs land in the
    sandbox's own <sandbox>/.abvm/out/<run>/ - the launcher writes shots and clip frames there itself (its
    AB_DEBUG_OUT, this machine's folder through the share), grabs come over the socket; a clip becomes an MP4"""
    segments = sb_split_probes(name, script)  # a bad probe path fails before the run starts
    run = uuid.uuid4().hex[:8]
    run_dir = os.path.join(sb_host(name), '.abvm', 'out', run)
    os.makedirs(run_dir, exist_ok=True)
    guest_root = f'{SB_GUEST}/{name}/'

    def to_host(path):  # the guest's name for a sandbox file -> this machine's
        return os.path.join(sb_host(name), path[len(guest_root):]) if path.startswith(guest_root) else None

    # before each step the tree is opened again: a save folder made by the first game launch is the guest's
    d = sb_driver(name, out_prefix=run + '/', path_map=to_host, before_step=lambda: sb_open_tree(name))
    cwd = os.getcwd()
    os.chdir(run_dir)
    failure = None
    try:
        replies = []
        for kind, text in segments:
            if kind == 'probe':
                replies.append(sb_probe(name, text))
                continue
            try:
                replies += d.run(text)
            except import_ab_drive().RunFailed as e:
                replies += e.replies  # a failed run keeps what it made: the replies and the run's folder
                failure = e
                break
        for reply in replies:
            if isinstance(reply, ProbeReply):
                print(reply)
                continue
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
    orphan = sb_orphans().get(name)
    if orphan:  # a launcher of this sandbox with no state file: found by the process list
        sb_kill_pids(orphan)
        print(f'sandbox {name}: orphan launcher (pid {", ".join(map(str, orphan))}) killed')
    print(f'sandbox {name} stopped')


SB_SIGNALS = ('ABRT', 'SEGV', 'TERM', 'KILL', 'INT')  # the only signals `sandbox signal` sends


def sb_signal_name(sig):
    """'abrt' / 'SIGABRT' / 'ABRT' -> 'ABRT'; Fail for a signal outside SB_SIGNALS"""
    s = (sig or '').strip().upper()
    s = s[3:] if s.startswith('SIG') else s
    if s not in SB_SIGNALS:
        raise Fail(f'sandbox signal: {sig!r} is not allowed (only {", ".join(SB_SIGNALS)})')
    return s


def sb_signal(name, sig):
    """send `sig` to the launcher process of sandbox `name` (the pid its state file names, checked to be that
    sandbox's launcher); the run script then records the death in exit-status"""
    s = sb_signal_name(sig)
    st = sb_running(name)
    if not st:
        raise Fail(f'sandbox {name}: no launcher runs - nothing to signal')
    pid = int(st['pid'])
    out = guest_run(f'kill -0 {pid} 2>/dev/null && kill -{s} {pid} 2>/dev/null && echo sent || echo gone', check=False)
    if out.strip() != 'sent':
        print(f'sandbox {name}: launcher already gone (pid {pid}) - SIG{s} not delivered; see exit-status')
        return
    print(f'sandbox {name}: SIG{s} sent to the launcher (pid {pid})')


def sb_release(name):
    """give a sandbox's lease back - and stop its launcher first if the holder forgot to: a launcher left running
    takes a whole CPU and a slot of the cap. Only the holder's own release (or one of a free lease) stops it; a
    release refused because someone else holds the sandbox touches nothing."""
    holder = lease_op('show')
    if WHO and (holder == 'free' or holder.startswith(f'held by {WHO} (')) and sb_running(name):
        print(f'sandbox {name}: the launcher is still running - stopping it before the release')
        sb_stop(name)
    print(lease_op('release', WHO))


def sb_remove(name):
    root = sb_host(name)
    r = subprocess.run(['rm', '-rf', root], capture_output=True, text=True)
    if os.path.exists(root):
        # a file the guest made without write access for us - the guest's root can remove it
        guest_run(f'sudo rm -rf {SB_GUEST}/{name}', check=False)
    if os.path.exists(root):
        raise Fail(f'cannot remove {root}: {r.stderr.strip()}')


def sb_list():
    print(f'template: {sb_template_source() or "none, or made before it was recorded"}')
    global LEASE_KEY
    orphans = sb_orphans()
    for n in SB_POOL:
        st = sb_running(n)
        LEASE_KEY = f'sb-{n}'
        holder = lease_op('show')
        state = (f'running on {st["port"]}' if st else
                 f'orphan (pid {", ".join(map(str, orphans[n]))})' if n in orphans else 'stopped')
        print(f'{n:16} {state:18} lease: {holder}')
    LEASE_KEY = 'vm'
    legacy = sb_legacy()
    if legacy:
        print(f'legacy folders: {len(legacy)} (abvm sandbox rm-legacy removes the stopped, unleased ones; '
              f'_template and _shared stay)')


def sb_legacy():
    """the sandbox folders outside the pool (made before the pool rule); _template, _shared and the like never count"""
    return [n for n in sb_names() if n not in SB_POOL and n not in SB_PROTECTED]


def sb_tree_bytes(path):
    total = 0
    for d, _, files in os.walk(path):
        for f in files:
            try:
                total += os.lstat(os.path.join(d, f)).st_size
            except OSError:
                pass
    return total


def sb_rm_legacy(dry_run=False):
    """remove the legacy folders that are stopped and have no lease; a pool slot, _template, _template.old, _shared
    and TEMPLATE_SOURCE are never touched. Prints what went and the GB freed."""
    global LEASE_KEY
    orphans = sb_orphans()
    freed, removed = 0, 0
    try:
        for n in sb_legacy():
            LEASE_KEY = f'sb-{n}'
            holder = lease_op('show')
            if holder != 'free':
                print(f'kept {n}: {holder}')
            elif sb_running(n) or n in orphans:
                print(f'kept {n}: its launcher runs')
            else:
                size = sb_tree_bytes(sb_host(n))
                if dry_run:
                    print(f'would remove {n} ({size / 1e9:.2f} GB)')
                else:
                    sb_remove(n)
                    print(f'removed {n} ({size / 1e9:.2f} GB)')
                freed += size
                removed += 1
    finally:
        LEASE_KEY = 'vm'
    print(f'{"would free" if dry_run else "freed"} {freed / 1e9:.2f} GB in {removed} folders')


SANDBOX_FLAGS = ('--build', '--ext', '--package', '--games', '--set', '--size')


def check_sandbox_flags(rest):
    """sandbox new|start|reset <name> [flags]: an unknown flag, or one without its value, is an error - never ignored"""
    i = 0
    while i < len(rest):
        if rest[i] == '--valgrind':  # the one flag with no value
            i += 1
            continue
        if rest[i] not in SANDBOX_FLAGS:
            raise Fail(f'sandbox: unknown argument {rest[i]!r} (flags: {" ".join(SANDBOX_FLAGS)})')
        if i + 1 >= len(rest):
            raise Fail(f'sandbox: {rest[i]} needs a value')
        i += 2


def sandbox_command(args, out_dir):
    global LEASE_KEY
    sub = args[0] if args else 'list'
    name = (sb_name(args[1]) if len(args) > 1 and sub not in ('list', 'setup', 'template', 'take-free', 'rm-legacy')
            else '')
    if name and sub != 'rm':  # only `rm` may name an old folder outside the pool (the clean-up)
        sb_pool_check(name)
    if sub in ('new', 'start', 'reset'):
        check_sandbox_flags(args[2:])
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
            if args[1:2] == ['--package'] and len(args) == 3:
                sb_template(args[2])
            elif len(args) > 1:
                raise Fail('sandbox template [--package <pcusb .tar.gz|URL|dir|nightly>]')
            else:
                sb_template()
        finally:
            lease_op('release', WHO)
    elif sub == 'take-free':
        if len(args) < 2:
            raise Fail('sandbox take-free <task> [minutes] [--who <name>]')
        sb_take_free(args[1], int(args[2]) if len(args) > 2 else LOCK_MINUTES)
    elif sub == 'rm-legacy':
        if args[1:] not in ([], ['--dry-run']):
            raise Fail('sandbox rm-legacy [--dry-run]')
        sb_rm_legacy(dry_run=bool(args[1:]))
    elif sub in ('take', 'release'):
        if not name:
            raise Fail(f'sandbox {sub} <name>' + (' <task> [minutes]' if sub == 'take' else ''))
        LEASE_KEY = f'sb-{name}'
        if sub == 'take':
            sb_take(name, args[2] if len(args) > 2 else '', int(args[3]) if len(args) > 3 else LOCK_MINUTES)
        else:
            sb_release(name)
    elif sub == 'logs':
        path = os.path.join(sb_host(name), 'System', 'Logs', 'abvm-out.txt')
        n = int(args[2]) if len(args) > 2 else 40
        with open(path, errors='replace') as f:
            print(''.join(f.readlines()[-n:]), end='')
    elif sub in ('exit-status', 'crash-report'):  # read-only: no lease
        if not name:
            raise Fail(f'sandbox {sub} <name>')
        if sub == 'exit-status':
            print(json.dumps(sb_exit_status(name)))
        else:
            print(sb_crash_report(name))
    elif sub == 'signal':
        if not name or len(args) != 3:
            raise Fail(f'sandbox signal <name> <{"|".join(SB_SIGNALS)}>')
        sb_signal_name(args[2])  # refused before the lease or the guest is touched
        LEASE_KEY = f'sb-{name}'
        lease_op('check', WHO)
        sb_signal(name, args[2])
    elif sub in ('new', 'start', 'drive', 'stop', 'reset', 'rm'):
        if not name:
            raise Fail(f'sandbox {sub} <name>')
        LEASE_KEY = f'sb-{name}'
        lease_op('check', WHO)
        def one(flag):
            return args[args.index(flag) + 1] if flag in args else None

        def every(flag):
            return [args[i + 1] for i, a in enumerate(args) if a == flag and i + 1 < len(args)]

        size = one('--size')
        lay = dict(build=one('--build'), exts=every('--ext'), package=one('--package'),
                   games=parse_games(one('--games')), sets=every('--set'))
        if sub == 'new':
            sb_new(name, **lay)
        elif sub == 'start':
            sb_start(name, size, valgrind='--valgrind' in args, **lay)
        elif sub == 'drive':
            sb_drive(name, _script_from_args(args[2:]), out_dir)
        elif sub == 'stop':
            sb_stop(name)
        elif sub == 'reset':
            sb_stop(name)
            sb_remove(name)
            sb_new(name, **lay)
        elif sub == 'rm':
            sb_stop(name)
            sb_remove(name)
            lease_op('release', WHO)
            print(f'sandbox {name} removed')
    else:
        raise Fail('sandbox list | setup | template [--package <pcusb package|nightly>] | take-free <task> [min] | '
                   'take <name> <task> [min] | release <name> | rm-legacy [--dry-run] | '
                   'new|start <name> [--build <dist dir>] [--ext <zip|dir>]... [--valgrind] | drive <name> "<script>" | '
                   'logs <name> [n] | exit-status <name> | crash-report <name> | signal <name> <SIG> | stop|reset|rm <name>')


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


# ------------------------------------------------------------------ the VM sleeps when unused

STATE_DIR = '~/.local/state/abvm'           # on the test machine, next to the lease
LAST_USE_FILE = STATE_DIR + '/last-use'     # epoch seconds of the last command that took a lease or drove
SUSPENDED_FILE = STATE_DIR + '/vm-suspended.json'
IDLE_MINUTES = 20
RESUME_WAIT = 60                            # seconds the guest has to answer ssh after a resume
# commands that need the guest awake; sandbox, pad and run delegate to the test machine's copy, which resumes it there
GUEST_COMMANDS = {'status', 'install', 'restore', 'restart', 'update', 'remount', 'clip', 'pad', 'run', 'drive',
                  'padsim-install', 'guest', 'sandbox'}
DELEGATING = {'sandbox', 'pad', 'run'}
SANDBOX_NO_USE = ('list', 'logs')           # the sandbox subcommands that are not a "use"


def state_path(name):
    return os.path.expanduser(f'{STATE_DIR}/{name}')


def touch_last_use():
    path = state_path('last-use')
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path + '.tmp', 'w') as f:
        f.write(str(int(time.time())))
    os.replace(path + '.tmp', path)


def last_use():
    """epoch seconds of the last use, or None when never recorded"""
    try:
        with open(state_path('last-use')) as f:
            return float(f.read().strip())
    except (OSError, ValueError):
        return None


def counts_as_use(cmd, args):
    if cmd in NEEDS_LEASE:
        return True
    if cmd == 'lock':
        return args[:1] == ['take']
    if cmd == 'sandbox':
        return (args[0] if args else 'list') not in SANDBOX_NO_USE
    return False


def vm_state():
    return host_run(f'{VIRSH} domstate {shlex.quote(DOMAIN)}', check=False).strip() or 'unknown'


def vm_guest_answers():
    try:
        return host_run(f'{guest_ssh()} true', full=True).returncode == 0
    except Fail:  # libvirt reports no address (yet)
        return False


def vm_ensure_running(wait=RESUME_WAIT):
    """a suspended (paused) VM is resumed and the guest awaited; returns the one line to print"""
    state = vm_state()
    if state == 'running':
        return 'running'
    if state != 'paused':
        raise Fail(f'the VM {DOMAIN} is {state}, not running or suspended - start it ({VIRSH} start {DOMAIN})')
    print('abvm: resuming the VM...', file=sys.stderr)
    start = time.time()
    host_run(f'{VIRSH} resume {shlex.quote(DOMAIN)}')
    try:
        os.remove(state_path('vm-suspended.json'))
    except OSError:
        pass
    touch_last_use()  # woken for a reason: the idle clock starts now
    while not vm_guest_answers():
        if time.time() - start > wait:
            raise Fail(f'the VM was resumed but the guest did not answer ssh in {wait} s')
        time.sleep(2)
    return f'resumed (guest answered after {int(time.time() - start)} s)'


def vm_prepare(cmd, args):
    """before a command: wake the VM if it needs the guest, and record the use. On a PC the test machine's copy does it
    (state files live there)."""
    use = counts_as_use(cmd, args)
    if LOCAL:
        if cmd in GUEST_COMMANDS:
            vm_ensure_running()
        if use:
            touch_last_use()
    elif cmd in GUEST_COMMANDS and cmd not in DELEGATING:
        remote_tool(['vm', 'ensure-running'] + (['--touch'] if use else []))


def idle_blocker(minutes, now=None):
    """why the VM must stay awake (the first reason found), or None when it is idle. Raises Fail when it cannot tell."""
    global LEASE_KEY
    now = now or time.time()
    saved = LEASE_KEY
    try:
        LEASE_KEY = 'vm'
        holder = lease_op('show')
        if holder != 'free':
            return f'the stick launcher is in use: the VM lease {holder}'
        for n in SB_POOL:
            LEASE_KEY = f'sb-{n}'
            holder = lease_op('show')
            if holder != 'free':
                return f'sandbox slot {n} has a lease: {holder}'
    finally:
        LEASE_KEY = saved
    used = last_use()
    if used is None:
        touch_last_use()
        return 'no last-use record - the idle clock starts now'
    if now - used < minutes * 60:
        return f'used {int((now - used) / 60)} min ago (less than {minutes})'
    r = host_run(guest_ssh() + ' ' + shlex.quote('for p in $(pidof autobleem-gui); do tr "\\0" " " < '
                                                 '/proc/$p/cmdline 2>/dev/null; echo; done; true'), full=True)
    if r.returncode != 0:
        raise Fail(f'cannot look into the guest (exit {r.returncode}): {r.stderr.strip()} - the VM is left alone')
    running = sorted(sb_names_in_cmdlines(r.stdout) | {n for n in sb_names() if sb_running(n)})
    if running:
        return f'a launcher runs in the guest: {", ".join(running)} (an orphan too)'
    return None


def vm_idle_check(minutes, dry_run):
    import json
    state = vm_state()
    if state == 'paused':
        return 'already suspended'
    if state != 'running':
        return f'not running: {state}'
    why = idle_blocker(minutes)
    if why:
        return f'not idle: {why}'
    if dry_run:
        return f'would suspend: idle for {minutes} min'
    host_run(f'{VIRSH} suspend {shlex.quote(DOMAIN)}')
    path = state_path('vm-suspended.json')
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, 'w') as f:
        json.dump({'at': time.time(), 'how': 'virsh suspend', 'idle_minutes': minutes}, f)
    return f'suspended: idle for {minutes} min'


def vm_command(args):
    op = args[0] if args else ''
    if not LOCAL:  # the state files and the leases are the test machine's
        print(remote_tool(['vm'] + args), end='')
        return 0
    if op == 'idle-check':
        rest, minutes = args[1:], IDLE_MINUTES
        if '--minutes' in rest:
            i = rest.index('--minutes')
            try:
                minutes = float(rest[i + 1])
            except (IndexError, ValueError):
                raise UsageError('vm idle-check [--minutes N] [--dry-run]')
            del rest[i:i + 2]
        if minutes < 0 or [a for a in rest if a != '--dry-run']:
            raise UsageError('vm idle-check [--minutes N] [--dry-run]')
        minutes = int(minutes) if minutes == int(minutes) else minutes
        print(vm_idle_check(minutes, '--dry-run' in rest))
    elif op == 'ensure-running':
        print(vm_ensure_running())
        if '--touch' in args:
            touch_last_use()
    elif op == 'touch':
        touch_last_use()
    elif op == 'status':
        import json
        used = last_use()
        print(f'vm        {DOMAIN}: {vm_state()}')
        print('last use  ' + (time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(used)) if used else 'never recorded'))
        try:
            with open(state_path('vm-suspended.json')) as f:
                at = json.load(f)['at']
            print('suspended since ' + time.strftime('%Y-%m-%d %H:%M:%S', time.localtime(at)))
        except (OSError, ValueError, KeyError):
            pass
    else:
        raise UsageError('vm idle-check [--minutes N] [--dry-run] | ensure-running | status')
    return 0


class UsageError(Fail):
    pass


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
    if not LOCAL:
        mine, theirs = tool_version(), remote_version()
        stale = sorted(n for n in mine if theirs.get(n) != mine[n])
        print(f'abvm      {"on the test machine, current" if not stale else "stale on the test machine (" + ", ".join(stale) + ") - the next remote call refreshes it, or run `abvm.py setup`"}')
    else:
        print('abvm      on the test machine')
    return 0


def setup():
    # the tool, the DebugDriver's client (`sandbox drive` there) and the fake games and Apps (`--games N` there);
    # every remote call refreshes them by itself when they differ (ensure_remote_current) - this is the manual form
    push_remote()
    print(f'copied to {HOST}:{REMOTE_TOOL} (and ab_drive.py, make_usb.py)')


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
        vm_prepare(cmd, args)
        if cmd == 'status':
            return status()
        elif cmd == 'vm':
            return vm_command(args)
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
        elif cmd == 'update':
            update(args[args.index('--version') + 1] if '--version' in args else None)
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
            print(f'abvm: unknown command {cmd!r}', file=sys.stderr)
            print(__doc__, file=sys.stderr)
            return 2
    except UsageError as e:
        print(f'abvm: {e}', file=sys.stderr)
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
