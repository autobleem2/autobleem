#!/usr/bin/env python3
"""
A small Audio CD image for the CD player screen: a .cue and a .bin with generated tones, one track each.

    python tools/make_test_cd.py OUT_DIR [--tracks 3] [--seconds 6]

Writes OUT_DIR/testcd.cue and OUT_DIR/testcd.bin (raw 2352-byte sectors, 16-bit stereo 44.1 kHz). Every track is a
chord of a few sine tones that sweeps slowly, so the spectrum bars move. Open it with AB_CD_PLAYER_IMAGE=OUT_DIR/testcd.cue.
"""

import argparse
import math
import struct
from pathlib import Path

RATE = 44100
SECTOR_FRAMES = 588  # 2352 bytes / 4


def track_samples(index, seconds):
    """interleaved stereo s16 frames for sectors * 588 frames of a chord that sweeps up and down"""
    sectors = int(seconds * 75)
    base = [220.0, 330.0, 440.0][index % 3] * (1 + index // 3)
    out = bytearray()
    for n in range(sectors * SECTOR_FRAMES):
        t = n / RATE
        sweep = 1.0 + 0.5 * math.sin(2 * math.pi * 0.4 * t)
        left = sum(math.sin(2 * math.pi * base * k * sweep * t) / k for k in (1, 2, 4, 8))
        right = sum(math.sin(2 * math.pi * base * k * t) / k for k in (1, 3, 6, 12))
        fade = min(1.0, n / 2000.0, (sectors * SECTOR_FRAMES - n) / 2000.0)
        out += struct.pack('<hh', int(9000 * fade * left / 1.9), int(9000 * fade * right / 1.9))
    return bytes(out), sectors


def msf(sector):
    return '%02d:%02d:%02d' % (sector // (75 * 60), sector // 75 % 60, sector % 75)


def main():
    parser = argparse.ArgumentParser(description=__doc__.split('\n')[1])
    parser.add_argument('out_dir')
    parser.add_argument('--tracks', type=int, default=3)
    parser.add_argument('--seconds', type=float, default=6.0)
    args = parser.parse_args()
    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    cue = ['FILE "testcd.bin" BINARY']
    data = bytearray()
    start = 0
    for i in range(args.tracks):
        samples, sectors = track_samples(i, args.seconds + i)
        cue.append('  TRACK %02d AUDIO' % (i + 1))
        cue.append('    INDEX 01 %s' % msf(start))
        data += samples
        start += sectors
    (out / 'testcd.bin').write_bytes(data)
    (out / 'testcd.cue').write_text('\n'.join(cue) + '\n')
    print('%s: %d tracks, %d sectors' % (out / 'testcd.cue', args.tracks, start))


if __name__ == '__main__':
    main()
