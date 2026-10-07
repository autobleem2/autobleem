#!/usr/bin/env python3
"""vm_testdata - the prepared, repeatable test data set for the pcusb-test VM's stick (PLATFORM-14).

  python tools/vm/vm_testdata.py build DIR              lay the data set into DIR (a stick root; a local dry run)
  python tools/vm/vm_testdata.py lay --who NAME         lay it on the VM stick (needs NAME's VM lease: abvm.py lock take)
  python tools/vm/vm_testdata.py check --who NAME       list what of the data set is missing on the stick (read-only)

Options: --cache DIR (where the free downloads are kept; default <temp>/ab-testdata-cache), --no-restart (lay: leave the
launcher as it is; by default it is restarted and made to Re-scan games, so the new folders show).

Everything is generated or freely redistributable - no copyrighted content. Idempotent: `lay` unpacks with
--skip-old-files, so a file that is already on the stick (the app's own Game.ini / pcsx.cfg / covers written by the
scan, a save made on the stick, a card the tester played) is never overwritten and nothing is duplicated; run it again
after any reinstall. It writes only the data-set folders (Games/, RetroArch/roms/<system>/, Apps/<app>/), never
config.ini, System/, Themes/, Extensions/, the launcher, or Tetrade (the installer's sample game and its saves).

What is laid (names are fixed, so a second run finds them):
  Games/                 8 generated PS1 games over the sub-folders (make_usb.py's fake games: a bin/cue whose ISO holds a
                         SLUS_nnn.nn file, serials SLUS-01234 ..; the cover DB knows those serials, so the scan shows real
                         titles and covers for most of them - the only part of the set that looks like a real library), one
                         multi-disc game "Fake Chronicles" (two discs in one folder, serials SLUS-99901/02, which no DB
                         knows - the title stays), one game the scan refuses ("Broken Disc": its Game.ini names a disc
                         with no cue -> Game Manager "Not added")
  Games/!MemCards/       three card sets (Fighting games, RPG saves, Kids); the first two hold generated saves
                         (title, product code, a small icon), "Kids" is blank
  Games/!SaveStates/     resume slots for three generated games (Crash Dummies 3 slots, Spyro the Fake 1, Fake Chronicles 1):
                         the launcher's slot files and a generated picture each; the state files are placeholders - they
                         fill the Resume screen but cannot be loaded (the games are not real discs)
  RetroArch/roms/        the four free homebrew ROMs the installer lays too (sources and licences below)
  Apps/                  Terminal and SDLPoP, the two free Apps' pcusb packages from the autobleem2 releases

Free content (downloaded from the official pages, sha256 pinned below):
  Nova the Squirrel (NES)       NovaSquirrel, GPL-3.0     https://github.com/NovaSquirrel/NovaTheSquirrel
  Asteroids (SNES)              undisbeliever, MIT        https://github.com/undisbeliever/asteroids
  Castle Platformer (SNES)      undisbeliever, MIT        https://github.com/undisbeliever/castle_platformer
  Alex vs Bus - The Race (MD)   M374LX, GPL-3.0 code and CC BY-SA 4.0 assets   https://github.com/M374LX/alexvsbus-md
  Terminal 1.0.0 (App)          AutoBleem, GPL-3.0-or-later (font DejaVu Sans Mono, free licence)
                                https://github.com/autobleem2/app_terminal
  SDLPoP 1.24-RC-1 (App)        the SDLPoP authors, GPL-3.0   https://github.com/autobleem2/app_sdlpop
The generated games, cards, pictures and states are made by this script and carry no third-party content.
"""
import argparse
import hashlib
import io
import os
import shutil
import struct
import subprocess
import sys
import tarfile
import tempfile
import urllib.request
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.dirname(HERE)
REPO = os.path.dirname(TOOLS)
sys.path.insert(0, TOOLS)
import make_usb  # noqa: E402  (the fake games' ISO and cue writers)

ABVM = os.path.join(HERE, 'abvm.py')
STICK = '/media/autobleem'
GUEST_TAR = '/tmp/platform14-data.tar'

