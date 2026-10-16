#!/usr/bin/env python3
"""Compare two screen tours (tools/ui_tour.py) and show only the screens that changed:

  python tools/ui_diff.py PREV_DIR NEW_DIR [--tolerance N]

Each <nn>-<screen>.png of NEW_DIR is matched by screen name (the <nn>- number is ignored, so a new screen in the
middle does not shift the rest) with the one in PREV_DIR. A screen is changed when it is missing on either side,
the sizes differ, or more pixels than its allowance differ by more than the tolerance in any channel (default 16
of 255, enough for antialiasing).

Writes <NEW_DIR>/diff/<screen>.png (old | new | changed pixels in red) for changed screens only and
<NEW_DIR>/changed.html (the list, the changed-pixel percentage, and "N of M screens unchanged"), prints a JSON
summary {"changed": [...], "unchanged": N} on stdout and always exits 0.

Animated screens: the per-screen table MASKS below. Each entry may have
  'ignore': True            the screen is not compared (counted unchanged unless missing on one side)
  'rects': [(x0,y0,x1,y1)]  areas to leave out, as fractions 0..1 of the width/height (a clock, a carousel)
  'max_percent': P          the screen counts as changed only above P percent changed pixels
A new screen that moves on its own is one more line there.
"""
import json
import os
import re
import sys

DEFAULT_TOLERANCE = 16

MASKS = {
    # the surprise game and its title animate on their own
    'surprise-title': {'ignore': True},
    'surprise-game': {'ignore': True},
    # the carousel idles with a slow motion; allow a little
    'launcher': {'max_percent': 1.0},
    'launcher-icon-row': {'max_percent': 1.0},
    'launcher-retroarch': {'max_percent': 1.0},
    'launcher-apps': {'max_percent': 1.0},
    # live numbers (temperature, memory, uptime)
    'hardware-info': {'max_percent': 2.0},
    'hardware-info-page-2': {'max_percent': 2.0},
}


def screen_name(filename):
    """'07-options.png' -> 'options'"""
    return re.sub(r'^\d+-', '', os.path.splitext(filename)[0])


def tour_shots(directory):
    """{screen name: path} of the <nn>-<screen>.png in a tour folder (tour.png is the contact sheet, skipped)."""
    found = {}
    if not os.path.isdir(directory):
        return found
    for f in sorted(os.listdir(directory)):
        if f.lower().endswith('.png') and re.match(r'^\d+-', f):
            found[screen_name(f)] = os.path.join(directory, f)
    return found


def _mask_image(size, name):
    from PIL import Image, ImageDraw
    mask = Image.new('L', size, 255)
    draw = ImageDraw.Draw(mask)
    for x0, y0, x1, y1 in MASKS.get(name, {}).get('rects', []):
        draw.rectangle([int(x0 * size[0]), int(y0 * size[1]), int(x1 * size[0]), int(y1 * size[1])], fill=0)
    return mask


def compare(old_path, new_path, name='', tolerance=DEFAULT_TOLERANCE):
    """-> (changed: bool, percent: float, diff_mask image or None). Missing side or other size: changed, 100%."""
    from PIL import Image, ImageChops
    if not old_path or not new_path or not os.path.exists(old_path) or not os.path.exists(new_path):
        return True, 100.0, None
    old = Image.open(old_path).convert('RGB')
    new = Image.open(new_path).convert('RGB')
    if old.size != new.size:
        return True, 100.0, None
    rule = MASKS.get(name, {})
    if rule.get('ignore'):
        return False, 0.0, None
    delta = ImageChops.difference(old, new)
    r, g, b = delta.split()
    biggest = ImageChops.lighter(ImageChops.lighter(r, g), b)
    flags = biggest.point(lambda v: 255 if v > tolerance else 0)
    flags = ImageChops.multiply(flags, _mask_image(old.size, name))
    count = flags.histogram()[255]
    percent = 100.0 * count / float(old.size[0] * old.size[1])
    return count > 0 and percent > rule.get('max_percent', 0.0), percent, flags


def _side_by_side(old_path, new_path, flags, out_path):
    from PIL import Image
    panels = []
    for p in (old_path, new_path):
        panels.append(Image.open(p).convert('RGB') if p and os.path.exists(p) else None)
    ref = next((p for p in panels if p is not None), None)
    size = ref.size if ref else (640, 360)
    panels = [p if p is not None and p.size == size else Image.new('RGB', size, (60, 60, 60)) for p in panels]
    if flags is None or flags.size != size:
        highlight = Image.new('RGB', size, (255, 0, 0))
    else:
        base = panels[1].convert('L').point(lambda v: v // 3).convert('RGB')
        highlight = Image.composite(Image.new('RGB', size, (255, 0, 0)), base, flags)
    sheet = Image.new('RGB', (size[0] * 3 + 8, size[1]), (255, 255, 255))
    sheet.paste(panels[0], (0, 0))
    sheet.paste(panels[1], (size[0] + 4, 0))
    sheet.paste(highlight, (size[0] * 2 + 8, 0))
    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    sheet.save(out_path)


def _html(rows, unchanged, total):
    out = ['<!DOCTYPE html><html><head><meta charset="utf-8"><title>Changed screens</title></head><body>']
    for name, percent, note in rows:
        out.append('<h3>%s - %.2f%% changed%s</h3>' % (name, percent, (' (%s)' % note) if note else ''))
        out.append('<p>old | new | changed pixels</p>')
        out.append('<img src="diff/%s.png" style="max-width:100%%">' % name)
    out.append('<p>%d of %d screens unchanged</p></body></html>' % (unchanged, total))
    return '\n'.join(out)


def diff_tours(prev_dir, new_dir, tolerance=DEFAULT_TOLERANCE):
    old = tour_shots(prev_dir)
    new = tour_shots(new_dir)
    names = list(new) + [n for n in old if n not in new]
    changed = []
    rows = []
    for name in names:
        is_changed, percent, flags = compare(old.get(name), new.get(name), name, tolerance)
        if not is_changed:
            continue
        note = ''
        if name not in old:
            note = 'missing in the previous tour'
        elif name not in new:
            note = 'missing in this tour'
        elif flags is None:
            note = 'size differs'
        _side_by_side(old.get(name), new.get(name), flags, os.path.join(new_dir, 'diff', name + '.png'))
        changed.append(name)
        rows.append((name, percent, note))
    unchanged = len(names) - len(changed)
    with open(os.path.join(new_dir, 'changed.html'), 'w', encoding='utf-8') as f:
        f.write(_html(rows, unchanged, len(names)))
    return {'changed': changed, 'unchanged': unchanged}


def main(argv):
    args = [a for a in argv[1:]]
    tolerance = DEFAULT_TOLERANCE
    if '--tolerance' in args:
        i = args.index('--tolerance')
        tolerance = int(args[i + 1])
        del args[i:i + 2]
    if len(args) != 2:
        print(__doc__)
        return 0
    try:
        summary = diff_tours(os.path.abspath(args[0]), os.path.abspath(args[1]), tolerance)
    except Exception as e:  # always exit 0; the summary says what happened
        summary = {'changed': [], 'unchanged': 0, 'error': str(e)}
    print(json.dumps(summary))
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
