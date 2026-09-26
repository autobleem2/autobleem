#!/usr/bin/env python3
"""The user manual's screenshots, taken through the DebugDriver (tools/ab_drive.py) on the Windows dev build:
the launcher and the two console tools walked through their screens, one JPEG per screen per language into
manuals/images/<lang>/. Run after `make_win.sh` and `python tools/make_usb.py usb --games 50`:

    python tools/manual_shots.py                # English and Polish
    python tools/manual_shots.py --lang English --show
    python tools/manual_shots.py --only keyboard,store-apps   # just these screens

The Store's screens need the Store built (-DAB_EXTENSION_DIRS=<ext_store>) and staged by make_usb.py; they show
the published rpi catalog (AB_STORE_CATALOG, overridable) and the stick's own sources
(usb/System/Extensions/store/sources.txt) as they are.

The stick's config.ini (usb/Autobleem/bin/autobleem/config.ini) is switched to each language for its run and
put back to English. The UpdateRoms and installer windows are plain Win32 programs without the driver: those
two are captured by window title (PowerShell, PrintWindow) when they are running, else skipped.
"""
import os
import re
import subprocess
import sys
import time

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
USB = os.path.join(REPO, 'usb')
OUT = os.path.join(REPO, 'manuals', 'images')
DRIVE = [sys.executable, os.path.join(REPO, 'tools', 'ab_drive.py')]
PORT = '7795'

LANGS = {'en': 'English', 'pl': 'Polski'}

# the launcher's screens: name -> the driver script that reaches it (the shot is taken by the caller)
LAUNCHER = [
    ('launcher', 'wait 800'),
    ('launcher-icons', 'press down; wait 500'),
    ('launcher-icons-game', 'press right; wait 400'),
    ('set-picker', 'press o; wait 400; press select; wait_screen GuiSetPicker; wait 400'),
    ('set-picker-retroarch', 'press r1; wait 400'),
    ('set-picker-apps', 'press r1; wait 400'),
    ('system-menu', 'press o; wait 400; down l2; press r2; up l2; wait_screen GuiSystemMenu; wait 400'),
    ('options', 'press o; wait 300; menu options; wait_screen GuiOptions; wait 400'),
    ('game-editor', 'press o; wait 500; press down; wait 400; press right; wait 300; press x; wait_screen GuiEditor; wait 500'),
    ('memory-card-editor', 'press o; wait 400; press right; wait 300; press x; wait 1200'),
    ('memory-cards', 'press o; wait 500; press o; wait 400; menu memory; wait 500'),
    ('game-manager', 'press o; wait 400; menu game; wait_screen GuiManager; wait 500'),
    ('hardware-info', 'press o; wait 400; menu hardware; wait 600'),
    # the system menu's items after Extensions: Scanner processors (7), Software Update (8), About (9)
    ('about', 'press o; wait 400; menu about; wait_screen GuiAbout; wait 500'),
    ('button-guide', 'press o; wait 400; press t; wait 600'),
    ('keyboard', 'press o; wait 400; menu memory; wait 400; press s; wait_screen GuiKeyboard; wait 400'),
    ('app-start', 'press o; wait 300; press o; wait 300; press select; wait_screen GuiSetPicker; press r1; press r1; wait 300; press x; wait 900; press x; wait_screen GuiAppStart; wait 500'),
    ('launcher-apps', 'press o; wait 500'),
    ('rescan', 'menu re-scan; wait 700'),
    # the Extensions list and the AutoBleem Store
    ('extensions', 'press o; wait 300; press o; wait 300; press o; wait 1500; menu extensions; wait_screen GuiExtensions; wait 400'),
    # the scanner processors' sequences: copy proc_unzip's folder into usb/System/Processors/unzip first
    ('processors', 'press o; wait 300; press o; wait 300; press o; wait 1500; menu scanner; wait_screen GuiProcessors; wait 400'),
    ('store-apps', 'wait 500; press x; wait_screen GuiStore; wait 3000'),
    ('store-sources', 'press l1; wait 1500'),
    ('store-source-menu', 'press up; press up; press x; wait_screen GuiActionMenu; wait 400'),
]
# the catalog the Store's shots show: the published one with the Apps in it
STORE_CATALOG = 'https://autobleem.retromenele.pl/store/rpi/catalog.json'

