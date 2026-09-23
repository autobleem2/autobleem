#!/usr/bin/env python3
"""Builds (or refreshes) a mocked USB root for running autobleem-gui on a PC.

    python tools/make_usb.py <usbRoot> [--fresh] [--build <buildDir>]

The layout is CLAUDE.md's "Smoke test layout": the payload's rc scripts and themes, src/resources next to the
binary, the cover DBs, a copy of internal.db, and one fake PS1 game (a generated bin/cue whose ISO holds a
SLUS_012.34 file, so the scanner finds a serial). A fake RetroArch install too - a stub binary (what makes
the app treat RetroArch as present), one fake core with an .info naming three systems, and a few tiny zipped
ROMs in roms/<system>/ - so the background scan's ROM pass has something to write playlists for. Everything
that comes from the repo is refreshed on every run - the exe, resources, themes, rc scripts - and everything
the app writes stays (Games/, System/, regional.db, memcards, save states, the playlists) unless --fresh
wipes the whole tree first.

The .vscode tasks call it after every build; tools/win_drive.ps1 and the debugger run against it.
"""
import argparse
import os
import shutil
import sqlite3
import struct
import sys
import zipfile

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def copy_tree(src, dst):
    """copies src over dst, file by file (dst may exist and keep files src does not have)"""
    for root, dirs, files in os.walk(src):
        rel = os.path.relpath(root, src)
        target = dst if rel == '.' else os.path.join(dst, rel)
        os.makedirs(target, exist_ok=True)
        for f in files:
            shutil.copy2(os.path.join(root, f), os.path.join(target, f))


def replace_tree(src, dst):
    """dst becomes a copy of src (a stale theme file must not survive a refresh)"""
    if os.path.isdir(dst):
        shutil.rmtree(dst)
    shutil.copytree(src, dst)


# ---------------------------------------------------------------------------------------------------------------
# a minimal ISO9660 image in MODE2/2352 raw sectors: PVD in sector 16, terminator in 17, root directory in 18
# with SYSTEM.CNF and a file named after the serial - which is what SerialScanner looks for.
# ---------------------------------------------------------------------------------------------------------------
SECTOR = 2352
DATA = 2048
SERIAL_FILE = 'SLUS_012.34'      # -> serial SLUS-01234
TITLE = 'Fake Game'


def both_endian32(v):
    return struct.pack('<I', v) + struct.pack('>I', v)


def both_endian16(v):
    return struct.pack('<H', v) + struct.pack('>H', v)


def dir_record(name, extent, size, is_dir):
    ident = name.encode('ascii')
    rec = bytearray()
    rec += b'\x00'                       # extended attribute length
    rec += both_endian32(extent)
    rec += both_endian32(size)
    rec += bytes(7)                      # recording date
    rec += bytes([0x02 if is_dir else 0x00])
    rec += bytes(2)                      # unit size, gap
    rec += both_endian16(1)              # volume sequence number
    rec += bytes([len(ident)]) + ident
    if len(rec) % 2 == 0:                # the length byte plus the record must be even
        rec += b'\x00'
    return bytes([len(rec) + 1]) + bytes(rec)


