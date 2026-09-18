#!/usr/bin/env python3
"""
The language files: src/resources/lang/<Language>.txt, one "English text=Translated text" per line.

    python tools/lang_tools.py extract              # English.txt from every _("...") in src/code
    python tools/lang_tools.py update               # every other file gets English.txt's keys (missing -> empty)
    python tools/lang_tools.py update --remove-obsolete
    python tools/lang_tools.py validate [FILE...]   # format, duplicates, keys not in English.txt; exit 1 on a problem
    python tools/lang_tools.py compare Polski       # what a language still lacks
    python tools/lang_tools.py convert FILE...      # the old pairs-of-lines layout -> key=value, in place
    python tools/lang_tools.py merge DIR [--map Deutsch=German ...]
                                                    # fill empty values from another tree's files (AutoBleem-NG's)

The file: a "# ..." header (the loader takes a leading comment as the mark of this layout), then the keys
sorted, "Key=" for a string not translated yet - the app shows the English then. "|@lang|" is a key like
any other: its value is the language code PCSX is started with. From AutoBleem-NG's scripts/lang_tools.py.
"""

import argparse
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
SRC_DIR = REPO / 'src' / 'code'
LANG_DIR = REPO / 'src' / 'resources' / 'lang'
SOURCE = 'English'


def extract_strings(src_dir: Path) -> set:
    """every _("...") in the C++ sources: a literal, or adjacent literals the way clang-format splits a long
    one ("..." "..."), joined; a _( inside another identifier (runLines_("x")) is not one; comments skipped"""
    call = re.compile(r'(?<![A-Za-z0-9_])_\(\s*((?:"(?:[^"\\]|\\.)*"\s*)+)\)')
    literal = re.compile(r'"((?:[^"\\]|\\.)*)"')
    strings = set()
    for ext in ('*.cpp', '*.h'):
        for path in src_dir.rglob(ext):
            lines = []
            for line in path.read_text(encoding='utf-8', errors='replace').splitlines():
                cut = line.find('//')
                lines.append(line if cut == -1 else line[:cut])
            for pieces in call.findall('\n'.join(lines)):
                text = ''.join(literal.findall(pieces)).replace('\\"', '"')
                if text:
                    strings.add(text)
    return strings


def read_pairs(path: Path) -> dict:
    """the old layout: the source line, then its translation"""
    lines = [l.rstrip('\r\n') for l in path.read_text(encoding='utf-8-sig').split('\n')]
    out = {}
    for i in range(0, len(lines) - 1, 2):
        key = lines[i].strip()
        if key:
            out[key] = lines[i + 1].strip()
    return out


def is_key_value(path: Path) -> bool:
    for line in path.read_text(encoding='utf-8-sig').splitlines():
        if line.strip():
            return line.lstrip().startswith('#')
    return True


def read_key_value(path: Path) -> dict:
    out = {}
    for line in path.read_text(encoding='utf-8-sig').splitlines():
        line = line.strip()
        if not line or line.startswith('#') or '=' not in line:
            continue
        key, value = line.split('=', 1)
        if key.strip():
            out[key.strip()] = value.strip()
    return out


def read_any(path: Path) -> dict:
    return read_key_value(path) if is_key_value(path) else read_pairs(path)


def write_key_value(path: Path, translations: dict, language: str, total: int = None):
    untranslated = sum(1 for k, v in translations.items() if not v)
    total = total if total is not None else len(translations)
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(f'# AutoBleem {language} Translation\n')
        f.write('# Format: English Text=Translated Text\n')
        f.write(f'# Untranslated: {untranslated} / Total: {total}\n\n')
        for key in sorted(translations, key=str.lower):
            f.write(f'{key}={translations[key]}\n')


def language_files(lang_dir: Path):
    return sorted(p for p in lang_dir.glob('*.txt') if p.stem != SOURCE)


def cmd_extract(args):
    strings = extract_strings(Path(args.src_dir))
    path = Path(args.lang_dir) / f'{SOURCE}.txt'
    write_key_value(path, {s: s for s in strings}, SOURCE)
    print(f'{path}: {len(strings)} strings')
    return 0