PSCBIOS = [
    # PSC-Bios is an extension (2026-09-24); the launcher's Network & Controllers item opens its hub
    # (2026-09-26), whose rows are, in order: Wi-Fi network, Bluetooth controllers, DualShock 3 pairing,
    # Controller mapping. Each shot starts where the one before left off.
    ('pscbios-main', 'menu network; wait_screen GuiActionMenu; wait 600'),
    # GuiNetworkMenu, or a GuiTextPage on a machine without abnet/nmcli - so a plain wait
    ('pscbios-network', 'press x; wait 800'),
    ('pscbios-bluetooth', 'press o; wait_screen GuiActionMenu; press down; press x; wait_screen GuiBtPairing; wait 400'),
    ('pscbios-ds3', 'press o; wait_screen GuiActionMenu; press down; press x; wait_screen GuiTextPage; wait 400'),
    ('pscbios-wizard', 'press o; wait_screen GuiActionMenu; press down; press x; wait_screen GuiPadConfig; wait 1200'),
]
ABFLASHKIT = [
    ('abflashkit-warning', 'wait 500'),
    ('abflashkit-menu', 'press x; wait_screen GuiActionMenu; wait 400'),
    ('abflashkit-progress', 'press down; press x; wait 600; press x; wait 2500'),
]


def run(args, check=True):
    return subprocess.run(args, check=check, text=True, capture_output=True)


def set_language(name):
    path = os.path.join(USB, 'Autobleem', 'bin', 'autobleem', 'config.ini')
    text = open(path, encoding='utf-8').read()
    text = re.sub(r'(?im)^language=.*$', 'Language=' + name, text)
    open(path, 'w', encoding='utf-8', newline='\n').write(text)


def to_jpeg(png, jpg):
    from PIL import Image
    Image.open(png).convert('RGB').save(jpg, quality=86, optimize=True)
    os.remove(png)


def shoot(tool, screens, out_dir, show):
    if not screens:
        return
    start = DRIVE + ['start', '--port', PORT] + (['--show'] if show else []) + (['--tool', tool] if tool else [])
    os.environ.setdefault('AB_STORE_CATALOG', STORE_CATALOG)
    run(start)
    try:
        for name, script in screens:
            png = os.path.join(out_dir, name + '.png')
            r = run(DRIVE + ['run', '--port', PORT, script + '; shot ' + png], check=False)
            if r.returncode != 0 or not os.path.exists(png):
                print('  !! %s: %s' % (name, (r.stdout + r.stderr).strip().splitlines()[-1:]))
                continue
            to_jpeg(png, os.path.join(out_dir, name + '.jpg'))
            print('  ' + name)
    finally:
        run(DRIVE + ['stop', '--port', PORT], check=False)


def capture_window(title_part, out):
    """a plain Win32 window by (part of) its title, through PowerShell's PrintWindow; False when not found"""
    script = r'''
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class W { [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr dc, uint f);
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out R r);
  [StructLayout(LayoutKind.Sequential)] public struct R { public int L, T, Ri, B; } }
"@
$p = Get-Process | Where-Object { $_.MainWindowTitle -like "*%s*" } | Select-Object -First 1
if (-not $p) { exit 3 }
$h = $p.MainWindowHandle
$r = New-Object W+R; [W]::GetWindowRect($h, [ref]$r) | Out-Null
$bmp = New-Object System.Drawing.Bitmap(($r.Ri - $r.L), ($r.B - $r.T))
$g = [System.Drawing.Graphics]::FromImage($bmp)
$dc = $g.GetHdc(); [W]::PrintWindow($h, $dc, 2) | Out-Null; $g.ReleaseHdc($dc)
$bmp.Save("%s", [System.Drawing.Imaging.ImageFormat]::Png)
''' % (title_part, out.replace('\\', '\\\\'))
    r = subprocess.run(['powershell', '-NoProfile', '-Command', script], capture_output=True, text=True)
    return r.returncode == 0 and os.path.exists(out)


def main(argv):
    langs = dict(LANGS)
    show = '--show' in argv
    only = set(argv[argv.index('--only') + 1].split(',')) if '--only' in argv else None

    def pick(screens):
        return [sc for sc in screens if only is None or sc[0] in only]

    if '--lang' in argv:
        name = argv[argv.index('--lang') + 1]
        langs = {k: v for k, v in LANGS.items() if v == name}
    for code, name in langs.items():
        out_dir = os.path.join(OUT, code)
        os.makedirs(out_dir, exist_ok=True)
        print('== %s' % name)
        set_language(name)
        shoot(None, pick(LAUNCHER), out_dir, show)
        shoot(None, pick(PSCBIOS), out_dir, show)
        shoot('abflashkit', pick(ABFLASHKIT), out_dir, show)
    set_language('English')
    if only is not None:
        return 0
    # the two Win32 programs, when the owner has them open
    shared = os.path.join(OUT, 'shared')
    os.makedirs(shared, exist_ok=True)
    for title, name in (('UpdateRoms', 'updateroms'), ('AutoBleem Installer', 'installer')):
        png = os.path.join(shared, name + '.png')
        if capture_window(title, png):
            to_jpeg(png, os.path.join(shared, name + '.jpg'))
            print('  ' + name + ' (window)')
        else:
            print('  (no %s window open - skipped)' % title)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv[1:]))