def raw_sector(number, data):
    """wraps 2048 bytes of data in a MODE2 form 1 raw sector (sync, header, subheader; no ECC)"""
    assert len(data) <= DATA
    sync = b'\x00' + b'\xff' * 10 + b'\x00'
    minutes, rest = divmod(number + 150, 75 * 60)
    seconds, frames = divmod(rest, 75)
    bcd = lambda v: ((v // 10) << 4) | (v % 10)
    header = bytes([bcd(minutes), bcd(seconds), bcd(frames), 0x02])
    subheader = bytes([0, 0, 0x08, 0]) * 2
    body = sync + header + subheader + data.ljust(DATA, b'\x00')
    return body.ljust(SECTOR, b'\x00')


def make_iso(sectors=24, serial_file=SERIAL_FILE, title=TITLE):
    root_sector = 18
    cnf_sector = 19
    serial_sector = 20
    system_cnf = ('BOOT = cdrom:\\%s;1\r\nTCB = 4\r\nEVENT = 10\r\nSTACK = 801FFFF0\r\n' % serial_file).encode('ascii')

    root = dir_record('\x00', root_sector, DATA, True) + dir_record('\x01', root_sector, DATA, True)
    root += dir_record(serial_file + ';1', serial_sector, 4, False)
    root += dir_record('SYSTEM.CNF;1', cnf_sector, len(system_cnf), False)

    pvd = bytearray()
    pvd += b'\x01' + b'CD001' + b'\x01\x00'
    pvd += b'PLAYSTATION'.ljust(32)                 # system identifier
    pvd += title.upper().replace(' ', '').encode('ascii')[:32].ljust(32)   # volume identifier
    pvd += bytes(8)
    pvd += both_endian32(sectors)                   # volume space size
    pvd += bytes(32)
    pvd += both_endian16(1) + both_endian16(1)      # volume set size, sequence number
    pvd += both_endian16(DATA)                      # logical block size
    pvd += both_endian32(10)                        # path table size
    pvd += struct.pack('<I', 0) * 2 + struct.pack('>I', 0) * 2   # path table locations (none)
    root_rec = dir_record('\x00', root_sector, DATA, True)
    assert len(pvd) == 156
    pvd += root_rec.ljust(34, b'\x00')
    pvd = bytes(pvd).ljust(DATA, b'\x00')
    assert struct.unpack('<I', pvd[158:162])[0] == root_sector

    image = bytearray()
    for n in range(sectors):
        if n == 16:
            data = pvd
        elif n == 17:
            data = b'\xffCD001\x01' + bytes(DATA - 7)
        elif n == root_sector:
            data = root
        elif n == cnf_sector:
            data = system_cnf
        elif n == serial_sector:
            data = b'fake'
        else:
            data = b''
        image += raw_sector(n, data)
    return bytes(image)


def make_fake_game(games_dir, title=TITLE, serial_file=SERIAL_FILE):
    folder = os.path.join(games_dir, title)
    if os.path.exists(os.path.join(folder, title + '.bin')):
        return
    os.makedirs(folder, exist_ok=True)
    with open(os.path.join(folder, title + '.bin'), 'wb') as f:
        f.write(make_iso(serial_file=serial_file, title=title))
    with open(os.path.join(folder, title + '.cue'), 'w', newline='\n') as f:
        f.write('FILE "%s.bin" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n' % title)
    print('fake game:', folder)


# --games N: more fake games, spread over sub-folders so the set picker and the folder rows have
# something to show, each with its own serial (SLUS_012.34, .35, ...); the folder name is the title (the
# cover databases here are stubs)
FAKE_FOLDERS = ['', 'Action', 'Action/Platformers', 'RPG', 'Puzzle']
FAKE_TITLES = ['Fake Game', 'Crash Dummies', 'Spyro the Fake', 'Tekken Faux', 'Final Fakesy', 'Gran Fakismo',
               'Metal Gear Fake', 'Tomb Faker', 'Ridge Faker', 'Wipeout Fake', 'Castlevania Faux', 'Resident Fake',
               'Silent Fake', 'Tony Fake Pro Skater', 'Crash Team Faking', 'Spyro Fake 2', 'Ape Fake',
               'Parappa the Faker', 'Vagrant Fake', 'Chrono Faux', 'Fake Fantasy Tactics', 'Bust a Fake',
               'Klonoa Fake', 'Mega Fake X4', 'Suikoden Faux', 'Xenofakes', 'Faketris', 'Point Fake',
               'Rayman Fake', 'Oddfake']


def make_fake_games(games_dir, count):
    for i in range(count):
        title = FAKE_TITLES[i % len(FAKE_TITLES)] + ('' if i < len(FAKE_TITLES) else ' %d' % (i // len(FAKE_TITLES) + 1))
        sub = FAKE_FOLDERS[i % len(FAKE_FOLDERS)]
        serial = 'SLUS_%03d.%02d' % (12 + i // 100, (34 + i) % 100)
        make_fake_game(os.path.join(games_dir, sub) if sub else games_dir, title, serial)


def make_fake_memcards(games_dir):
    """three memory card sets from the blank cards in src/resources/memcard, for the memory card screens"""
    template = os.path.join(REPO, 'src', 'resources', 'memcard')
    for name in ('Fighting games', 'RPG saves', 'Kids'):
        folder = os.path.join(games_dir, '!MemCards', name)
        if os.path.exists(folder):
            continue
        os.makedirs(folder)
        for card in ('card1.mcd', 'card2.mcd'):
            shutil.copy2(os.path.join(template, card), os.path.join(folder, card))
        print('fake memory cards:', folder)


def make_cover_db(path):
    """an empty covers db with the real schema, for when db/ (git-ignored) has none"""
    if os.path.exists(path):
        return
    db = sqlite3.connect(path)
    db.execute('CREATE TABLE `GAME` ( `ID` INTEGER NOT NULL UNIQUE, `TITLE` TEXT NOT NULL, `PUBLISHER` TEXT NOT NULL, '
               '`RELEASE` INTEGER NOT NULL, `PLAYERS` INTEGER NOT NULL, `COVER` BLOB, PRIMARY KEY(`ID`) )')
    db.execute('CREATE TABLE `SERIALS` ( `SERIAL` TEXT NOT NULL, `GAME` INTEGER NOT NULL, PRIMARY KEY(`SERIAL`) )')
    db.commit()
    db.close()


# the console tools under apps/ that are staged into usb/Apps/<tool>/ for a visual test on Windows
TOOLS = ['pscbios', 'abflashkit']

# the fake RetroArch: one core that "plays" three systems, and a couple of ROMs per system. The names are
# real no-intro names so the thumbnail lookup has something to match once a thumbnail pack is dropped in.
FAKE_SYSTEMS = {
    'Nintendo - Nintendo Entertainment System': ('nes', ['Adventures of Lolo (USA)', 'Arkanoid (USA)',
                                                         'Battletoads (USA)']),
    'Nintendo - Super Nintendo Entertainment System': ('sfc', ['Chrono Trigger (USA)', 'EarthBound (USA)']),
    'Sega - Mega Drive - Genesis': ('md', ['Sonic The Hedgehog (USA, Europe)', 'Streets of Rage 2 (USA)']),
}


def make_fake_retroarch(usb):
    """RetroArch/bin with a stub binary, the fake core + .info, RetroArch/bios, and RetroArch/roms/<system>/<game>.zip
    (one ROM inside) - the console's layout"""
    ra = os.path.join(usb, 'RetroArch', 'bin')
    os.makedirs(os.path.join(usb, 'RetroArch', 'bios'), exist_ok=True)
    for d in ('cores', 'info', 'playlists'):
        os.makedirs(os.path.join(ra, d), exist_ok=True)
    binary = os.path.join(ra, 'retroarch')
    if not os.path.exists(binary):
        with open(binary, 'w') as f:
            f.write('#!/bin/sh\n# a stand-in: Env::retroArchInstalled() only asks whether the file exists\n')
    with open(os.path.join(ra, 'cores', 'fake_libretro.so'), 'wb') as f:
        f.write(b'not a core')
    extensions = '|'.join(sorted({ext for ext, _ in FAKE_SYSTEMS.values()}))
    with open(os.path.join(ra, 'info', 'fake_libretro.info'), 'w', encoding='utf-8') as f:
        f.write('display_name = "Fake core"\n')
        f.write('supported_extensions = "%s"\n' % extensions)
        f.write('database = "%s"\n' % '|'.join(FAKE_SYSTEMS))

    roms = os.path.join(usb, 'RetroArch', 'roms')
    for system, (ext, games) in FAKE_SYSTEMS.items():
        folder = os.path.join(roms, system)
        os.makedirs(folder, exist_ok=True)
        for game in games:
            path = os.path.join(folder, game + '.zip')
            if os.path.exists(path):
                continue
            with zipfile.ZipFile(path, 'w', zipfile.ZIP_DEFLATED) as z:
                z.writestr('%s.%s' % (game, ext), b'fake rom ' + game.encode('utf-8'))


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('usb', help='the USB root to create or refresh')
    ap.add_argument('--fresh', action='store_true', help='delete the tree first')
    ap.add_argument('--games', type=int, default=1, help='how many fake PS1 games (over a few sub-folders; default 1)')
    ap.add_argument('--build', default=os.path.join(REPO, 'build_win'), help='where autobleem-gui.exe is (default build_win)')
    args = ap.parse_args()
    usb = os.path.abspath(args.usb)

    if args.fresh and os.path.isdir(usb):
        shutil.rmtree(usb)
    os.makedirs(usb, exist_ok=True)

    app = os.path.join(usb, 'Autobleem', 'bin', 'autobleem')
    copy_tree(os.path.join(REPO, 'src', 'resources'), app)          # resources next to the binary, fonts/ included
    exe = os.path.join(args.build, 'autobleem-gui.exe')
    if os.path.exists(exe):
        shutil.copy2(exe, app)
    else:
        print('note: no', exe, '- build first (make_win.sh), then run this again or let the build task copy it')
    # the console's update helpers, next to the launcher as on a stick: abfetch (+ its CA bundle) fetches,
    # abupdate lays the downloaded package over the stick - so the console's update can be tried on a PC
    for helper in ('abfetch.exe', 'cacert.pem', 'abupdate.exe'):
        if os.path.exists(os.path.join(args.build, helper)):
            shutil.copy2(os.path.join(args.build, helper), app)

    replace_tree(os.path.join(REPO, 'payload', 'Autobleem', 'rc'), os.path.join(usb, 'Autobleem', 'rc'))
    replace_tree(os.path.join(REPO, 'payload', 'Themes'), os.path.join(usb, 'Themes'))
    if os.path.isdir(os.path.join(REPO, 'payload', 'Apps')):
        replace_tree(os.path.join(REPO, 'payload', 'Apps'), os.path.join(usb, 'Apps'))

    # the console tools built from apps/: each one's resources plus its Windows exe over the payload's copy,
    # so that usb/Apps/<tool>/<tool>.exe <usb root> is the visual test of it
    for tool in TOOLS:
        src = os.path.join(REPO, 'apps', tool, 'resources')
        if not os.path.isdir(src):
            continue
        dst = os.path.join(usb, 'Apps', tool)
        copy_tree(src, dst)
        tool_exe = os.path.join(args.build, 'apps', tool, tool + '.exe')
        if os.path.exists(tool_exe):
            shutil.copy2(tool_exe, dst)
        else:
            print('note: no', tool_exe, '- build first for the', tool, 'visual test')

    db_dir = os.path.join(usb, 'Autobleem', 'bin', 'db')
    os.makedirs(db_dir, exist_ok=True)
    for region in 'JPU':
        name = 'covers%s.db' % region
        src = os.path.join(REPO, 'db', name)
        if os.path.exists(src):
            shutil.copy2(src, os.path.join(db_dir, name))
        else:
            make_cover_db(os.path.join(db_dir, name))

    databases = os.path.join(usb, 'System', 'Databases')
    os.makedirs(databases, exist_ok=True)
    os.makedirs(os.path.join(usb, 'System', 'Logs'), exist_ok=True)
    if not os.path.exists(os.path.join(databases, 'internal.db')):
        shutil.copy2(os.path.join(REPO, 'src', 'resources', 'internal.db'), databases)

    # the app writes an EmulationStation gamelist here after a scan, and demands a scan at every boot while the
    # file is missing - the directory has to exist for it to be written at all
    os.makedirs(os.path.join(usb, 'RetroArch', 'bin', 'retroboot', 'emulationstation', '.emulationstation', 'gamelists', 'psx'),
                exist_ok=True)

    games = os.path.join(usb, 'Games')
    os.makedirs(games, exist_ok=True)
    make_fake_games(games, max(1, args.games))
    make_fake_memcards(games)
    make_fake_retroarch(usb)

    # UpdateRoms, the PC-side scanner, in the stick's root as a release lays it out (the DLLs come from
    # PATH here; tools/make_updateroms_bundle.sh gathers them for a real stick)
    updateroms = os.path.join(args.build, 'apps', 'updateroms', 'UpdateRoms.exe')
    if os.path.exists(updateroms):
        dst = os.path.join(usb, 'UpdateRoms')
        copy_tree(os.path.join(REPO, 'apps', 'updateroms', 'resources'), dst)
        shutil.copy2(updateroms, dst)

    print('usb root ready:', usb)
    print('run:  cd "%s" && autobleem-gui.exe "%s"   (C:\\msys64\\ucrt64\\bin on PATH for the SDL DLLs)' % (app, usb))
    return 0


if __name__ == '__main__':
    sys.exit(main())