# -------------------------------------------------------------------------------------------------------------------
# the free downloads: (url, sha256 of the download)
# -------------------------------------------------------------------------------------------------------------------
GH = 'https://github.com/'
DOWNLOADS = {
    'nova': (GH + 'NovaSquirrel/NovaTheSquirrel/releases/download/v1.0.6a/nova.nes',
             'e4780e90b9d1587489bfb797d2ca395be21371ea9262fa9f87f99324ec6960ab'),
    'asteroids': (GH + 'undisbeliever/asteroids/releases/download/v1.03/Asteroids.v1.03.zip',
                  '3b8fde888ce802900e66bd89cb7bdbb0444691111f8a8ea7eb0c887fd2ca62dc'),
    'castle': (GH + 'undisbeliever/castle_platformer/releases/download/v1.04/Castle.Platformer.v1.04.zip',
               'e75da7af6f3a1bd10c21cf4324d0c4321271ce3b9ef3408b2dc2db9cb05f9159'),
    'alexvsbus': (GH + 'M374LX/alexvsbus-md/releases/download/pre3/alexvsbus-md-pre3.zip',
                  'eea48b73298b7d38e69557cba291eaf05cf20f637d8faeaa032a53de2c562710'),
    'terminal': (GH + 'autobleem2/app_terminal/releases/download/v1.0.0/terminal-pcusb-1.0.0.zip',
                 '9df9475f5e8edf182940dbb5b55d274c7bc0380c83e9de31410451a56fb25a2e'),
    'sdlpop': (GH + 'autobleem2/app_sdlpop/releases/download/v1.24-RC-1/sdlpop-pcusb-1.24-RC-1.zip',
               'de23af47144a85a4c3c7cc58fefb3bf720b0d59cfef4486d1e92874c7042d4ec'),
}

# (download, member of the zip or None for a plain file, the ROM's path under RetroArch/roms/)
ROMS = [
    ('nova', None, 'Nintendo - Nintendo Entertainment System/Nova the Squirrel.nes'),
    ('asteroids', 'Asteroids (v1.03)/Asteroids (v1.03).sfc',
     'Nintendo - Super Nintendo Entertainment System/Asteroids.sfc'),
    ('castle', 'Castle Platformer (v1.04)/Castle Platformer (v1.04).sfc',
     'Nintendo - Super Nintendo Entertainment System/Castle Platformer.sfc'),
    ('alexvsbus', 'alexvsbus-md-pre3/alexvsbus-pre3.md', 'Sega - Mega Drive - Genesis/Alex vs Bus - The Race.md'),
]
APPS = ['terminal', 'sdlpop']   # zips that hold Apps/<name>/ and unpack at the stick's root

GENERATED_GAMES = 8


def fetch(name, cache):
    url, sha = DOWNLOADS[name]
    os.makedirs(cache, exist_ok=True)
    path = os.path.join(cache, os.path.basename(url))
    if os.path.exists(path) and sha256_of(path) == sha:
        return path
    print('downloading', url)
    req = urllib.request.Request(url, headers={'User-Agent': 'vm_testdata'})
    with urllib.request.urlopen(req, timeout=120) as r, open(path + '.part', 'wb') as f:
        shutil.copyfileobj(r, f)
    if sha256_of(path + '.part') != sha:
        os.remove(path + '.part')
        raise SystemExit('sha256 of %s differs from the pinned one - the release changed' % url)
    os.replace(path + '.part', path)
    return path


