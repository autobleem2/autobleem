#!/usr/bin/env python3
"""The user manual's screenshots, taken in a VM sandbox (tools/vm/abvm.py) from a release package - the look a user
of that release gets (its launcher, themes, extensions, VERSION), 50 fake games named from the cover DBs, the
launcher in each manual language. One drive per language; the shots go as JPEGs into an autobleem-manuals checkout's
manuals/images/<lang>/, replacing only the pictures that folder already has (--add-new writes the rest too):

    python tools/manual_shots.py --package '~/autobleem-pcusb-i386-v2.0.0-alpha1.tar.gz'
    python tools/manual_shots.py --package '~/...tar.gz' --lang pl --lang fi --manuals ../autobleem-manuals

--package is a path ON THE TEST MACHINE (quote it, so the shell here does not expand ~ or rewrite the path): copy
the PC-stick package there first (the site's download, or the release folder). --sandbox (default manual-shots) is
leased for the run and removed after it (--keep leaves it, still leased, for a look). The shots a PC-stick sandbox
cannot show (NOT_HERE below) stay as they are - they are taken on a console or a Pi.
"""
import argparse
import glob
import os
import shutil
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)
ABVM = [sys.executable, os.path.join(HERE, 'vm', 'abvm.py')]

# a manual's language folder -> the launcher's Language= name (src/resources/lang/<name>.txt)
LANGS = {'en': 'English', 'pl': 'Polski', 'da': 'Danish', 'fi': 'Finnish'}
GAMES = 50

# name -> the driver steps that reach it from where the previous one left off (the shot is added after each).
# `home` = back to the carousel; the system menu opens with L2 + R2; `quick <item>` = the Quick menu's item.
IDLE = 'wait_idle 300 10'
SCREENS = [
    ('launcher', f'wait 3500; {IDLE}'),
    ('launcher-icons', f'tap down; {IDLE}'),
    ('launcher-icons-game', f'tap right; {IDLE}'),
    ('game-editor', f'tap x; wait_screen GuiEditor; {IDLE}'),
    ('memory-card-editor', f'home; tap right; {IDLE}; tap x; wait 1500; {IDLE}'),
    ('quick-menu', f'home; tap up; {IDLE}; tap up; wait 800; {IDLE}'),
    ('set-picker', f'tap o; {IDLE}; tap select; wait_screen GuiSetPicker; {IDLE}'),
    ('set-picker-apps', f'tap r1; tap r1; {IDLE}'),
    ('launcher-apps', f'tap x; wait 1500; {IDLE}'),
    ('app-start', f'tap x; wait_screen GuiAppStart; {IDLE}'),
    ('button-guide', f'tap o; {IDLE}; tap select; wait_screen GuiSetPicker; tap l1; tap l1; {IDLE}; tap x; '
                     f'wait 1500; {IDLE}; tap t; wait 800; {IDLE}'),
    ('system-menu', f'tap o; {IDLE}; down l2; down r2; wait 200; up r2; up l2; wait_screen GuiSystemMenu; wait 800'),
    ('options', f'tap o; {IDLE}; menu Options; wait_screen GuiOptions; wait 1000'),
    ('game-manager', 'home; menu Game Manager; wait_screen GuiManager; wait 1000'),
    ('memory-cards', f'home; menu Memory Cards; wait 1000; {IDLE}'),
    ('keyboard', f'tap s; wait_screen GuiKeyboard; {IDLE}'),
    ('hardware-info', f'home; menu Hardware Information; wait 1500; {IDLE}'),
    ('extensions', f'home; menu Extensions; wait 1000; {IDLE}'),
    ('processors', f'home; menu Scanner processors; wait 1000; {IDLE}'),
    ('about', 'home; menu About; wait_screen GuiAbout; wait 2000'),
    ('store-apps', 'home; quick Store; wait 8000'),
    ('store-source-menu', 'tap select; wait 1500'),
]
WARM_UP = f'wait 5000; {IDLE}; wait 25000'
# the manual's pictures this sandbox cannot take, and why
NOT_HERE = {
    'pscbios-main': 'PSC-Bios needs console hardware (a Pi or the console)',
    'pscbios-network': 'PSC-Bios needs console hardware',
    'pscbios-gamepads': 'PSC-Bios needs console hardware',
    'pscbios-wizard': 'PSC-Bios needs console hardware',
    'abflashkit-menu': 'ABFlashKit runs on the console only',
    'abflashkit-progress': 'ABFlashKit runs on the console only',
    'abflashkit-warning': 'ABFlashKit runs on the console only',
    'lanshare': 'a Windows program',
    'set-picker-retroarch': 'the RetroArch tab needs RetroArch installed',
    'rescan': 'the scan is over before a shot',
    'store-sources': 'needs a user source added to the Store',
}


