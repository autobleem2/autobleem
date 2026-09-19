#!/usr/bin/env python3
"""Refreshes a PSC USB stick: strips whatever old AutoBleem install is on it (any version - this
matches by the stable top-level layout AutoBleem has always shipped, not by version-specific content)
down to Games/themes/roms and the console's boot-exploit/RetroBoot layer, then lays down a fresh
AutoBleem2 build on top. Cross-platform (Windows/Linux), stdlib only.

    python tools/install_autobleem.py                                   interactive
    python tools/install_autobleem.py --drive F:\\ --dry-run
    python tools/install_autobleem.py --drive /media/user/AUTOBLEEM --execute
    python tools/install_autobleem.py --drive F:\\ --execute --stage clean
    python tools/install_autobleem.py --drive F:\\ --execute --with-retroboot

Three stages (--stage), 'all' (the default) runs clean then install:
  analyze   report what's on the drive; changes nothing
  clean     remove old AutoBleem elements + macOS junk (see CLEAN NOTES below)
  install   copy a fresh AutoBleem2 payload onto the drive (does not clean first)

CLEAN NOTES - removed, because rc/backup.sh and backup_internal.sh regenerate every one of these
from the console's own /gaadata and /data at the next boot (see payload/Autobleem/rc/backup*.sh):
  macOS junk (.DS_Store, AppleDouble ._* files, .Trashes, .Spotlight-V100, ...) - anywhere on the drive
  Autobleem/                                  the old frontend binary + rc scripts
  System/Bios, Preferences, Databases,        (Databases holds regional.db/internal.db - regional.db is
    Region, Logs, UI, lightguns.txt            rebuilt by AutoBleem2's next scan)
  Apps/pscbios/, Apps/abflashkit/             AutoBleem2 ships these two fresh (--remove-all-apps for
                                               the whole Apps/ folder instead, --keep-apps for neither)
  retroarch/playlists/AutoBleem.lpl           the PS1 library export; rewritten by AutoBleem2's next
                                               boot/scan (needs a real boot - UpdateRoms.exe never touches
                                               PS1 playlists, only the per-system ROM ones)
  retroarch/playlists/Sony - PlayStation.lpl  a stale PS1 playlist not written by AutoBleem2 at all
                                               (--remove-retroarch-tree removes the whole tree instead)
Left alone: Games/, themes/, roms/, the rest of retroarch/ (RetroBoot's own tree, boot-critical), the
boot-exploit payload folder (a bare UUID-named directory - required to boot at all), and anything else
not recognized as AutoBleem's own (reported, never touched).

INSTALL NOTES - copies onto the (already cleaned, or already-fresh) drive:
  build_psc/dist/autobleem-gui  -> Autobleem/bin/autobleem/autobleem-gui   (run ./make_psc.sh first)
  src/resources/                -> Autobleem/bin/autobleem/
  payload/Autobleem/{rc,start.sh,lib,bin/emu}  -> Autobleem/...
  db/covers*.db                 -> Autobleem/bin/db/           (optional - see README/CLAUDE.md)
  payload/Apps/<name>/          -> Apps/<name>/                (each app folder replaced whole)
  payload/themes/<name>/        -> themes/<name>/              (each theme folder replaced whole)
  build_win/UpdateRoms/         -> UpdateRoms/                 (run ./tools/make_updateroms_bundle.sh first,
                                                                 or pass --skip-updateroms)
Each prerequisite that is missing is reported and skipped rather than aborting the whole install, so
e.g. Apps/themes can be refreshed even without a freshly built binary at hand.

roms/<old short name>/ (nes, snes, gba, ...) is renamed to roms/<RetroArch database name>/ (see
ROMS_LAYOUT_MAP) - the scanner only recognizes a folder named exactly as its database is. Two old
names that target the same database (nes+famicom, snes+sfc, ...) are merged, not overwritten.
--skip-roms-convert leaves roms/ untouched.

UpdateRoms.exe (apps/updateroms/) is the PC-side RetroArch scanner - run it from the stick after install
(UpdateRoms/UpdateRoms.exe, or --quiet for no window) to write the per-system playlists under roms/<system>/
with the PC's network (box art, RetroArch's databases). It deliberately never touches AutoBleem.lpl,
Favorites or History - AutoBleem.lpl (the PS1 library export) is written only by autobleem-gui's own scan,
which needs a real boot (the console, or a PC debug build run against the stick once).

RETROBOOT (optional, off unless asked): --with-retroboot copies a RetroBoot/RetroArch bundle to
retroarch/ when the drive doesn't already have one (or always, with --replace-retroboot). The source
is --retroboot-source, default vendor/retroboot-bundle/retroarch - not part of this repo; populate it
yourself from a full AutoBleem release package (see tools/install_autobleem.py --help-retroboot).

This tool does not jailbreak a blank stick - it refreshes AutoBleem on one that is already exploited
(the boot-exploit folder must already be there; --remove-boot-exploit deletes it, which needs redoing
the jailbreak).
"""
import argparse
import ctypes
import glob
import os
import re
import shutil
import stat
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent

