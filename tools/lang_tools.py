#!/usr/bin/env python3
"""
The language files: src/resources/lang/<Language>.txt, one "English text=Translated text" per line.

    python tools/lang_tools.py add "English text" ["Deutsch=Text" ...]   # THE way to add a string: inserts only
    python tools/lang_tools.py add --from FILE      # that key, in its sorted place, into every file; FILE lines are
                                                    # key<TAB>Language=value[<TAB>Language=value...]
    python tools/lang_tools.py extract              # English.txt from every _("...") in SRC_DIRS and every
                                                    # translate("...") in TRANSLATE_DIRS (ab_gui)
    python tools/lang_tools.py update               # every other file gets English.txt's keys (missing -> empty)
    python tools/lang_tools.py update --remove-obsolete
    extract and update rewrite whole files: they stop (exit 2) when more than 20 lines of any file would change,
    unless --force; --dry-run prints the counts only. The files differ from a fresh extract, so do not add strings
    with them.
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
import difflib
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
# the launcher's own code and the shared code in the autobleem-core submodule (ab_core, ab_classic, lib_ableem),
# whose screens and services the launcher shows too; a directory that is not there (no submodule) is skipped
SRC_DIRS = [
    REPO / 'src' / 'code',
    REPO / 'src' / 'tools',  # the console helpers with a screen of their own (abdialog)
    REPO / 'autobleem-core' / 'src' / 'code',
    REPO / 'autobleem-core' / 'lib_ableem' / 'src',
]
# ab_gui (autobleem-core's UI library) has no _(): its widgets hand their English to the program's translator as
# ctx.translate("..."), and the launcher's language files hold those strings too - so without --src-dir these
# trees are scanned for translate("...") as well
TRANSLATE_DIRS = [
    REPO / 'autobleem-core' / 'ab_gui',
]
LANG_DIR = REPO / 'src' / 'resources' / 'lang'
SOURCE = 'English'


LITERAL = re.compile(r'"((?:[^"\\]|\\.)*)"')


def source_texts(src_dirs):
    """every C++ source under the trees, its // comments cut off"""
    paths = [path for src_dir in src_dirs if src_dir.is_dir()
             for ext in ('*.cpp', '*.h') for path in src_dir.rglob(ext)]
    for path in paths:
        lines = []
        for line in path.read_text(encoding='utf-8', errors='replace').splitlines():
            cut = line.find('//')
            lines.append(line if cut == -1 else line[:cut])
        yield '\n'.join(lines)


def extract_strings(src_dirs) -> set:
    """every _("...") in the C++ sources: a literal, or adjacent literals the way clang-format splits a long
    one ("..." "..."), joined; a _( inside another identifier (runLines_("x")) is not one; comments skipped"""
    call = re.compile(r'(?<![A-Za-z0-9_])_\(\s*((?:"(?:[^"\\]|\\.)*"\s*)+)\)')
    strings = set()
    for text in source_texts(src_dirs):
        for pieces in call.findall(text):
            joined = ''.join(LITERAL.findall(pieces)).replace('\\"', '"')
            if joined:
                strings.add(joined)
    return strings


def call_argument(text: str, start: int) -> str:
    """the argument of the call whose '(' is at text[start]: up to the matching ')', string literals skipped over"""
    depth, i = 0, start
    while i < len(text):
        c = text[i]
        if c == '"':
            match = LITERAL.match(text, i)
            i = match.end() if match else i + 1
            continue
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
            if depth == 0:
                return text[start + 1:i]
        i += 1
    return ''


def extract_translated(src_dirs) -> set:
    """every string an ab_gui widget hands its translator: translate("...") (adjacent literals joined, as for _()),
    and each literal of a choice between literals, translate(on ? "ON" : "OFF"); an argument that builds its text
    (a '+', a call) gives nothing - its pieces are not keys"""
    call = re.compile(r'(?<![A-Za-z0-9_])translate\(')
    strings = set()
    for text in source_texts(src_dirs):
        for match in call.finditer(text):
            argument = call_argument(text, match.end() - 1)
            literals = LITERAL.findall(argument)
            outside = LITERAL.sub('', argument)
            if not literals or '+' in outside or '(' in outside:
                continue
            if outside.strip() == '':
                keys = [''.join(literals)]
            elif '?' in outside:
                keys = literals
            else:
                continue
            for key in keys:
                key = key.replace('\\"', '"')
                if key:
                    strings.add(key)
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