def abvm(who, *args, check=True):
    r = subprocess.run(ABVM + ['--who', who] + list(args), text=True, capture_output=True)
    if check and r.returncode != 0:
        sys.exit('abvm %s: %s' % (' '.join(args[:2]), (r.stderr or r.stdout).strip()))
    return r


def script():
    return '; '.join('%s; shot %s.png' % (steps, name) for name, steps in SCREENS)


def to_jpegs(shots, target, add_new):
    from PIL import Image
    have = {os.path.splitext(f)[0] for f in os.listdir(target) if f.endswith('.jpg')}
    done = []
    for png in sorted(glob.glob(os.path.join(shots, '*.png'))):
        name = os.path.splitext(os.path.basename(png))[0]
        if name in have or add_new:
            Image.open(png).convert('RGB').save(os.path.join(target, name + '.jpg'), quality=86, optimize=True)
            done.append(name)
    return done, sorted(have - set(done))


def main(argv):
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--package', required=True, help='the PC-stick release package, a path on the test machine')
    ap.add_argument('--lang', action='append', choices=sorted(LANGS), help='a manual language (default: all)')
    ap.add_argument('--manuals', default=os.path.join(REPO, '..', 'autobleem-manuals'),
                    help='the autobleem-manuals checkout (default: the sibling one)')
    ap.add_argument('--sandbox', default='manual-shots')
    ap.add_argument('--who', default=os.environ.get('ABVM_WHO', 'manual-shots'), help='the lease holder name')
    ap.add_argument('--add-new', action='store_true', help='also write the shots a language folder does not have')
    ap.add_argument('--keep', action='store_true', help='leave the sandbox (and its lease) after the run')
    args = ap.parse_args(argv)
    images = os.path.join(args.manuals, 'manuals', 'images')
    if not os.path.isdir(images):
        sys.exit('%s: no manuals/images - give the autobleem-manuals checkout with --manuals' % args.manuals)
    sb = args.sandbox
    abvm(args.who, 'sandbox', 'take', sb, 'manual screenshots', '120')
    work = tempfile.mkdtemp(prefix='manual-shots-')
    try:
        for code in args.lang or sorted(LANGS):
            target = os.path.join(images, code)
            os.makedirs(target, exist_ok=True)
            print('== %s (%s)' % (code, LANGS[code]))
            # afresh per language: the template, the release over it, the games, the language
            abvm(args.who, 'sandbox', 'reset', sb, '--package', args.package, '--games', str(GAMES),
                 '--set', 'Language=' + LANGS[code])
            # the first start scans the games and fetches the covers the DBs lack (a welcome card, bubbles):
            # let it finish, then start again on a scanned shelf
            abvm(args.who, 'sandbox', 'start', sb)
            abvm(args.who, 'sandbox', 'drive', sb, WARM_UP, check=False)
            abvm(args.who, 'sandbox', 'stop', sb)
            abvm(args.who, 'sandbox', 'start', sb)
            shots = os.path.join(work, code)
            r = abvm(args.who, 'sandbox', 'drive', sb, script(), '--out', shots, check=False)
            if r.returncode != 0:
                print('  !! the drive stopped: %s' % (r.stderr or r.stdout).strip().splitlines()[-1:])
            os.makedirs(shots, exist_ok=True)
            done, old = to_jpegs(shots, target, args.add_new)
            print('  %d replaced: %s' % (len(done), ', '.join(done)))
            missed = [n for n in old if n not in NOT_HERE]
            if missed:
                print('  !! not taken: %s' % ', '.join(missed))
            kept = [n for n in old if n in NOT_HERE]
            if kept:
                print('  kept (not on a PC stick): %s' % ', '.join(kept))
    finally:
        abvm(args.who, 'sandbox', 'stop', sb, check=False)
        if not args.keep:
            abvm(args.who, 'sandbox', 'rm', sb, check=False)
        shutil.rmtree(work, ignore_errors=True)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