JUNK_DIR_NAMES = {'.Trashes', '.Spotlight-V100', '.TemporaryItems', '.fseventsd', '.apdisk',
                  '.DocumentRevisions-V100'}
JUNK_FILE_NAMES = {'.DS_Store', '.VolumeIcon.icns', '.com.apple.timemachine.supported'}
UUID_RE = re.compile(r'^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$')

SYSTEM_BACKUP_DIRS = ['Bios', 'Preferences', 'Databases', 'Region', 'Logs', 'UI']

# roms/<folder> must be named exactly as the RetroArch database it plays, or the scanner skips it
# ("a folder per system named as RetroArch's databases are" - resources/platform/psc.ini). These are the
# old ES/RetroPie-style short names AutoBleem2 inherited from older AutoBleem installs, mapped onto the
# canonical database name - verified against retroarch/info/*.info's "database" fields (installed cores)
# and retroarch/database/rdb/*.rdb (the curated identification databases). Several old names collapse
# onto the same target (nes+famicom, snes+sfc, genesis+megadrive, pcengine+tg16, pcenginecd+tg-cd,
# arcade+fba2012+neogeo) - convert_roms_layout() merges their contents rather than overwriting.
ROMS_LAYOUT_MAP = {
    '3do': 'The 3DO Company - 3DO',
    'a2600': 'Atari - 2600',
    'a5200': 'Atari - 5200',
    'a7800': 'Atari - 7800',
    'amiga': 'Commodore - Amiga',
    'amstradcpc': 'Amstrad - CPC',
    'arcade': 'FBNeo - Arcade Games',
    'atarijaguar': 'Atari - Jaguar',
    'atarilynx': 'Atari - Lynx',
    'atarist': 'Atari - ST',
    'c64': 'Commodore - 64',
    'colecovision': 'Coleco - ColecoVision',
    'daphne': 'Daphne',
    'dosbox': 'DOS',
    'dreamcast': 'Sega - Dreamcast',
    'famicom': 'Nintendo - Nintendo Entertainment System',
    'fba2012': 'FBNeo - Arcade Games',
    'fds': 'Nintendo - Family Computer Disk System',
    'gameandwatch': 'Handheld Electronic Game',
    'gamegear': 'Sega - Game Gear',
    'gb': 'Nintendo - Game Boy',
    'gba': 'Nintendo - Game Boy Advance',
    'gbc': 'Nintendo - Game Boy Color',
    'genesis': 'Sega - Mega Drive - Genesis',
    'intellivision': 'Mattel - Intellivision',
    'mame': 'MAME',
    'mastersystem': 'Sega - Master System - Mark III',
    'megadrive': 'Sega - Mega Drive - Genesis',
    'msx': 'Microsoft - MSX',
    'n64': 'Nintendo - Nintendo 64',
    'naomi': 'Sega - NAOMI',
    'nds': 'Nintendo - Nintendo DS',
    'neogeo': 'FBNeo - Arcade Games',
    'neogeocd': 'SNK - Neo Geo CD',
    'nes': 'Nintendo - Nintendo Entertainment System',
    'ngp': 'SNK - Neo Geo Pocket',
    'ngpc': 'SNK - Neo Geo Pocket Color',
    'odyssey2': 'Magnavox - Odyssey2',
    'pcengine': 'NEC - PC Engine - TurboGrafx 16',
    'pcenginecd': 'NEC - PC Engine CD - TurboGrafx-CD',
    'psp': 'Sony - PlayStation Portable',
    'saturn': 'Sega - Saturn',
    'scummvm': 'ScummVM',
    'sega32x': 'Sega - 32X',
    'segacd': 'Sega - Mega-CD - Sega CD',
    'sfc': 'Nintendo - Super Nintendo Entertainment System',
    'sg1000': 'Sega - SG-1000',
    'snes': 'Nintendo - Super Nintendo Entertainment System',
    'snesh': 'Nintendo - Super Nintendo Entertainment System Hacks',
    'supergrafx': 'NEC - PC Engine SuperGrafx',
    'tg-cd': 'NEC - PC Engine CD - TurboGrafx-CD',
    'tg16': 'NEC - PC Engine - TurboGrafx 16',
    'virtualboy': 'Nintendo - Virtual Boy',
    'wonderswan': 'Bandai - WonderSwan',
    'zxspectrum': 'Sinclair - ZX Spectrum +3',
}

