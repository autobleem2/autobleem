#!/usr/bin/env python3
"""Checks an AutoBleem scanner processor before it is published (docs/scanner-processors-plan.md).

    python tools/proc_check.py <processor folder> --games <sample Games tree>
    python tools/proc_check.py <processor folder> --roms <sample roms tree>
    python tools/proc_check.py <processor folder> --games DIR --roms DIR [--key KEY] [--no-interrupt] [-v]

The processor folder is the one that goes into System/Processors/<name>/ (processor.ini + bin/<key>/...).
The sample trees are never touched: every run works on a scratch copy. What is checked:

  processor.ini   Name, Version, Exec and Kinds are there and make sense; Match, Systems, Order, Timeout and
                  Modifies parse; the program resolves for this machine's key (--key to pick another one)
  --version       exits 0 with one "#Name Vx.y - description" line
  each kind       for every Kinds= entry the sample trees can feed (games-folder, roms-folder on the trees;
                  ps1 on every game folder, rom on every ROM file):
                    - --ismine answers 0 or 1 (an item kind)
                    - --start: the first line is "#Starting - ...", every line is protocol or chatter
                      (chatter is reported), it ends in #DONE with exit 0 or #ERROR with exit != 0, and never
                      goes quiet for longer than Timeout=
                    - no *.part file is left behind, and nothing outside its target changed
                    - run again on its own output it says nothing but #Starting/#DONE and changes nothing
                      (idempotent)
                    - stopped in the middle (after its first progress line) and started again, it finishes
                      and leaves no *.part (atomic) - skip with --no-interrupt

Exit status: 0 when nothing FAILED (warnings allowed), 1 otherwise.
"""
import argparse
import os
import platform
import re
import shutil
import signal
import subprocess
import sys
import tempfile
import threading
import time

KINDS = ('games-folder', 'roms-folder', 'ps1', 'rom')
IGNORED = ('game.ini', 'pcsx.cfg')  # the launcher's own files in a game folder


class Report:
    def __init__(self, verbose):
        self.verbose = verbose
        self.failed = 0
        self.warned = 0

    def ok(self, text):
        print('  ok    ' + text)

    def warn(self, text):
        self.warned += 1
        print('  WARN  ' + text)

    def fail(self, text):
        self.failed += 1
        print('  FAIL  ' + text)

    def info(self, text):
        if self.verbose:
            print('        ' + text)

    def section(self, text):
        print('\n' + text)


# ---------------------------------------------------------------------------------------------------------
# processor.ini
# ---------------------------------------------------------------------------------------------------------

def strip_comment(value):
    # a comment needs a blank before it: "*.zip;*.7z" keeps its ';'
    m = re.search(r'[ \t][;#]', value)
    if m:
        value = value[:m.start()]
    if value.startswith(';') or value.startswith('#'):
        return ''
    return value.strip()


def read_ini(path):
    values = {}
    with open(path, encoding='utf-8-sig') as f:
        for line in f:
            line = line.strip()
            if not line or line[0] in ';#' or line.startswith('['):
                continue
            if '=' not in line:
                continue
            key, _, value = line.partition('=')
            values[key.strip().lower()] = strip_comment(value)
    return values


def host_keys():
    system = platform.system()
    machine = platform.machine().lower()
    if system == 'Windows':
        return ['windows-x86_64']
    arch = {'x86_64': 'x86_64', 'amd64': 'x86_64', 'i686': 'i386', 'i386': 'i386', 'aarch64': 'arm64',
            'armv7l': 'armhf', 'armv8l': 'armhf'}.get(machine, machine)
    return ['linux-' + arch]


def resolve_program(folder, values, keys):
    for key in keys:
        exec_value = values.get('exec.' + key) or values.get('exec', '').replace('{key}', key)
        if not exec_value:
            continue
        path = os.path.join(folder, exec_value)
        for candidate in (path, path + '.exe'):
            if os.path.isfile(candidate):
                return key, candidate
    return None, None