def cmd_update(args):
    lang_dir = Path(args.lang_dir)
    english = read_key_value(lang_dir / f'{SOURCE}.txt')
    keys = set(english)
    for path in language_files(lang_dir):
        old = read_any(path)
        new = {k: old.get(k, '') for k in keys}
        if not args.remove_obsolete:
            for k, v in old.items():
                if k not in keys:
                    new[k] = v
        write_key_value(path, new, path.stem, len(keys))
        done = sum(1 for k in keys if new.get(k))
        print(f'{path.stem:16} {done:3}/{len(keys)} translated, +{len(keys - set(old))} added, '
              f'{len(set(old) - keys)} obsolete{"" if args.remove_obsolete else " kept"}')
    return 0


def cmd_convert(args):
    for name in args.files:
        path = Path(name)
        if is_key_value(path):
            print(f'{path}: already key=value')
            continue
        pairs = read_pairs(path)
        write_key_value(path, pairs, path.stem)
        print(f'{path}: {len(pairs)} strings converted')
    return 0


def cmd_validate(args):
    lang_dir = Path(args.lang_dir)
    english = read_key_value(lang_dir / f'{SOURCE}.txt')
    files = [Path(f) for f in args.files] or language_files(lang_dir)
    problems = 0
    for path in files:
        seen = set()
        for n, raw in enumerate(path.read_text(encoding='utf-8-sig').splitlines(), 1):
            line = raw.strip()
            if not line or line.startswith('#'):
                continue
            if '=' not in line:
                print(f'{path}:{n}: no "=": {raw}')
                problems += 1
                continue
            key = line.split('=', 1)[0].strip()
            if not key:
                print(f'{path}:{n}: empty key')
                problems += 1
            elif key in seen:
                print(f'{path}:{n}: duplicate key: {key}')
                problems += 1
            elif key not in english:
                print(f'{path}:{n}: not in {SOURCE}.txt: {key}')
                problems += 1
            seen.add(key)
            if raw != raw.rstrip():
                print(f'{path}:{n}: trailing whitespace')
                problems += 1
        missing = set(english) - seen
        if missing:
            print(f'{path}: {len(missing)} keys missing - run update')
            problems += 1
    print('ok' if problems == 0 else f'{problems} problems')
    return 0 if problems == 0 else 1


def cmd_compare(args):
    lang_dir = Path(args.lang_dir)
    english = read_key_value(lang_dir / f'{SOURCE}.txt')
    lang = read_any(lang_dir / f'{args.language}.txt')
    todo = [k for k in sorted(english, key=str.lower) if not lang.get(k)]
    for k in todo:
        print(k)
    print(f'{len(english) - len(todo)}/{len(english)} translated, {len(todo)} to do', file=sys.stderr)
    return 0


def cmd_merge(args):
    lang_dir = Path(args.lang_dir)
    other = Path(args.dir)
    names = dict(m.split('=', 1) for m in args.map)
    for path in language_files(lang_dir):
        src = other / f'{names.get(path.stem, path.stem)}.txt'
        if not src.exists():
            print(f'{path.stem:16} no {src.name} there')
            continue
        theirs = read_any(src)
        ours = read_any(path)
        filled = 0
        for k, v in ours.items():
            if not v and theirs.get(k) and theirs[k] != k:
                ours[k] = theirs[k]
                filled += 1
        write_key_value(path, ours, path.stem)
        print(f'{path.stem:16} {filled} filled from {src.name}')
    return 0


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--src-dir', default=str(SRC_DIR))
    parser.add_argument('--lang-dir', default=str(LANG_DIR))
    sub = parser.add_subparsers(dest='command', required=True)
    sub.add_parser('extract').set_defaults(func=cmd_extract)
    p = sub.add_parser('update'); p.add_argument('--remove-obsolete', action='store_true'); p.set_defaults(func=cmd_update)
    p = sub.add_parser('convert'); p.add_argument('files', nargs='+'); p.set_defaults(func=cmd_convert)
    p = sub.add_parser('validate'); p.add_argument('files', nargs='*'); p.set_defaults(func=cmd_validate)
    p = sub.add_parser('compare'); p.add_argument('language'); p.set_defaults(func=cmd_compare)
    p = sub.add_parser('merge'); p.add_argument('dir'); p.add_argument('--map', action='append', default=[]); p.set_defaults(func=cmd_merge)
    args = parser.parse_args()
    return args.func(args)


if __name__ == '__main__':
    sys.exit(main())