# old folder names known to have NO confident canonical target - reported, never touched:
#  atari800: no distinct database for 8-bit computers (would wrongly collide with the 5200 console)
#  gbah/gbh/ggh/genh/nesh: no official "<system> Hacks" database exists for these (unlike SNES)
#  psx: PS1 is Games/ + AutoBleem.lpl's job, never a roms/ folder - out of scope by design
#  videos: not a ROM system at all
ROMS_LAYOUT_SKIP = {'atari800', 'gbah', 'gbh', 'ggh', 'genh', 'nesh', 'psx', 'videos'}


# --------------------------------------------------------------------------------------------------
# small helpers
# --------------------------------------------------------------------------------------------------

def human(n):
    for unit in ('B', 'KB', 'MB', 'GB'):
        if n < 1024 or unit == 'GB':
            return f'{n:.1f} {unit}' if unit != 'B' else f'{n} B'
        n /= 1024


def path_size(p: Path) -> int:
    if p.is_file() or p.is_symlink():
        try:
            return p.lstat().st_size
        except OSError:
            return 0
    total = 0
    for root, _dirs, files in os.walk(p):
        for f in files:
            try:
                total += os.lstat(os.path.join(root, f)).st_size
            except OSError:
                pass
    return total


def is_junk(name: str, is_dir: bool) -> bool:
    if is_dir:
        return name in JUNK_DIR_NAMES
    return name in JUNK_FILE_NAMES or name.startswith('._')


def find_ci(parent: Path, name: str):
    """case-insensitive child lookup - exFAT layouts drift in case across AutoBleem versions/OSes"""
    if not parent.is_dir():
        return None
    target = name.lower()
    try:
        for child in parent.iterdir():
            if child.name.lower() == target:
                return child
    except OSError:
        pass
    return None


def onerror_chmod(func, path, _exc_info):
    """shutil.rmtree callback: clear read-only (common on files copied off exFAT) and retry once"""
    try:
        os.chmod(path, stat.S_IWRITE)
        func(path)
    except OSError:
        pass


def remove_path(p: Path):
    if p.is_dir() and not p.is_symlink():
        shutil.rmtree(p, onerror=onerror_chmod)
    else:
        try:
            p.unlink()
        except PermissionError:
            os.chmod(p, stat.S_IWRITE)
            p.unlink()


def replace_tree(src: Path, dst: Path, dry_run: bool, log):
    """dst becomes a copy of src - a stale file from an old version must not survive a refresh"""
    if dry_run:
        log(f'[DRYRUN] would replace {dst} with {src}')
        return
    if dst.exists():
        remove_path(dst)
    shutil.copytree(src, dst)


def copy_tree_merge(src: Path, dst: Path, dry_run: bool, log):
    """copies src over dst file by file - dst may already exist and keep files src does not have"""
    if dry_run:
        log(f'[DRYRUN] would copy {src} into {dst}')
        return
    for root, _dirs, files in os.walk(src):
        rel = os.path.relpath(root, src)
        target = dst if rel == '.' else dst / rel
        target.mkdir(parents=True, exist_ok=True)
        for f in files:
            shutil.copy2(os.path.join(root, f), target / f)