def check_ini(folder, report, keys):
    report.section('processor.ini')
    ini = os.path.join(folder, 'processor.ini')
    if not os.path.isfile(ini):
        report.fail('there is no processor.ini in ' + folder)
        return None
    v = read_ini(ini)
    for key in ('name', 'version', 'exec', 'kinds'):
        if not v.get(key):
            (report.fail if key in ('exec', 'kinds') else report.warn)('%s= is missing' % key.capitalize())
    kinds = [k.strip().lower() for k in re.split(r'[,; ]+', v.get('kinds', '')) if k.strip()]
    unknown = [k for k in kinds if k not in KINDS]
    if unknown:
        report.fail('Kinds= has what the launcher does not know: ' + ', '.join(unknown))
    kinds = [k for k in kinds if k in KINDS]
    if kinds:
        report.ok('Kinds: ' + ', '.join(kinds))
    for key in ('order', 'timeout'):
        if v.get(key) and not re.fullmatch(r'-?\d+', v[key]):
            report.fail('%s= is not a number: %s' % (key.capitalize(), v[key]))
    if v.get('modifies') and v['modifies'].lower() not in ('true', 'false', 'yes', 'no', '1', '0'):
        report.fail('Modifies= is not true/false: ' + v['modifies'])
    match = [m.strip() for m in re.split(r'[;,]', v.get('match', '')) if m.strip()]
    if match:
        report.ok('Match: ' + ', '.join(match))
    elif any(k in ('ps1', 'rom') for k in kinds):
        report.warn('no Match=: every game or ROM is a candidate, and --ismine is asked of every one of them')
    key, program = resolve_program(folder, v, keys)
    if not program:
        report.fail('no program for ' + ', '.join(keys) + ' (Exec=%s)' % v.get('exec', ''))
        return None
    if os.name != 'nt' and not os.access(program, os.X_OK):
        report.fail(program + ' is not executable')
        return None
    report.ok('program for %s: %s' % (key, os.path.relpath(program, folder)))
    timeout = int(v['timeout']) if re.fullmatch(r'\d+', v.get('timeout', '')) else 600
    systems = [s.strip() for s in v.get('systems', '').split(';') if s.strip()]
    return {'name': os.path.basename(os.path.normpath(folder)), 'title': v.get('name') or '', 'version':
            v.get('version', ''), 'kinds': kinds, 'match': match, 'systems': systems, 'timeout': timeout,
            'program': program, 'folder': folder, 'key': key}


# ---------------------------------------------------------------------------------------------------------
# running it
# ---------------------------------------------------------------------------------------------------------

class Run:
    def __init__(self):
        self.code = None
        self.lines = []  # (seconds since start, 'out'|'err', text)
        self.stopped = False
        self.longest_silence = 0.0


def run(proc, args, env, stop_after_progress=False, timeout_cap=None):
    """Runs the processor; with stop_after_progress it is stopped after its first stage/percent/counter line."""
    r = Run()
    started = time.time()
    last = [started]
    cmd = [proc['program']] + args
    kwargs = {}
    if os.name == 'nt':
        kwargs['creationflags'] = subprocess.CREATE_NEW_PROCESS_GROUP
    else:
        kwargs['start_new_session'] = True
    p = subprocess.Popen(cmd, cwd=proc['folder'], env=env, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                         stdin=subprocess.DEVNULL, **kwargs)
    progress = threading.Event()

    def reader(stream, name):
        for raw in iter(stream.readline, b''):
            now = time.time()
            r.longest_silence = max(r.longest_silence, now - last[0])
            last[0] = now
            for text in raw.decode('utf-8', 'replace').replace('\r', '\n').split('\n'):
                if text.strip():
                    r.lines.append((now - started, name, text.strip()))
                    if name == 'out' and is_progress(text.strip()):
                        progress.set()

    threads = [threading.Thread(target=reader, args=(p.stdout, 'out'), daemon=True),
               threading.Thread(target=reader, args=(p.stderr, 'err'), daemon=True)]
    for t in threads:
        t.start()
    cap = timeout_cap or (proc['timeout'] + 30 if proc['timeout'] > 0 else 3600)
    while p.poll() is None:
        if stop_after_progress and progress.is_set():
            r.stopped = True
            stop(p)
            break
        if time.time() - last[0] > cap:
            r.stopped = True
            stop(p)
            break
        time.sleep(0.02)
    p.wait()
    for t in threads:
        t.join(2)
    r.code = p.returncode
    r.longest_silence = max(r.longest_silence, time.time() - last[0]) if not r.stopped else r.longest_silence
    return r


def stop(p):
    # what the launcher does: SIGTERM to the group, 3 s, SIGKILL (a hard stop on Windows)
    try:
        if os.name == 'nt':
            p.kill()
        else:
            os.killpg(p.pid, signal.SIGTERM)
            try:
                p.wait(3)
            except subprocess.TimeoutExpired:
                os.killpg(p.pid, signal.SIGKILL)
    except (ProcessLookupError, PermissionError):
        pass