def render_key_value(translations: dict, language: str, total: int = None) -> str:
    untranslated = sum(1 for k, v in translations.items() if not v)
    total = total if total is not None else len(translations)
    out = [f'# AutoBleem {language} Translation\n', '# Format: English Text=Translated Text\n',
           f'# Untranslated: {untranslated} / Total: {total}\n\n']
    # case-insensitive, then by case, so INTERNAL/Internal come out in the same order on every run
    for key in sorted(translations, key=lambda k: (k.lower(), k)):
        out.append(f'{key}={translations[key]}\n')
    return ''.join(out)


def write_key_value(path: Path, translations: dict, language: str, total: int = None):
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(render_key_value(translations, language, total))


def changed_lines(path: Path, new_text: str) -> int:
    """how many lines a rewrite would change: the larger of the lines added and removed (a replaced line is one)"""
    old = path.read_bytes().decode('utf-8').split('\n') if path.exists() else []
    diff = list(difflib.unified_diff(old, new_text.split('\n'), n=0, lineterm=''))
    added = sum(1 for l in diff if l.startswith('+') and not l.startswith('+++'))
    removed = sum(1 for l in diff if l.startswith('-') and not l.startswith('---'))
    return max(added, removed)


GUARD_LINES = 20


def apply_rewrites(rewrites, args) -> int:
    """rewrites: [(path, new text)]. The result is computed before anything is written: more than GUARD_LINES
    changed lines in any file stops with exit 2 (--force writes anyway); --dry-run only prints the counts."""
    counts = [(path, changed_lines(path, text)) for path, text in rewrites]
    for path, n in counts:
        print(f'{path.name:28} {n} lines would change')
    if args.dry_run:
        return 0
    if not args.force and any(n > GUARD_LINES for _, n in counts):
        print(f'stopped: more than {GUARD_LINES} lines would change in a file - these commands rewrite whole files '
              f'and the files differ from a fresh extract. To add strings use `lang_tools.py add`; to rewrite '
              f'anyway, --force.', file=sys.stderr)
        return 2
    for path, text in rewrites:
        with open(path, 'w', encoding='utf-8', newline='\n') as f:
            f.write(text)
    return 0


def language_files(lang_dir: Path):
    return sorted(p for p in lang_dir.glob('*.txt') if p.stem != SOURCE)


def cmd_extract(args):
    src_dirs = [Path(d) for d in args.src_dir] if args.src_dir else SRC_DIRS
    translate_dirs = [] if args.src_dir else TRANSLATE_DIRS
    for d in src_dirs + translate_dirs:
        if not d.is_dir():
            print(f'{d}: not there, skipped', file=sys.stderr)
    strings = extract_strings(src_dirs) | extract_translated(translate_dirs)
    path = Path(args.lang_dir) / f'{SOURCE}.txt'
    print(f'{path}: {len(strings)} strings')
    return apply_rewrites([(path, render_key_value({s: s for s in strings}, SOURCE))], args)


def cmd_update(args):
    lang_dir = Path(args.lang_dir)
    english = read_key_value(lang_dir / f'{SOURCE}.txt')
    keys = set(english)
    rewrites = []
    for path in language_files(lang_dir):
        old = read_any(path)
        new = {k: old.get(k, '') for k in keys}
        if not args.remove_obsolete:
            for k, v in old.items():
                if k not in keys:
                    new[k] = v
        rewrites.append((path, render_key_value(new, path.stem, len(keys))))
        done = sum(1 for k in keys if new.get(k))
        print(f'{path.stem:16} {done:3}/{len(keys)} translated, +{len(keys - set(old))} added, '
              f'{len(set(old) - keys)} obsolete{"" if args.remove_obsolete else " kept"}')
    return apply_rewrites(rewrites, args)


def line_key(line: str):
    """the key of a "Key=value" line, None for a header, blank or other line"""
    if not line.strip() or line.lstrip().startswith('#') or '=' not in line:
        return None
    return line.split('=', 1)[0].strip()


HEADER = re.compile(r'^(# Untranslated: )(\d+)( / Total: )(\d+)(\s*)$')