def convert_roms_layout(roms_dir: Path, dry_run: bool, log):
    """renames/merges roms/<old ES-style name> onto roms/<canonical RetroArch database name>, so the
    scanner (the console's own, or UpdateRoms.exe) recognizes the folder - see ROMS_LAYOUT_MAP"""
    renamed = merged = 0
    skipped_present = []
    claimed = set()  # target names an earlier entry in this pass already took, even in dry-run

    for old_name, target_name in sorted(ROMS_LAYOUT_MAP.items()):
        src = find_ci(roms_dir, old_name)
        if not src or src.name == target_name:
            continue
        existing_dst = find_ci(roms_dir, target_name)

        # old_name and target_name differ only by case (daphne -> Daphne): find_ci's case-insensitive
        # lookup for target_name finds src itself, NOT a separate folder. Treating that as "already
        # exists, merge into it" would move src's contents into itself then rmdir it - destroying it
        # with nothing surviving. This is a plain case-fixup rename, never a merge.
        if existing_dst is not None and existing_dst == src:
            if dry_run:
                log(f'[DRYRUN] would rename roms/{src.name} -> roms/{target_name} (case only)')
            else:
                # a same-name-different-case rename is unreliable on case-insensitive filesystems
                # (Windows/exFAT) without an intermediate name
                tmp = src.with_name(src.name + '.ab_rename_tmp')
                src.rename(tmp)
                tmp.rename(roms_dir / target_name)
            renamed += 1
            claimed.add(target_name.lower())
        elif existing_dst is None and target_name.lower() not in claimed:
            if dry_run:
                log(f'[DRYRUN] would rename roms/{src.name} -> roms/{target_name}')
            else:
                src.rename(roms_dir / target_name)
            renamed += 1
            claimed.add(target_name.lower())
        else:
            dst = existing_dst or (roms_dir / target_name)
            entries = list(src.iterdir()) if src.is_dir() else []
            if dry_run:
                log(f'[DRYRUN] would merge roms/{src.name} ({len(entries)} item(s)) into roms/{target_name}, then remove roms/{src.name}')
            else:
                for item in entries:
                    shutil.move(str(item), str(dst / item.name))
                src.rmdir()
            merged += 1

    for old_name in sorted(ROMS_LAYOUT_SKIP):
        if find_ci(roms_dir, old_name):
            skipped_present.append(old_name)

    if renamed or merged or skipped_present:
        print(f'--- roms/ layout: {renamed} renamed, {merged} merged onto an existing folder ---')
        if skipped_present:
            print(f'  left as-is (no confident database mapping): {", ".join(skipped_present)}')


# --------------------------------------------------------------------------------------------------
# drive discovery
# --------------------------------------------------------------------------------------------------

def _windows_volume_label(letter: str) -> str:
    buf = ctypes.create_unicode_buffer(261)
    ok = ctypes.windll.kernel32.GetVolumeInformationW(
        ctypes.c_wchar_p(letter), buf, 260, None, None, None, None, 0)
    return buf.value if ok else ''


def list_removable_drives():
    """best-effort; the tool always also accepts a manually typed path"""
    drives = []
    if os.name == 'nt':
        DRIVE_REMOVABLE = 2
        bitmask = ctypes.windll.kernel32.GetLogicalDrives()
        for i in range(26):
            if not (bitmask & (1 << i)):
                continue
            letter = f'{chr(ord("A") + i)}:\\'
            if ctypes.windll.kernel32.GetDriveTypeW(ctypes.c_wchar_p(letter)) != DRIVE_REMOVABLE:
                continue
            try:
                total, _used, _free = shutil.disk_usage(letter)
            except OSError:
                continue
            drives.append((letter, _windows_volume_label(letter), total))
    else:
        seen = set()
        for pattern in ('/media/*/*', '/run/media/*/*', '/mnt/*'):
            for r in glob.glob(pattern):
                if r in seen or not os.path.ismount(r):
                    continue
                seen.add(r)
                try:
                    total, _used, _free = shutil.disk_usage(r)
                except OSError:
                    continue
                drives.append((r, os.path.basename(r), total))
    return drives


def prompt_for_drive() -> str:
    drives = list_removable_drives()
    if drives:
        print('Removable drives found:')
        for i, (path, label, total) in enumerate(drives, 1):
            print(f'  {i}) {path}  {label or "(no label)"}  {human(total)}')
        print(f'  {len(drives) + 1}) enter a path manually')
        choice = input('Pick one: ').strip()
        if choice.isdigit() and 1 <= int(choice) <= len(drives):
            return drives[int(choice) - 1][0]
    return input('USB drive path (e.g. F:\\ or /media/you/AUTOBLEEM): ').strip()


# --------------------------------------------------------------------------------------------------
# analyze
# --------------------------------------------------------------------------------------------------

class Analysis:
    def __init__(self):
        self.junk = []                       # list[Path]
        self.actions = []                    # list[(Path, category, reason)]
        self.unrecognized = []               # list[Path]
        self.boot_exploit_dirs = []          # list[Path]
        self.retroarch_dir = None            # Path or None
        self.games_dir = None
        self.themes_dir = None
        self.roms_dir = None


