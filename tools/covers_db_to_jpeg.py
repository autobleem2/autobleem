#!/usr/bin/env python3
"""Re-encode the PNG covers of the covers databases as JPEG.

    python tools/covers_db_to_jpeg.py SRC_DIR OUT_DIR [--quality 85] [names...]

Every covers{U,P,J}.db in SRC_DIR (or only the files named, e.g. coversU.db) is copied to OUT_DIR with the
sqlite backup API (the source is opened read-only and never changed), every PNG blob in GAME.COVER is
re-encoded to JPEG (RGB, optimize=True) and the copy is VACUUMed. A blob that is not a PNG (a JPEG already,
NULL) is left as it is. Prints the size of each database before and after.

Needs Pillow. The launcher decodes both formats and names the cover file after what the bytes are.
"""
import argparse
import io
import sqlite3
import sys
from pathlib import Path

PNG_MAGIC = b'\x89PNG'
DEFAULT_DBS = ('coversU.db', 'coversP.db', 'coversJ.db')


def to_jpeg(blob: bytes, quality: int) -> bytes:
    from PIL import Image
    with Image.open(io.BytesIO(blob)) as img:
        if img.mode in ('RGBA', 'LA') or (img.mode == 'P' and 'transparency' in img.info):
            rgba = img.convert('RGBA')
            flat = Image.new('RGB', rgba.size, (255, 255, 255))
            flat.paste(rgba, mask=rgba.getchannel('A'))
            rgb = flat
        else:
            rgb = img.convert('RGB')
    out = io.BytesIO()
    rgb.save(out, 'JPEG', quality=quality, optimize=True)
    return out.getvalue()


def convert_db(src: Path, dst: Path, quality: int):
    """Returns (converted, kept) blob counts."""
    source = sqlite3.connect(f'file:{src.resolve().as_posix()}?mode=ro', uri=True)
    target = sqlite3.connect(str(dst))
    try:
        source.backup(target)
    finally:
        source.close()
    converted = kept = 0
    try:
        rows = target.execute('SELECT ID, COVER FROM GAME WHERE COVER IS NOT NULL').fetchall()
        for game_id, blob in rows:
            blob = bytes(blob)
            if not blob.startswith(PNG_MAGIC):
                kept += 1
                continue
            try:
                jpeg = to_jpeg(blob, quality)
            except Exception as e:  # an undecodable PNG stays as it was
                print(f'  WARNING: {src.name} GAME {game_id}: {e}; kept as is', file=sys.stderr)
                kept += 1
                continue
            target.execute('UPDATE GAME SET COVER = ? WHERE ID = ?', (jpeg, game_id))
            converted += 1
        target.commit()
        target.execute('VACUUM')
    finally:
        target.close()
    return converted, kept


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('src_dir', type=Path)
    p.add_argument('out_dir', type=Path)
    p.add_argument('--quality', type=int, default=85, help='JPEG quality (default 85)')
    p.add_argument('names', nargs='*', help=f'database file names (default {", ".join(DEFAULT_DBS)})')
    opts = p.parse_args()

    if opts.src_dir.resolve() == opts.out_dir.resolve():
        print('SRC_DIR and OUT_DIR must differ: the sources are never changed', file=sys.stderr)
        return 2
    names = opts.names or [n for n in DEFAULT_DBS if (opts.src_dir / n).is_file()]
    if not names:
        print(f'no covers database found in {opts.src_dir}', file=sys.stderr)
        return 2
    opts.out_dir.mkdir(parents=True, exist_ok=True)

    status = 0
    for name in names:
        src = opts.src_dir / name
        if not src.is_file():
            print(f'{name}: not found in {opts.src_dir}', file=sys.stderr)
            status = 1
            continue
        dst = opts.out_dir / name
        if dst.exists():
            dst.unlink()
        converted, kept = convert_db(src, dst, opts.quality)
        before, after = src.stat().st_size, dst.stat().st_size
        pct = 100.0 * after / before if before else 0.0
        print(f'{name}: {before:,} -> {after:,} bytes ({pct:.0f}%); {converted} re-encoded, {kept} left as they were')
    return status


if __name__ == '__main__':
    sys.exit(main())