def is_progress(line):
    if line.startswith('#'):
        body = line[1:].strip().upper()
        return bool(body) and not re.match(r'(STARTING|DONE|ERROR|WARN)\b', body)
    return bool(re.fullmatch(r'\d{1,3}', line) or re.fullmatch(r'\d+\s*/\s*\d+', line))


def check_output(r, report, what, expect_quiet=False):
    """The protocol rules over one run's stdout; returns 'done', 'error' or None (neither)."""
    out = [t for (_, s, t) in r.lines if s == 'out']
    if not out or not re.match(r'#\s*Starting\b', out[0], re.I):
        report.fail('%s: the first line is not "#Starting - ..." (%r)' % (what, out[0] if out else ''))
    chatter = []
    end = None
    active = False
    for line in out:
        if end:
            report.warn('%s: output after the %s line: %r' % (what, end, line))
            break
        if line.startswith('#'):
            body = line[1:].strip()
            upper = body.upper()
            if re.match(r'DONE\b', upper):
                end = 'done'
            elif re.match(r'ERROR\b', upper):
                end = 'error'
            elif re.match(r'(STARTING|WARN)\b', upper):
                pass
            elif body:
                active = True
        elif re.fullmatch(r'\d{1,3}', line):
            if int(line) > 100:
                report.warn('%s: a percent above 100: %s' % (what, line))
            active = True
        elif re.fullmatch(r'\d+\s*/\s*\d+', line):
            active = True
        else:
            chatter.append(line)
    if chatter:
        report.warn('%s: %d line(s) that are not protocol (logged, ignored), e.g. %r' % (what, len(chatter),
                                                                                      chatter[0]))
    if end == 'done' and r.code != 0:
        report.fail('%s: said #DONE but exited with %d' % (what, r.code))
    elif end == 'error' and r.code == 0:
        report.fail('%s: said #ERROR but exited with 0' % what)
    elif end is None and not r.stopped:
        report.fail('%s: ended (exit %s) without #DONE or #ERROR' % (what, r.code))
    if expect_quiet and active:
        report.fail('%s: run again on its own output it still had work to do (not idempotent)' % what)
    if end == 'error':
        message = [t for t in out if t.upper().startswith('#ERROR')]
        report.warn('%s: #ERROR - %s' % (what, message[0][6:].strip(' -:') if message else ''))
    return end


def check_silence(r, proc, report, what):
    if proc['timeout'] > 0 and r.longest_silence > proc['timeout']:
        report.fail('%s: silent for %.0f s, longer than Timeout=%d - the launcher would have killed it'
                    % (what, r.longest_silence, proc['timeout']))
    elif r.lines:
        report.info('%s: %.1f s, the longest silence %.1f s' % (what, r.lines[-1][0], r.longest_silence))


# ---------------------------------------------------------------------------------------------------------
# the trees
# ---------------------------------------------------------------------------------------------------------

def snapshot(root):
    """relative path -> (size, mtime_ns) for every file, and every directory as (-1, 0)."""
    snap = {}
    for dirpath, dirnames, filenames in os.walk(root):
        rel_dir = os.path.relpath(dirpath, root)
        for d in dirnames:
            snap[os.path.normpath(os.path.join(rel_dir, d))] = (-1, 0)
        for f in filenames:
            st = os.stat(os.path.join(dirpath, f))
            snap[os.path.normpath(os.path.join(rel_dir, f))] = (st.st_size, st.st_mtime_ns)
    return snap


def changes(before, after):
    out = []
    for k in sorted(set(before) | set(after)):
        if k not in before:
            out.append('+ ' + k)
        elif k not in after:
            out.append('- ' + k)
        elif before[k] != after[k]:
            out.append('~ ' + k)
    return out


def part_files(root):
    return [os.path.relpath(os.path.join(d, f), root) for d, _, fs in os.walk(root) for f in fs
            if f.lower().endswith('.part')]


def game_folders(games):
    """Every folder under Games/ with files of its own, as the scan hands them to a ps1 item chain."""
    out = []
    for dirpath, dirnames, filenames in os.walk(games):
        dirnames[:] = [d for d in dirnames if not d.startswith(('.', '!'))]
        own = [f for f in filenames if not f.startswith('.') and f.lower() not in IGNORED
               and not f.lower().endswith(('.part', '.m3u'))]
        if own and os.path.normpath(dirpath) != os.path.normpath(games):
            out.append(dirpath)
    return out