def analyze(root: Path, opts) -> Analysis:
    a = Analysis()

    print('--- scanning for macOS junk (walks the whole tree, can take a while) ---')
    for dirpath, dirnames, filenames in os.walk(root):
        base = Path(dirpath)
        keep_dirs = []
        for d in dirnames:
            if is_junk(d, True):
                a.junk.append(base / d)
            else:
                keep_dirs.append(d)
        dirnames[:] = keep_dirs
        for f in filenames:
            if is_junk(f, False):
                a.junk.append(base / f)

    def known(name):
        c = find_ci(root, name)
        return c

    autobleem_dir = known('Autobleem')
    if autobleem_dir:
        a.actions.append((autobleem_dir, 'Autobleem/', 'old frontend binary + rc scripts; AutoBleem2 installs fresh'))

    system_dir = known('System')
    if system_dir:
        for name in SYSTEM_BACKUP_DIRS:
            c = find_ci(system_dir, name)
            if c:
                a.actions.append((c, 'System backup dirs', 'recreated from the console (/gaadata, /data) at next boot'))
        lg = find_ci(system_dir, 'lightguns.txt')
        if lg:
            a.actions.append((lg, 'System/lightguns.txt', 'regenerated as light-gun games are flagged again'))

    apps_dir = known('Apps')
    if apps_dir:
        if opts.keep_apps:
            pass
        elif opts.remove_all_apps:
            a.actions.append((apps_dir, 'Apps/ (whole folder)', 'old bundled/third-party apps; --remove-all-apps'))
        else:
            for name in ('pscbios', 'abflashkit'):
                c = find_ci(apps_dir, name)
                if c:
                    a.actions.append((c, 'Apps/pscbios + abflashkit', 'AutoBleem2 ships this fresh'))

    retroarch_dir = known('retroarch')
    a.retroarch_dir = retroarch_dir
    if retroarch_dir:
        if opts.remove_retroarch_tree:
            a.actions.append((retroarch_dir, 'retroarch/ (whole tree)', '--remove-retroarch-tree'))
        elif not opts.keep_autobleem_playlists:
            playlists_dir = find_ci(retroarch_dir, 'playlists')
            if playlists_dir:
                c = find_ci(playlists_dir, 'AutoBleem.lpl')
                if c:
                    a.actions.append((c, 'AutoBleem playlists', "the PS1 library export - GameLibrary::exportToRetroArchPlaylist() "
                                                                  "rewrites it on AutoBleem2's next boot/scan"))
                c = find_ci(playlists_dir, 'Sony - PlayStation.lpl')
                if c:
                    a.actions.append((c, 'AutoBleem playlists', 'a stale PS1 playlist (not written by AutoBleem2 - '
                                                                  'likely an old RetroArch Import Content scan); '
                                                                  'AutoBleem2 uses AutoBleem.lpl for PS1 instead'))

    a.games_dir = known('Games')
    a.themes_dir = known('themes')
    a.roms_dir = known('roms')

    if root.is_dir():
        for child in root.iterdir():
            if UUID_RE.match(child.name):
                a.boot_exploit_dirs.append(child)
    if opts.remove_boot_exploit:
        for d in a.boot_exploit_dirs:
            a.actions.append((d, 'boot-exploit folder', '--remove-boot-exploit; jailbreak must be redone'))

    known_names = {'autobleem', 'apps', 'system', 'games', 'themes', 'roms', 'retroarch'}
    known_names |= {d.name.lower() for d in a.boot_exploit_dirs}
    junk_names_lower = {n.lower() for n in JUNK_DIR_NAMES} | {n.lower() for n in JUNK_FILE_NAMES}
    if root.is_dir():
        for child in root.iterdir():
            low = child.name.lower()
            if low in known_names or low in junk_names_lower or low.startswith('._'):
                continue
            a.unrecognized.append(child)

    return a


# --------------------------------------------------------------------------------------------------
# clean
# --------------------------------------------------------------------------------------------------

def stage_clean(root: Path, opts, dry_run: bool):
    a = analyze(root, opts)

    stats = {}

    def log_and_track(p: Path, category: str, reason: str):
        size = path_size(p)
        c = stats.setdefault(category, [0, 0])
        c[0] += 1
        c[1] += size
        if dry_run:
            print(f'[DRYRUN] would remove ({category}): {p}  -- {reason}')
        else:
            print(f'removing ({category}): {p}')
            try:
                remove_path(p)
            except OSError as e:
                print(f'  WARNING: failed to remove {p}: {e}', file=sys.stderr)

    for p in sorted(a.junk, key=lambda x: len(str(x))):
        if not p.exists():
            continue  # already gone as part of a parent junk dir
        log_and_track(p, 'macOS junk', 'macOS metadata, not part of AutoBleem or your games')

    for p, category, reason in a.actions:
        if not p.exists():
            continue
        log_and_track(p, category, reason)

    if a.boot_exploit_dirs and not opts.remove_boot_exploit:
        print('[skip] boot-exploit folder kept (needed to boot at all)')

    if a.unrecognized:
        print()
        print('--- unrecognized top-level items (left completely untouched) ---')
        for u in a.unrecognized:
            print(f'  {u}')

    print()
    print('=== clean summary ===')
    total_count = total_bytes = 0
    for category, (count, size) in stats.items():
        print(f'{category:<28} {count:>6} item(s)   {human(size):>10}')
        total_count += count
        total_bytes += size
    print('-' * 55)
    print(f'{"TOTAL":<28} {total_count:>6} item(s)   {human(total_bytes):>10}')
    if dry_run:
        print()
        print('Dry run only - nothing was deleted.')
    return stats


