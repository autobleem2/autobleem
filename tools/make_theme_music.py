#!/usr/bin/env python3
"""Generates the default theme's background music, a seamless ambient loop:

  python tools/make_theme_music.py [out file]      (default: payload/Themes/default/ambient.wav)

Four soft pad chords (Am, F, C, G - six seconds each), each note three slightly detuned sines with a
sub-octave under them and a slow swell, and a quiet plucked arpeggio over the chord that drifts
between the two channels. Notes running past the end wrap round to the start, so the loop has no seam.
32 kHz 16-bit stereo, 24 seconds - the format the previous track had (AppAudio opens the mixer at
32 kHz for a wav) and about its length. Standard library only, so the file is reproducible.
"""
import math
import os
import struct
import sys
import wave

RATE = 32000
SECONDS = 24.0
CHORD_SECONDS = 6.0
PEAK = 9000  # of 32767: quiet, it sits under the UI sounds

A3, C4, E4, F3, G3, B3, D4, G4 = 220.0, 261.63, 329.63, 174.61, 196.0, 246.94, 293.66, 392.0
CHORDS = [(A3, C4, E4), (F3, A3, C4), (C4, E4, G4), (G3, B3, D4)]


def pad_note(freq, seconds, swell):
    """one pad note: three detuned sines, a sub-octave, a slow swell in and out"""
    n = int(seconds * RATE)
    out = [0.0] * n
    detunes = (0.997, 1.0, 1.004)
    for i in range(n):
        t = i / RATE
        env = math.sin(math.pi * min(1.0, t / seconds)) ** 0.6  # in and out over the chord
        env *= 0.85 + 0.15 * math.sin(2 * math.pi * swell * t)  # a slow tremolo
        v = 0.0
        for d in detunes:
            v += math.sin(2 * math.pi * freq * d * t)
        v += 0.6 * math.sin(2 * math.pi * freq * 0.5 * t)
        v += 0.12 * math.sin(2 * math.pi * freq * 2.0 * t)
        out[i] = env * v / 4.0
    return out


def pluck(freq, seconds=0.9):
    n = int(seconds * RATE)
    out = [0.0] * n
    for i in range(n):
        t = i / RATE
        env = min(1.0, t / 0.004) * math.exp(-t / 0.28)
        out[i] = env * (math.sin(2 * math.pi * freq * t) + 0.3 * math.sin(2 * math.pi * freq * 2 * t)
                        + 0.1 * math.sin(2 * math.pi * freq * 3 * t))
    return out


def main(argv):
    out = argv[1] if len(argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'payload',
                                                     'Themes', 'default', 'ambient.wav')
    total = int(SECONDS * RATE)
    left = [0.0] * total
    right = [0.0] * total

    # the pads, each chord overlapping the next by a second so they cross-fade
    overlap = 1.0
    for c, chord in enumerate(CHORDS):
        start = int(c * CHORD_SECONDS * RATE)
        for k, f in enumerate(chord):
            note = pad_note(f, CHORD_SECONDS + overlap, swell=0.11 + 0.03 * k)
            pan = 0.35 + 0.3 * k  # spread the chord's notes across the field
            for i, v in enumerate(note):
                j = (start + i) % total
                left[j] += v * (1.0 - pan) * 0.9
                right[j] += v * pan * 0.9

    # the arpeggio: eighth notes at 70 bpm - 56 of them fill the loop exactly, so the rhythm carries over
    # the seam - the chord's notes an octave up, in turn, drifting left-right
    step = SECONDS / 56
    i = 0
    t = 0.0
    while t < SECONDS:
        chord = CHORDS[int(t // CHORD_SECONDS) % len(CHORDS)]
        f = chord[i % len(chord)] * 2
        note = pluck(f)
        pan = 0.5 + 0.35 * math.sin(2 * math.pi * t / 9.0)
        start = int(t * RATE)
        gain = 0.22 if i % 4 else 0.3
        for k, v in enumerate(note):
            j = (start + k) % total
            left[j] += v * gain * (1.0 - pan)
            right[j] += v * gain * pan
        i += 1
        t += step

    # no seam to hide: every note that runs past the end wraps round to the start (the `% total`), so the
    # last chord fades into the first exactly as the others fade into each other

    top = max(max(abs(v) for v in left), max(abs(v) for v in right))
    scale = PEAK / top
    frames = bytearray()
    for l, r in zip(left, right):
        frames += struct.pack('<hh', int(l * scale), int(r * scale))
    with wave.open(out, 'wb') as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(bytes(frames))
    print(out, os.path.getsize(out), 'bytes')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
