#!/usr/bin/env python3
"""A screenshot of every screen the launcher can show, through the DebugDriver (tools/ab_drive.py):

  python tools/ui_tour.py OUTDIR [--show] [--port N]

Starts the dev build on usb/ (a tree from `tools/make_usb.py usb --games 30` has enough in it to open
everything), walks the screens, writes <OUTDIR>/<nn>-<screen>.png for each and a contact sheet
<OUTDIR>/tour.png, and stops the launcher. The walk: the launcher (Games and the icon row), the set picker's
three tabs, the system menu, Options, the Game Manager and its two confirms, Memory Cards with the New Card
keyboard, Hardware Information (two pages), About and the surprise game, the PS1 game editor with its
rename keyboard and memory card picker, the memory card manager, the button guide, a RetroArch game's editor,
the Apps set and an app's start screen.
"""
import os
import subprocess
import sys
import time

REPO = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
sys.path.insert(0, os.path.join(REPO, 'tools'))
import ab_drive  # noqa: E402


def main(argv):
    if len(argv) < 2:
        print(__doc__)
        return 2
    out = os.path.abspath(argv[1])
    os.makedirs(out, exist_ok=True)
    show = '--show' in argv
    port = int(argv[argv.index('--port') + 1]) if '--port' in argv else 7789
    subprocess.check_call([sys.executable, os.path.join(REPO, 'tools', 'ab_drive.py'), 'start', '--port', str(port)]
                          + (['--show'] if show else []))
    d = ab_drive.Driver(port)
    shots = []
    n = [0]

    def shot(name):
        n[0] += 1
        path = os.path.join(out, '%02d-%s.png' % (n[0], name))
        d.cmd('shot ' + path)
        shots.append(path)

    def run(script):
        d.run(script)

    try:
        # the launcher
        run('wait 800')
        shot('launcher')
        run('press down; wait 400')
        shot('launcher-icon-row')
        run('press up; wait 400')
        # the set picker, each tab
        run('press select; wait_screen GuiSetPicker; wait 200')
        shot('set-picker-playstation')
        run('press r1; wait 200')
        shot('set-picker-retroarch')
        run('press r1; wait 200')
        shot('set-picker-apps')
        run('press o; wait_screen GuiLauncher')
        # the system menu
        run('down l2; press r2; up l2; wait_screen GuiSystemMenu; wait 200')
        shot('system-menu')
        run('press o; wait_screen GuiLauncher')
        # Options
        run('menu 5; wait_screen GuiOptions; wait 200')
        shot('options')
        run('press o; wait_screen GuiLauncher')
        # the Game Manager and its confirms
        run('menu 3; wait_screen GuiManager; wait 300')
        shot('game-manager')
        run('press s; wait_screen GuiConfirm; wait 200')
        shot('confirm-delete-game')
        run('press o; wait 200; press t; wait_screen GuiConfirm; wait 200')
        shot('confirm-flush-covers')
        run('press o; wait 200; press o; wait_screen GuiLauncher')
        # Memory Cards, the New Card keyboard
        run('menu 2; wait_screen GuiMemcards; wait 200')
        shot('memory-cards')
        run('press s; wait_screen GuiKeyboard; wait 200')
        shot('keyboard-new-card')
        run('press o; wait 200; press o; wait_screen GuiLauncher')
        # Hardware Information
        run('menu 4; wait_screen GuiHardwareInfo; wait 300')
        shot('hardware-info')
        run('press r2; wait 300')
        shot('hardware-info-page-2')
        run('press o; wait_screen GuiLauncher')
        # About and the surprise game
        run('menu 7; wait_screen GuiAbout; wait 300')
        shot('about')
        run('press start; wait 1500')
        shot('surprise-game')
        run('press start; wait 300; press o; wait_screen GuiLauncher')
        # the PS1 game editor, its rename keyboard and memory card picker
        run('press down; wait 400; press right; wait 300; press x; wait_screen GuiEditor; wait 200')
        shot('game-editor-ps1')
        run('press t; wait_screen GuiKeyboard; wait 200')
        shot('keyboard-rename')
        run('press o; wait 200; press s; wait 500')
        if d.cmd('screen').endswith('GuiSelectMemcard'):
            shot('select-memcard')
            run('press o; wait 200')
        run('press o; wait 300; press o; wait_screen GuiLauncher')
        # the memory card manager
        run('wait 600; press down; wait 500; press right; wait 300; press right; wait 300; press x; wait 1500')
        if d.cmd('screen').endswith('GuiMcManager'):
            shot('memcard-manager')
            run('press o; wait 400')
        run('wait_screen GuiLauncher')
        # the button guide
        run('press t; wait_screen GuiBtnGuide; wait 300')
        shot('button-guide')
        run('press o; wait_screen GuiLauncher')
        # a RetroArch game's editor
        run('press select; wait_screen GuiSetPicker; press r1; wait 200; press x; wait_screen GuiLauncher; wait 800')
        shot('launcher-retroarch')
        run('press down; wait 400; press right; wait 300; press x; wait 800')
        if d.cmd('screen').endswith('GuiEditorRA'):
            shot('game-editor-retroarch')
            run('press o; wait 300')
        run('wait_screen GuiLauncher')
        # the Apps set and an app's start screen (the tabs wrap: from RetroArch one to the right)
        run('press select; wait_screen GuiSetPicker; press r1; wait 200; press x; wait_screen GuiLauncher; wait 800')
        shot('launcher-apps')
        run('press x; wait 800')
        if d.cmd('screen').endswith('GuiAppStart'):
            shot('app-start')
            run('press o; wait 300')
        run('wait_screen GuiLauncher')
    finally:
        d.close()
        subprocess.call([sys.executable, os.path.join(REPO, 'tools', 'ab_drive.py'), 'stop', '--port', str(port)])
    ab_drive.sheet(os.path.join(out, 'tour.png'), shots, columns=3, width=426)
    print(len(shots), 'screens in', out)
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