# --------------------------------------------------------------------------------------------------
# install
# --------------------------------------------------------------------------------------------------

def stage_install(root: Path, opts, dry_run: bool):
    def log(msg):
        print(msg)

    payload_ab = REPO_ROOT / 'payload' / 'Autobleem'
    resources = REPO_ROOT / 'src' / 'resources'
    dist_binary = REPO_ROOT / 'build_psc' / 'dist' / 'autobleem-gui'
    db_dir = REPO_ROOT / 'db'
    payload_apps = REPO_ROOT / 'payload' / 'Apps'
    payload_themes = REPO_ROOT / 'payload' / 'themes'

    usb_autobleem = find_ci(root, 'Autobleem') or (root / 'Autobleem')
    usb_apps = find_ci(root, 'Apps') or (root / 'Apps')
    usb_themes = find_ci(root, 'themes') or (root / 'themes')
    usb_games = find_ci(root, 'Games') or (root / 'Games')
    usb_system = find_ci(root, 'System') or (root / 'System')
    usb_roms = find_ci(root, 'roms') or (root / 'roms')

    print('--- installing fresh AutoBleem2 payload ---')

    if not payload_ab.is_dir():
        print(f'  ERROR: {payload_ab} missing - are you running this from inside the repo? skipping frontend install')
    else:
        for name in ('rc', 'lib'):
            src = payload_ab / name
            if src.is_dir():
                replace_tree(src, usb_autobleem / name, dry_run, log)
        start_sh = payload_ab / 'start.sh'
        if start_sh.is_file():
            if dry_run:
                log(f'[DRYRUN] would copy {start_sh} -> {usb_autobleem / "start.sh"}')
            else:
                usb_autobleem.mkdir(parents=True, exist_ok=True)
                shutil.copy2(start_sh, usb_autobleem / 'start.sh')
        emu_src = payload_ab / 'bin' / 'emu'
        if emu_src.is_dir():
            replace_tree(emu_src, usb_autobleem / 'bin' / 'emu', dry_run, log)

    if not resources.is_dir():
        print(f'  ERROR: {resources} missing - skipping resources install')
    else:
        replace_tree(resources, usb_autobleem / 'bin' / 'autobleem', dry_run, log)

    if not dist_binary.is_file():
        print(f'  WARNING: {dist_binary} missing - run ./make_psc.sh first. Skipping the binary itself;')
        print('           everything else (resources, rc scripts, Apps, themes) still gets refreshed.')
    else:
        dst = usb_autobleem / 'bin' / 'autobleem' / 'autobleem-gui'
        if dry_run:
            log(f'[DRYRUN] would copy {dist_binary} -> {dst}')
        else:
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(dist_binary, dst)
            try:
                os.chmod(dst, os.stat(dst).st_mode | stat.S_IEXEC | stat.S_IXGRP | stat.S_IXOTH)
            except OSError:
                pass

    covers = sorted(db_dir.glob('covers*.db')) if db_dir.is_dir() else []
    if not covers:
        print(f'  NOTE: no db/covers*.db found - AutoBleem2 will fall back to RetroArch/rdb-based cover '
              f'lookup where available, or default.png. See README for where to get cover DBs.')
    else:
        db_dst = usb_autobleem / 'bin' / 'db'
        if dry_run:
            for c in covers:
                log(f'[DRYRUN] would copy {c} -> {db_dst / c.name}')
        else:
            db_dst.mkdir(parents=True, exist_ok=True)
            for c in covers:
                shutil.copy2(c, db_dst / c.name)

    if not payload_apps.is_dir():
        print(f'  WARNING: {payload_apps} missing - skipping Apps install')
    else:
        for app_dir in sorted(p for p in payload_apps.iterdir() if p.is_dir()):
            replace_tree(app_dir, usb_apps / app_dir.name, dry_run, log)

    if not payload_themes.is_dir():
        print(f'  WARNING: {payload_themes} missing - skipping themes install')
    else:
        for theme_dir in sorted(p for p in payload_themes.iterdir() if p.is_dir()):
            replace_tree(theme_dir, usb_themes / theme_dir.name, dry_run, log)

    updateroms_src = opts.updateroms_source or (REPO_ROOT / 'build_win' / 'UpdateRoms')
    if not opts.skip_updateroms:
        if not (updateroms_src / 'UpdateRoms.exe').is_file():
            print(f'  NOTE: no UpdateRoms.exe at {updateroms_src} - run ./tools/make_updateroms_bundle.sh '
                  f'first (or pass --updateroms-source), or --skip-updateroms to silence this.')
        else:
            replace_tree(updateroms_src, root / 'UpdateRoms', dry_run, log)

    # scaffolding a genuinely blank stick needs, harmless if already there
    for d in (usb_games, usb_games / '!SaveStates', usb_games / '!MemCards', usb_system, usb_roms):
        if not d.exists():
            if dry_run:
                log(f'[DRYRUN] would create {d}')
            else:
                d.mkdir(parents=True, exist_ok=True)

    if not opts.skip_roms_convert and usb_roms.is_dir():
        convert_roms_layout(usb_roms, dry_run, log)

    # --- optional RetroBoot/RetroArch bundle ------------------------------------------------------
    usb_retroarch = find_ci(root, 'retroarch')
    has_retroarch = usb_retroarch is not None
    want = opts.with_retroboot
    if want is None:
        if has_retroarch:
            print('[skip] retroarch/ already present, kept as-is (pass --with-retroboot --replace-retroboot to refresh it)')
            want = False
        elif opts.interactive:
            want = confirm('No RetroBoot/RetroArch tree found on this drive. Install one now?', default=False)
        else:
            print('[skip] no retroarch/ tree and --with-retroboot not given; RetroArch integration will stay off')
            want = False

    if want and (not has_retroarch or opts.replace_retroboot):
        source = opts.retroboot_source or (REPO_ROOT / 'vendor' / 'retroboot-bundle' / 'retroarch')
        if not source.is_dir():
            print(f'  ERROR: RetroBoot source not found at {source} - pass --retroboot-source PATH')
        else:
            dst = usb_retroarch or (root / 'retroarch')
            print(f'--- installing RetroBoot/RetroArch bundle from {source} (large, this can take a while) ---')
            replace_tree(source, dst, dry_run, log)

    print()
    print('=== install summary ===')
    for label, p in (
        ('autobleem-gui', usb_autobleem / 'bin' / 'autobleem' / 'autobleem-gui'),
        ('resources', usb_autobleem / 'bin' / 'autobleem' / 'config.ini'),
        ('Apps/', usb_apps),
        ('themes/', usb_themes),
        ('retroarch/', usb_retroarch or (root / 'retroarch')),
        ('UpdateRoms/', root / 'UpdateRoms' / 'UpdateRoms.exe'),
    ):
        state = 'present' if p.exists() else ('would exist' if dry_run else 'MISSING')
        print(f'  {label:<16} {state}')


