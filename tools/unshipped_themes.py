"""The theme folders of autobleem-themes that no package ships: tools/unshipped_themes.txt, one name per line.

A theme folder in the themes repository does not mean it ships - make_usb.py and install_autobleem.py skip
what this lists (the shell packaging scripts use tools/unshipped_themes.sh on the same file).
"""
import os

LIST = os.path.join(os.path.dirname(os.path.abspath(__file__)), 'unshipped_themes.txt')


def names():
    with open(LIST, encoding='utf-8') as f:
        lines = [line.strip() for line in f]
    return {line for line in lines if line and not line.startswith('#')}