def sha256_of(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for block in iter(lambda: f.read(1 << 20), b''):
            h.update(block)
    return h.hexdigest()


def stamp(path, when):
    os.utime(path, (when, when))


def write(path, data, when=None):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    mode = 'wb' if isinstance(data, bytes) else 'w'
    kwargs = {} if mode == 'wb' else {'newline': '\n'}
    with open(path, mode, **kwargs) as f:
        f.write(data)
    if when:
        stamp(path, when)


# -------------------------------------------------------------------------------------------------------------------
# generated games: single, multi-disc, refused
# -------------------------------------------------------------------------------------------------------------------
def write_disc(folder, name, serial_file, title):
    """<name>.bin (a generated ISO holding serial_file) and <name>.cue next to it"""
    write(os.path.join(folder, name + '.bin'), make_usb.make_iso(serial_file=serial_file, title=title))
    write(os.path.join(folder, name + '.cue'),
          'FILE "%s.bin" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n' % name)


MULTI = ('Fake Chronicles', ['SLUS_999.01', 'SLUS_999.02'])
REFUSED = 'Broken Disc'


def lay_games(root):
    games = os.path.join(root, 'Games')
    make_usb.make_fake_games(games, GENERATED_GAMES)
    # the multi-disc game: one folder, one cue/bin per disc ("(Disc n)"), each with its own serial
    folder = os.path.join(games, 'RPG', MULTI[0])
    for n, serial in enumerate(MULTI[1], 1):
        write_disc(folder, '%s (Disc %d)' % (MULTI[0], n), serial, MULTI[0])
    # the refused game: a bin whose Game.ini (automation off, as a hand-edited one) names a disc with no cue file ->
    # the scan lists the folder in the Game Manager as "Not added" ("Cue file not found", "Bin file failed to verify").
    # (A cue naming a missing bin is not refused: the scanner repairs cues, and a lone cue is no game file.)
    folder = os.path.join(games, REFUSED)
    write(os.path.join(folder, REFUSED + '.bin'), make_usb.make_iso(serial_file='SLUS_999.99', title=REFUSED))
    write(os.path.join(folder, 'Game.ini'),
          '[Game]\nAutomation=0\nDiscs=Ghost Disc\nImagetype=0\nMemcard=SONY\nPlayers=1\nPublisher=Nobody\n'
          'Title=%s\nYear=2025\n' % REFUSED)


# -------------------------------------------------------------------------------------------------------------------
# memory cards: the formatted blank card of the launcher plus generated saves (ps1 card layout: 15 blocks of 0x2000,
# a directory frame per block after the header frame)
# -------------------------------------------------------------------------------------------------------------------
FRAME = 0x80
BLOCK = 0x2000


def checksum_frame(card, pos):
    x = 0
    for i in range(126):
        x ^= card[pos + i]
    card[pos + 127] = x


def card_save(card, slots, title, product_code, game_id, colors):
    """a save over `slots` (the first is the top block): title, product code, game id and a 16x16 icon of two colours"""
    for i, slot in enumerate(slots):
        pos = FRAME + slot * FRAME
        last = i + 1 == len(slots)
        card[pos:pos + FRAME] = bytes(FRAME)
        card[pos] = 0x51 if i == 0 else (0x53 if last else 0x52)
        size = len(slots) * BLOCK
        card[pos + 4:pos + 7] = struct.pack('<I', size)[:3]
        card[pos + 8] = 0xFF if last else slots[i + 1]
        card[pos + 9] = 0xFF if last else 0x00
        if i == 0:   # the file name at 0x0A: region + product code ("BA" + "SLUS-01235"), then the game id
            name = (product_code + game_id).encode('ascii')
            card[pos + 10:pos + 10 + len(name)] = name
        checksum_frame(card, pos)
    b = BLOCK + slots[0] * BLOCK
    card[b:b + BLOCK] = bytes(BLOCK)
    card[b] = ord('S')
    card[b + 1] = ord('C')
    card[b + 2] = 0x11                     # one icon frame
    card[b + 3] = len(slots)
    card[b + 4:b + 4 + len(title)] = title.encode('ascii')
    for n, (r, g, bl) in enumerate(colors):  # palette: 15-bit BGR, entry 0 is transparent
        value = (bl >> 3) << 10 | (g >> 3) << 5 | (r >> 3)
        card[b + 0x60 + 2 * (n + 1):b + 0x60 + 2 * (n + 2)] = struct.pack('<H', value | 0x8000)
    for y in range(16):                    # a diamond: colour 1 inside, colour 2 outside
        for x in range(0, 16, 2):
            px = [1 if abs(x + k - 7.5) + abs(y - 7.5) <= 7 else 2 for k in (0, 1)]
            card[b + 0x80 + y * 8 + x // 2] = px[0] | px[1] << 4


def blank_card(template):
    with open(template, 'rb') as f:
        return bytearray(f.read())


def lay_memcards(root):
    template = os.path.join(REPO, 'src', 'resources', 'memcard')
    sets = {
        'Fighting games': [[([0], 'TEKKEN FAUX - ARCADE', 'BASLUS-01237', 'FAKE00', [(255, 80, 40), (40, 40, 40)]),
                            ([1], 'TEKKEN FAUX - SURVIVAL', 'BASLUS-01237', 'FAKE01', [(255, 200, 40), (40, 40, 40)])],
                           []],
        'RPG saves': [[([0, 1], 'FINAL FAKESY - CH 3', 'BASLUS-01238', 'FAKE00', [(60, 120, 255), (20, 20, 60)]),
                       ([2], 'SUIKODEN FAUX - CASTLE', 'BASLUS-01239', 'FAKE00', [(60, 220, 120), (20, 60, 20)]),
                       ([3, 4, 5], 'CHRONO FAUX - END', 'BASLUS-01240', 'FAKE00', [(200, 80, 220), (50, 20, 60)])],
                      [([0], 'FAKE FANTASY - SLOT B', 'BASLUS-01241', 'FAKE00', [(255, 140, 60), (60, 30, 10)])]],
        'Kids': [[], []],
    }
    for name, cards in sets.items():
        folder = os.path.join(root, 'Games', '!MemCards', name)
        for card_name, saves in zip(('card1.mcd', 'card2.mcd'), cards):
            card = blank_card(os.path.join(template, card_name))
            for slots, title, code, game_id, colors in saves:
                card_save(card, slots, title, code, game_id, colors)
            write(os.path.join(folder, card_name), bytes(card), 1759300000)
    # one generated game with its own card holding saves (the game menu's Memory Card Manager)
    internal = os.path.join(root, 'Games', '!SaveStates', 'Crash Dummies', 'memcards', 'card1.mcd')
    card = blank_card(os.path.join(template, 'card1.mcd'))
    card_save(card, [0], 'CRASH DUMMIES - LEVEL 4', 'BASLUS-01235', 'FAKE00', [(240, 220, 60), (60, 50, 10)])
    write(internal, bytes(card), 1759300000)


# -------------------------------------------------------------------------------------------------------------------
# resume slots: what PCSX leaves in !SaveStates/<game>/ (core/services/resume_point.cpp reads them): a kept filename
# file naming the state, the kept state file and the kept screenshot, per slot
# -------------------------------------------------------------------------------------------------------------------
def png(width, height, pixel):
    raw = b''.join(b'\x00' + b''.join(bytes(pixel(x, y)) for x in range(width)) for y in range(height))

    def chunk(kind, data):
        body = kind + data
        return struct.pack('>I', len(data)) + body + struct.pack('>I', zlib.crc32(body) & 0xFFFFFFFF)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 2, 0, 0, 0)) +
            chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