def confirm(prompt: str, default: bool) -> bool:
    suffix = ' [Y/n] ' if default else ' [y/N] '
    ans = input(prompt + suffix).strip().lower()
    if not ans:
        return default
    return ans in ('y', 'yes')


# --------------------------------------------------------------------------------------------------
# final readiness report
# --------------------------------------------------------------------------------------------------

def report_readiness(root: Path):
    print()
    print('=== is this drive ready for the PS Classic? ===')
    exploit = [c for c in root.iterdir() if UUID_RE.match(c.name)] if root.is_dir() else []
    if exploit:
        print(f'  [ok]   boot-exploit folder present ({exploit[0].name})')
    else:
        print('  [WARN] no boot-exploit folder found - this drive is not jailbroken; the console will not '
              'boot AutoBleem from it until the exploit is (re)applied. This tool does not create it.')

    binary = find_ci(root, 'Autobleem')
    binary = (binary / 'bin' / 'autobleem' / 'autobleem-gui') if binary else None
    if binary and binary.is_file():
        print(f'  [ok]   Autobleem/bin/autobleem/autobleem-gui present ({human(binary.stat().st_size)})')
    else:
        print('  [WARN] no autobleem-gui binary found')

    games_dir = find_ci(root, 'Games')
    n_games = 0
    if games_dir:
        n_games = sum(1 for c in games_dir.iterdir()
                       if c.is_dir() and not c.name.startswith('!'))
    print(f'  [info] {n_games} game folder(s) under Games/')

    retro = find_ci(root, 'retroarch')
    print(f'  [info] RetroBoot/RetroArch tree: {"present" if retro else "not installed"}')