def rom_files(roms):
    out = []
    for system in sorted(os.listdir(roms)):
        sdir = os.path.join(roms, system)
        if not os.path.isdir(sdir) or system.startswith('.'):
            continue
        for dirpath, _, filenames in os.walk(sdir):
            for f in sorted(filenames):
                if not f.startswith('.') and not f.lower().endswith('.part'):
                    out.append((os.path.join(dirpath, f), system))
    return out


def matches(proc, name):
    import fnmatch
    return not proc['match'] or any(fnmatch.fnmatch(name.lower(), m.lower()) for m in proc['match'])


# ---------------------------------------------------------------------------------------------------------
# the checks per kind
# ---------------------------------------------------------------------------------------------------------

def environment(proc, root, games, roms, tmp):
    env = dict(os.environ)
    env.update({'AB_PROCESSOR_PROTOCOL': '1', 'AB_PROCESSOR_NAME': proc['name'], 'AB_ROOT': root,
                'AB_GAMES_DIR': games, 'AB_ROMS_DIR': roms, 'AB_RDB_DIR': os.path.join(root, 'rdb'),
                'AB_TMP': tmp, 'AB_PLATFORM': 'proc_check', 'AB_PLATFORM_KEYS': proc['key'],
                'AB_LANGUAGE': 'English', 'AB_VERSION': 'proc_check'})
    return env


def check_target(proc, report, args, what, env, root, target, allowed_dir):
    """--start on one target: the protocol, the leftovers, what changed where, idempotence, a stop."""
    before = snapshot(root)
    r = run(proc, ['--start'] + args, env)
    end = check_output(r, report, what)
    check_silence(r, proc, report, what)
    left = part_files(root)
    if left:
        report.fail('%s: *.part left behind: %s' % (what, ', '.join(left[:3])))
    outside = [c for c in changes(before, snapshot(root))
               if not os.path.normpath(os.path.join(root, c[2:])).startswith(os.path.normpath(allowed_dir))]
    if outside:
        report.fail('%s: changed something outside its target: %s' % (what, ', '.join(outside[:3])))
    if end != 'done' or r.code != 0:
        return end
    report.ok('%s: #DONE, exit 0' % what)

    # idempotent: again, on what it left
    if os.path.exists(target):
        again_before = snapshot(root)
        r2 = run(proc, ['--start'] + args, env)
        check_output(r2, report, what + ' (again)', expect_quiet=True)
        moved = changes(again_before, snapshot(root))
        if moved:
            report.fail('%s (again): changed its own output: %s' % (what, ', '.join(moved[:3])))
        elif r2.code == 0:
            report.ok('%s: a second run finds nothing to do' % what)
    return end