def picture(seed):
    """a 160x120 generated 'screenshot': a gradient with a coloured block, different per seed"""
    base = [(40, 90, 200), (200, 80, 60), (60, 170, 90), (190, 160, 50)][seed % 4]

    def pixel(x, y):
        if 40 <= x < 120 and 30 <= y < 90:
            return base
        return (x * 255 // 160 // 3 + 20, y * 255 // 120 // 3 + 20, 60 + seed * 20 % 120)
    return png(160, 120, pixel)


SLOT_BASE = 1759400000   # 2025-10-02 - the slot files' fixed times, so the newest slot is always the same one


def lay_states(root):
    plan = [('Crash Dummies', 'SLUS-01235', 3), ('Spyro the Fake', 'SLUS-01236', 1), (MULTI[0], 'SLUS-99901', 1)]
    for game, name, slots in plan:
        folder = os.path.join(root, 'Games', '!SaveStates', game)
        cue = '/media/Games/%s/%s.cue' % (game, game)
        for slot in range(slots):
            when = SLOT_BASE + slot * 7200
            filename = cue + '\n' + name + '\n'
            if slot == 0:
                write(os.path.join(folder, 'filename.txt.res'), filename, when)
                write(os.path.join(folder, 'screenshots', name + '.png.res'), picture(slot + len(game)), when)
            else:
                write(os.path.join(folder, 'filename.%d.txt.res' % slot), filename, when)
                write(os.path.join(folder, 'screenshots', '%s.%d.png.res' % (name, slot)),
                      picture(slot + len(game)), when)
            write(os.path.join(folder, 'sstates', '%s.00%d.res' % (name, slot)),
                  b'placeholder state of a generated game (not loadable)\n', when)


# -------------------------------------------------------------------------------------------------------------------
# free content
# -------------------------------------------------------------------------------------------------------------------
def lay_roms(root, cache):
    import zipfile
    for name, member, dest in ROMS:
        path = fetch(name, cache)
        target = os.path.join(root, 'RetroArch', 'roms', dest)
        os.makedirs(os.path.dirname(target), exist_ok=True)
        if member is None:
            shutil.copyfile(path, target)
        else:
            with zipfile.ZipFile(path) as z, open(target, 'wb') as out:
                out.write(z.read(member))


def lay_apps(root, cache):
    import zipfile
    for name in APPS:
        with zipfile.ZipFile(fetch(name, cache)) as z:
            for info in z.infolist():
                if info.is_dir():
                    continue
                target = os.path.join(root, *info.filename.split('/'))
                os.makedirs(os.path.dirname(target), exist_ok=True)
                with open(target, 'wb') as out:
                    out.write(z.read(info))


def build(root, cache):
    lay_games(root)
    lay_memcards(root)
    lay_states(root)
    lay_roms(root, cache)
    lay_apps(root, cache)


# what `check` looks for on the stick: one marker file per piece of the data set
def markers():
    out = []
    for i in range(GENERATED_GAMES):
        title = make_usb.FAKE_TITLES[i % len(make_usb.FAKE_TITLES)]
        sub = make_usb.FAKE_FOLDERS[i % len(make_usb.FAKE_FOLDERS)]
        out.append('Games/%s%s/%s.cue' % (sub + '/' if sub else '', title, title))
    out += ['Games/RPG/%s/%s (Disc %d).cue' % (MULTI[0], MULTI[0], n) for n in (1, 2)]
    out.append('Games/%s/Game.ini' % REFUSED)
    out += ['Games/!MemCards/%s/card1.mcd' % n for n in ('Fighting games', 'RPG saves', 'Kids')]
    out += ['Games/!SaveStates/%s/filename.txt.res' % g for g in ('Crash Dummies', 'Spyro the Fake', MULTI[0])]
    out += ['RetroArch/roms/' + dest for _, _, dest in ROMS]
    out += ['Apps/terminal/app.ini', 'Apps/sdlpop/app.ini']
    return out


# -------------------------------------------------------------------------------------------------------------------
# the VM
# -------------------------------------------------------------------------------------------------------------------
def abvm(who, *args):
    cmd = [sys.executable, ABVM, '--who', who] + list(args)
    env = dict(os.environ, MSYS_NO_PATHCONV='1')
    return subprocess.run(cmd, env=env, check=False, capture_output=True, text=True, encoding='utf-8', errors='replace')


def pack(root, tar_path):
    """the tree as a tar owned by nobody in particular (the stick is exFAT: no owners, no modes)"""
    with tarfile.open(tar_path, 'w') as tar:
        for dirpath, dirs, files in os.walk(root):
            dirs.sort()
            for name in sorted(files):
                full = os.path.join(dirpath, name)
                info = tar.gettarinfo(full, arcname=os.path.relpath(full, root).replace(os.sep, '/'))
                info.uid = info.gid = 0
                info.uname = info.gname = 'root'
                info.mode = 0o644
                with open(full, 'rb') as f:
                    tar.addfile(info, f)


def lay(who, cache, restart):
    work = tempfile.mkdtemp(prefix='ab-testdata-')
    try:
        root = os.path.join(work, 'root')
        build(root, cache)
        tar_path = os.path.join(work, 'platform14-data.tar')
        pack(root, tar_path)
        done = abvm(who, 'install', tar_path, GUEST_TAR)
        if done.returncode:
            raise SystemExit('abvm install failed (%s): %s' % (done.returncode, (done.stdout + done.stderr).strip()))
        # --skip-old-files: whatever is on the stick already stays as it is
        cmd = ('sudo tar -x --skip-old-files --no-same-owner --no-same-permissions -C %s -f %s; '
               'sudo rm -f %s %s.abvm-orig; sync; echo laid' % (STICK, GUEST_TAR, GUEST_TAR, GUEST_TAR))
        done = abvm(who, 'guest', cmd)
        print((done.stdout + done.stderr).strip())
        if 'laid' not in done.stdout:
            raise SystemExit('the unpack in the guest failed')
        if restart:
            done = abvm(who, 'restart')
            print((done.stdout + done.stderr).strip())
            # a started launcher does not look for new folders by itself: the System menu's Re-scan games
            done = abvm(who, 'drive', 'home; menu Re-scan games; wait 3000; wait_screen GuiLauncher 120; '
                                      'wait_idle 500 30')
            print((done.stdout + done.stderr).strip())
    finally:
        shutil.rmtree(work, ignore_errors=True)


def check(who):
    paths = ' '.join("'%s/%s'" % (STICK, p) for p in markers())
    cmd = 'for p in %s; do [ -e "$p" ] || echo "missing $p"; done; echo checked' % paths
    done = abvm(who, 'guest', cmd)
    out = done.stdout.strip()
    print(out if out else done.stderr.strip())
    return 0 if 'checked' in out and 'missing' not in out else 1


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('command', choices=('build', 'lay', 'check'))
    ap.add_argument('dir', nargs='?', help='build: the stick root to lay into')
    ap.add_argument('--who', default=os.environ.get('ABVM_WHO', ''), help='the VM lease holder (lay, check)')
    ap.add_argument('--cache', default=os.path.join(tempfile.gettempdir(), 'ab-testdata-cache'))
    ap.add_argument('--no-restart', action='store_true')
    args = ap.parse_args()
    if args.command == 'build':
        if not args.dir:
            ap.error('build needs DIR')
        build(os.path.abspath(args.dir), args.cache)
        print('data set laid in', args.dir)
        return 0
    if not args.who:
        ap.error('lay and check need --who NAME (or ABVM_WHO) - the lease holder')
    if args.command == 'lay':
        lay(args.who, args.cache, not args.no_restart)
        return check(args.who)
    return check(args.who)


if __name__ == '__main__':
    sys.exit(main())