# --------------------------------------------------------------------------------------------------
# main
# --------------------------------------------------------------------------------------------------

def parse_args(argv):
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--drive', help='USB drive path (e.g. F:\\ or /media/you/AUTOBLEEM); prompted for if omitted')
    mode = p.add_mutually_exclusive_group()
    mode.add_argument('--dry-run', action='store_true', help='preview only, changes nothing')
    mode.add_argument('--execute', action='store_true', help='actually modify the drive')
    p.add_argument('--yes', action='store_true', help='skip the "are you sure" confirmation (still needs --execute)')
    p.add_argument('--stage', choices=['analyze', 'clean', 'install', 'all'], default='all')

    p.add_argument('--keep-apps', action='store_true', help='leave all of Apps/ as-is, including pscbios/abflashkit')
    p.add_argument('--remove-all-apps', action='store_true', help='delete the whole old Apps/ folder, not just pscbios/abflashkit')
    p.add_argument('--remove-retroarch-tree', action='store_true', help='delete the whole retroarch/ tree instead of just its two PS1 playlists (AutoBleem.lpl, Sony - PlayStation.lpl)')
    p.add_argument('--keep-autobleem-playlists', action='store_true', help="leave retroarch/playlists/AutoBleem.lpl and 'Sony - PlayStation.lpl' alone")
    p.add_argument('--remove-boot-exploit', action='store_true', help='also delete the boot-exploit payload folder (you will need to redo the jailbreak)')

    retro = p.add_mutually_exclusive_group()
    retro.add_argument('--with-retroboot', dest='with_retroboot', action='store_true', default=None,
                        help='install the RetroBoot/RetroArch bundle (skipped if already present, unless --replace-retroboot)')
    retro.add_argument('--without-retroboot', dest='with_retroboot', action='store_false',
                        help="don't install RetroBoot/RetroArch even if missing")
    p.add_argument('--replace-retroboot', action='store_true', help='replace an existing retroarch/ tree with --with-retroboot')
    p.add_argument('--retroboot-source', type=Path, help='source retroarch/ tree for the RetroBoot bundle (default vendor/retroboot-bundle/retroarch)')

    p.add_argument('--skip-roms-convert', action='store_true', help="don't rename roms/<old short name> onto roms/<RetroArch database name> (see ROMS_LAYOUT_MAP)")
    p.add_argument('--skip-updateroms', action='store_true', help="don't copy UpdateRoms/ onto the drive's root")
    p.add_argument('--updateroms-source', type=Path, help='source UpdateRoms/ folder (default build_win/UpdateRoms, from tools/make_updateroms_bundle.sh)')

    p.add_argument('--repo-root', type=Path, default=REPO_ROOT, help='override the AutoBleem2 checkout used as the source for install')
    return p.parse_args(argv)


def main(argv=None):
    global REPO_ROOT
    opts = parse_args(argv)
    REPO_ROOT = opts.repo_root
    opts.interactive = sys.stdin.isatty() and sys.stdout.isatty()

    drive = opts.drive or (prompt_for_drive() if opts.interactive else None)
    if not drive:
        print('No --drive given and not running interactively.', file=sys.stderr)
        return 2
    root = Path(drive)
    if not root.is_dir():
        print(f'Not a directory: {root}', file=sys.stderr)
        return 2
    root = root.resolve()

    dry_run = opts.dry_run
    if not opts.dry_run and not opts.execute:
        if opts.interactive:
            dry_run = not confirm(f'Preview only, or actually apply changes to {root}?'.replace('?', ' (apply)?'), default=False)
        else:
            print('Specify --dry-run to preview or --execute to actually modify the drive.', file=sys.stderr)
            return 2

    try:
        total, used, free = shutil.disk_usage(root)
        print(f'Target: {root}  ({human(free)} free of {human(total)})')
    except OSError:
        print(f'Target: {root}')
    print('Mode:   DRY RUN - nothing will be changed' if dry_run else 'Mode:   EXECUTE - the drive will be modified')
    print()

    if not dry_run and not opts.yes and opts.interactive:
        if not confirm(f'About to modify {root}. Continue?', default=False):
            print('Aborted.')
            return 1

    if opts.stage == 'analyze':
        analyze(root, opts)
        return 0

    if opts.stage in ('clean', 'all'):
        stage_clean(root, opts, dry_run)
        print()

    if opts.stage in ('install', 'all'):
        stage_install(root, opts, dry_run)

    if not dry_run:
        report_readiness(root)
    return 0


if __name__ == '__main__':
    sys.exit(main())