def check_interrupted(proc, report, args, what, env, root, fresh_copy):
    """Stopped after its first progress line, then started again: it must finish and leave no *.part."""
    fresh_copy()
    r = run(proc, ['--start'] + args, env, stop_after_progress=True)
    if not r.stopped:
        report.info('%s: finished before it could be stopped - nothing to learn about atomicity' % what)
        return
    r2 = run(proc, ['--start'] + args, env)
    end = check_output(r2, report, what + ' (after a stop)')
    left = part_files(root)
    if left:
        report.fail('%s: after a stop and a restart, *.part left: %s' % (what, ', '.join(left[:3])))
    elif end == 'done':
        report.ok('%s: stopped in the middle and started again, it finished cleanly' % what)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('processor', help='the processor folder (processor.ini + bin/<key>/...)')
    ap.add_argument('--games', help='a sample Games tree (copied, never touched)')
    ap.add_argument('--roms', help='a sample roms tree: roms/<system>/<files> (copied, never touched)')
    ap.add_argument('--key', action='append', help='the platform key(s) to run (default: this machine)')
    ap.add_argument('--no-interrupt', action='store_true', help='skip the stop-and-restart check')
    ap.add_argument('-v', '--verbose', action='store_true')
    a = ap.parse_args()

    report = Report(a.verbose)
    keys = a.key or host_keys()
    proc = check_ini(os.path.abspath(a.processor), report, keys)
    if not proc:
        print('\nFAILED: the processor could not be checked any further')
        return 1

    report.section('--version')
    r = run(proc, ['--version'], dict(os.environ), timeout_cap=30)
    out = [t for (_, s, t) in r.lines if s == 'out']
    if r.code != 0:
        report.fail('--version exited with %s' % r.code)
    elif len(out) != 1 or not out[0].startswith('#'):
        report.fail('--version should print one "#Name Vx.y - description" line, printed %r' % out)
    else:
        report.ok(out[0])
        if proc['version'] and proc['version'] not in out[0]:
            report.warn('--version does not mention Version=%s' % proc['version'])

    if not a.games and not a.roms:
        report.warn('no sample tree given (--games / --roms): only processor.ini and --version were checked')

    scratch = tempfile.mkdtemp(prefix='proc_check_')
    try:
        root = os.path.join(scratch, 'stick')
        games = os.path.join(root, 'Games')
        roms = os.path.join(root, 'roms')
        tmp = os.path.join(scratch, 'abtmp')

        def fresh():
            shutil.rmtree(root, ignore_errors=True)
            os.makedirs(root)
            shutil.copytree(a.games, games) if a.games else os.makedirs(games)
            shutil.copytree(a.roms, roms) if a.roms else os.makedirs(roms)
            shutil.rmtree(tmp, ignore_errors=True)
            os.makedirs(tmp)

        env = environment(proc, root, games, roms, tmp)
        for kind in proc['kinds']:
            if kind == 'games-folder' and a.games:
                report.section('games-folder: --start --games')
                fresh()
                check_target(proc, report, ['--games', games], 'Games/', env, root, games, games)
                if not a.no_interrupt:
                    check_interrupted(proc, report, ['--games', games], 'Games/', env, root, fresh)
            elif kind == 'roms-folder' and a.roms:
                report.section('roms-folder: --start --roms')
                fresh()
                check_target(proc, report, ['--roms', roms], 'roms/', env, root, roms, roms)
                if not a.no_interrupt:
                    check_interrupted(proc, report, ['--roms', roms], 'roms/', env, root, fresh)
            elif kind == 'ps1' and a.games:
                report.section('ps1: every game folder')
                fresh()
                folders = [f for f in game_folders(games)
                           if any(matches(proc, n) for n in os.listdir(f) if os.path.isfile(os.path.join(f, n)))]
                if not folders:
                    report.warn('no game folder in the sample matches Match=')
                for folder in folders:
                    what = os.path.relpath(folder, root)
                    code = run(proc, ['--ismine', '--ps1', folder], env, timeout_cap=60).code
                    if code not in (0, 1):
                        report.fail('%s: --ismine answered %s (0 = mine, 1 = not mine)' % (what, code))
                        continue
                    report.info('%s: --ismine %s' % (what, 'mine' if code == 0 else 'not mine'))
                    if code == 0:
                        check_target(proc, report, ['--ps1', folder], what, env, root, folder, folder)
            elif kind == 'rom' and a.roms:
                report.section('rom: every ROM file')
                fresh()
                files = [(f, s) for (f, s) in rom_files(roms) if matches(proc, os.path.basename(f))
                         and (not proc['systems'] or s.lower() in [x.lower() for x in proc['systems']])]
                if not files:
                    report.warn('no ROM in the sample matches Match=/Systems=')
                for path, system in files:
                    what = os.path.relpath(path, root)
                    args = ['--rom', path, '--system', system]
                    code = run(proc, ['--ismine'] + args, env, timeout_cap=60).code
                    if code not in (0, 1):
                        report.fail('%s: --ismine answered %s (0 = mine, 1 = not mine)' % (what, code))
                        continue
                    report.info('%s: --ismine %s' % (what, 'mine' if code == 0 else 'not mine'))
                    if code == 0:
                        # a ROM processor may write next to the file: its folder is the target
                        check_target(proc, report, args, what, env, root, path, os.path.dirname(path))
            elif kind in ('games-folder', 'ps1') and not a.games:
                report.warn('%s is in Kinds but no --games sample was given' % kind)
            elif kind in ('roms-folder', 'rom') and not a.roms:
                report.warn('%s is in Kinds but no --roms sample was given' % kind)
    finally:
        shutil.rmtree(scratch, ignore_errors=True)

    print('\n%s: %d failure(s), %d warning(s)' % ('FAILED' if report.failed else 'PASSED', report.failed,
                                                  report.warned))
    return 1 if report.failed else 0


if __name__ == '__main__':
    sys.exit(main())
