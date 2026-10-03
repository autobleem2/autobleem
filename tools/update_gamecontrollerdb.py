#!/usr/bin/env python3
"""
Refresh src/resources/gamecontrollerdb.txt from the community SDL_GameControllerDB
(https://github.com/mdqinc/SDL_GameControllerDB, zlib licence), keeping what AutoBleem added.

    python tools/update_gamecontrollerdb.py                # the commit pinned below
    python tools/update_gamecontrollerdb.py --commit <sha> # another one (then update PINNED_COMMIT)
    python tools/update_gamecontrollerdb.py --check        # say what would change, write nothing

The file is read by every launcher build, and the oldest SDL among them is the console's 2.0.14, so each line
is made safe for it:
  - only the platforms we build for (Linux, Windows, Mac OS X);
  - elements 2.0.14 does not know (misc2..misc6, crc:) are dropped from the line - 2.0.14 would skip them
    with an "Unexpected controller element" error and keep the rest, but the log need not say so each start;
  - a GUID carrying newer SDL's CRC16 in bytes 2-3 can never match on 2.0.14 and is left out.
What stays ours, in this order after the upstream part (SDL lets a later line for the same GUID and platform
replace an earlier one, so these win):
  - lines of the current file that upstream no longer has (e.g. from the 2020 snapshot) - "Kept";
  - the local sections of the current file, verbatim: everything from the first "#Magnus RC" or
    "#AutoBleem" comment line to the end (the PS Classic controller's tuned mapping is there).
The licence travels as src/resources/gamecontrollerdb.LICENSE (THIRD_PARTY_NOTICES.md lists it).
"""
import argparse
import datetime
import os
import re
import sys
import urllib.request

PINNED_COMMIT = 'c6d6e7ecca57ff106ef63350da3cc03728d88a5f'  # 2026-09-25
REPO = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
DB = os.path.join(REPO, 'src', 'resources', 'gamecontrollerdb.txt')
LICENCE = os.path.join(REPO, 'src', 'resources', 'gamecontrollerdb.LICENSE')
RAW = 'https://raw.githubusercontent.com/mdqinc/SDL_GameControllerDB/{}/{}'
PLATFORMS = ('Linux', 'Windows', 'Mac OS X')
LOCAL_SECTION = re.compile(r'^#\s*(Magnus RC|AutoBleem)\b')

# what SDL 2.0.14's SDL_gamecontroller.c knows (map_StringForControllerButton/Axis), plus the fields
SDL2014_ELEMENTS = {
    'a', 'b', 'x', 'y', 'back', 'guide', 'start', 'leftstick', 'rightstick', 'leftshoulder', 'rightshoulder',
    'dpup', 'dpdown', 'dpleft', 'dpright', 'misc1', 'paddle1', 'paddle2', 'paddle3', 'paddle4', 'touchpad',
    'leftx', 'lefty', 'rightx', 'righty', 'lefttrigger', 'righttrigger', 'platform', 'hint', 'sdk>=', 'sdk<=',
}


def fetch(commit, name):
    with urllib.request.urlopen(RAW.format(commit, name), timeout=60) as r:
        return r.read().decode('utf-8')


def key(line):
    m = re.search(r'platform:([^,]+)', line)
    return line.split(',', 1)[0].lower(), (m.group(1) if m else '')


def for_sdl2014(line):
    """the line cleaned for SDL 2.0.14, or None when it cannot apply there"""
    fields = line.rstrip(',').split(',')
    guid = fields[0]
    if len(guid) != 32 or guid[4:8] != '0000':
        return None  # newer SDL's CRC16 in the GUID
    out = fields[:2]
    for f in fields[2:]:
        name = f.split(':', 1)[0]
        if name in SDL2014_ELEMENTS or name.startswith('sdk'):
            out.append(f)
    return ','.join(out) + ','


def split_local(text):
    """(upstream-part lines, local-section text) of the current file"""
    lines = text.splitlines()
    for i, l in enumerate(lines):
        if LOCAL_SECTION.match(l):
            return lines[:i], '\n'.join(lines[i:]).rstrip() + '\n'
    return lines, ''


def mappings(lines):
    return [l.strip() for l in lines if l.strip() and not l.lstrip().startswith('#')]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--commit', default=PINNED_COMMIT)
    ap.add_argument('--check', action='store_true')
    a = ap.parse_args()

    current = open(DB, encoding='utf-8').read()
    ours_upstream_part, local = split_local(current)

    upstream = []
    dropped = 0
    for l in mappings(fetch(a.commit, 'gamecontrollerdb.txt').splitlines()):
        if key(l)[1] not in PLATFORMS:
            continue
        c = for_sdl2014(l)
        if c is None:
            dropped += 1
            continue
        upstream.append(c)
    have = {key(l) for l in upstream}
    local_keys = {key(l) for l in mappings(local.splitlines())}
    kept = [l if l.endswith(',') else l + ',' for l in mappings(ours_upstream_part)
            if key(l)[1] in PLATFORMS and key(l) not in have and key(l) not in local_keys]

    per = {p: sum(1 for l in upstream if key(l)[1] == p) for p in PLATFORMS}
    print(f'upstream {a.commit[:12]}: ' + ', '.join(f'{p} {n}' for p, n in per.items()) +
          f' ({dropped} newer-SDL GUIDs left out); kept from the current file: {len(kept)}; '
          f'local sections: {len(mappings(local.splitlines()))} lines')
    if a.check:
        return 0

    out = [
        '# Game Controller DB for SDL - AutoBleem\'s copy, made by tools/update_gamecontrollerdb.py',
        f'# Source: https://github.com/mdqinc/SDL_GameControllerDB at {a.commit}'
        f' ({datetime.date.today().isoformat()}), zlib licence (gamecontrollerdb.LICENSE)',
        '# Cleaned for SDL 2.0.14 (the console\'s): platforms ' + ', '.join(PLATFORMS) + ' only; elements it does not',
        '# know dropped. Later lines win for the same GUID and platform, so the local sections at the end do.',
        '',
    ]
    for p in PLATFORMS:
        out.append(f'# {p}')
        out += sorted((l for l in upstream if key(l)[1] == p), key=lambda l: (l.split(',')[1].lower(), l))
        out.append('')
    if kept:
        out.append('# Kept: lines of the previous AutoBleem copy that upstream does not have')
        out += kept
        out.append('')
    text = '\n'.join(out) + '\n' + local
    with open(DB, 'w', encoding='utf-8', newline='\n') as f:
        f.write(text)
    with open(LICENCE, 'w', encoding='utf-8', newline='\n') as f:
        f.write(fetch(a.commit, 'LICENSE'))
    print(f'wrote {os.path.relpath(DB, REPO)} ({len(mappings(text.splitlines()))} mappings) and the licence')
    return 0


if __name__ == '__main__':
    sys.exit(main())