def add_to_text(text: str, key: str, value: str):
    """text with `key=value` inserted before the first key sorting after it (case-insensitive, then by case), and
    the "Untranslated: x / Total: y" header counts moved; every other line byte for byte. None: the key is there"""
    lines = text.splitlines(keepends=True)
    nl = '\r\n' if any(l.endswith('\r\n') for l in lines) else '\n'
    at = len(lines)
    for i, line in enumerate(lines):
        k = line_key(line)
        if k == key:
            return None
        if k is not None and at == len(lines) and (k.lower(), k) > (key.lower(), key):
            at = i
    # a duplicate further down must still be found
    if any(line_key(l) == key for l in lines[at:]):
        return None
    if at == len(lines) and lines and not lines[-1].endswith('\n'):
        lines[-1] += nl
    lines.insert(at, f'{key}={value}{nl}')
    for i, line in enumerate(lines[:6]):
        m = HEADER.match(line.rstrip('\r\n'))
        if m:
            ending = line[len(line.rstrip('\r\n')):]
            lines[i] = (f'{m.group(1)}{int(m.group(2)) + (0 if value else 1)}{m.group(3)}{int(m.group(4)) + 1}'
                        f'{m.group(5)}{ending}')
            break
    return ''.join(lines)


def read_add_file(path: str):
    """key<TAB>Language=value... lines -> [(key, {language: value})]; '#' lines and blank lines skipped"""
    entries = []
    for n, raw in enumerate(Path(path).read_text(encoding='utf-8-sig').splitlines(), 1):
        if not raw.strip() or raw.startswith('#'):
            continue
        fields = raw.split('\t')
        values = {}
        for f in fields[1:]:
            if '=' not in f:
                raise ValueError(f'{path}:{n}: "{f}" is not Language=value')
            lang, value = f.split('=', 1)
            values[lang.strip()] = value
        entries.append((fields[0], values))
    return entries


def cmd_add(args):
    lang_dir = Path(args.lang_dir)
    try:
        if args.from_file:
            if args.items:
                raise ValueError('give the text and its translations, or --from FILE, not both')
            entries = read_add_file(args.from_file)
        else:
            if not args.items:
                raise ValueError('add "English text" ["Language=value" ...]')
            values = {}
            for item in args.items[1:]:
                if '=' not in item:
                    raise ValueError(f'"{item}" is not Language=value')
                lang, value = item.split('=', 1)
                values[lang.strip()] = value
            entries = [(args.items[0], values)]
    except (ValueError, OSError) as e:
        print(e, file=sys.stderr)
        return 1
    languages = {p.stem for p in lang_dir.glob('*.txt')}
    for key, values in entries:
        if not key or '=' in key or key != key.strip() or '\n' in key:
            print(f'bad key "{key}": empty, a "=", or surrounding space', file=sys.stderr)
            return 1
        for lang in values:
            if lang not in languages:
                print(f'"{key}": no language file {lang}.txt (have: {", ".join(sorted(languages))})', file=sys.stderr)
                return 1
    texts = {p: p.read_bytes().decode('utf-8') for p in sorted(lang_dir.glob('*.txt'))}
    for key, values in entries:
        missing = []
        for path in texts:
            lang = path.stem
            value = key if lang == SOURCE else values.get(lang, '')
            new = add_to_text(texts[path], key, value)
            if new is None:
                print(f'{path.name}: "{key}" is already there, left as it is')
                continue
            texts[path] = new
            if not value:
                missing.append(lang)
        print(f'"{key}": untranslated in {", ".join(missing) if missing else "none"}')
    for path, text in texts.items():
        with open(path, 'w', encoding='utf-8', newline='') as f:
            f.write(text)
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


def guard_args(p):
    p.add_argument('--force', action='store_true', help='write even when more than 20 lines of a file change')
    p.add_argument('--dry-run', action='store_true', help='print how many lines would change per file, write nothing')


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument('--src-dir', action='append', help='a source tree to scan (repeatable); default: SRC_DIRS')
    parser.add_argument('--lang-dir', default=str(LANG_DIR))
    sub = parser.add_subparsers(dest='command', required=True)
    p = sub.add_parser('add'); p.add_argument('items', nargs='*'); p.add_argument('--from', dest='from_file')
    p.set_defaults(func=cmd_add)
    p = sub.add_parser('extract'); guard_args(p); p.set_defaults(func=cmd_extract)
    p = sub.add_parser('update'); p.add_argument('--remove-obsolete', action='store_true'); guard_args(p)
    p.set_defaults(func=cmd_update)
    p = sub.add_parser('convert'); p.add_argument('files', nargs='+'); p.set_defaults(func=cmd_convert)
    p = sub.add_parser('validate'); p.add_argument('files', nargs='*'); p.set_defaults(func=cmd_validate)
    p = sub.add_parser('compare'); p.add_argument('language'); p.set_defaults(func=cmd_compare)
    p = sub.add_parser('merge'); p.add_argument('dir'); p.add_argument('--map', action='append', default=[]); p.set_defaults(func=cmd_merge)
    args = parser.parse_args()
    return args.func(args)


if __name__ == '__main__':
    sys.exit(main())
