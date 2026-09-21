#!/usr/bin/env python3
"""Generates the five UI sounds of the default theme (every other theme falls back to them file by file):

  python tools/make_theme_sounds.py [out dir]      (default: payload/Themes/default/sounds)

cursor.wav      a soft tick - moving the selection (0.2 s)
cancel.wav      two notes stepping down - Circle, going back (0.6 s)
home_up.wav     two notes stepping up - opening a game's menu (0.9 s)
home_down.wav   the same two notes stepping down - closing it (0.9 s)
resume_new.wav  a rising four-note chime with a chord under it - a game starting (2.5 s)

They replace the console firmware's own sounds, which the themes carried until 2026-09-21. Everything is
synthesised here with the standard library only, so the set is reproducible: soft tones (a sine with two
quiet harmonics), an attack/decay envelope, a touch of echo, 44.1 kHz 16-bit stereo like the originals,
peaks around -10 dBFS so they sit at the same level in the mixer.
"""
import math
import os
import struct
import sys
import wave

RATE = 44100
PEAK = 8000  # of 32767: the originals peaked at 6000-10000


def tone(freq, seconds, attack=0.005, decay=0.25, harmonics=(1.0, 0.25, 0.08), start=0.0, gain=1.0):
    """one note: samples of a soft tone with an exponential decay, placed at `start` seconds; returns
    (offset in samples, list of floats)"""
    n = int(seconds * RATE)
    out = []
    for i in range(n):
        t = i / RATE
        env = min(1.0, t / attack) * math.exp(-t / decay) if attack > 0 else math.exp(-t / decay)
        v = 0.0
        for k, a in enumerate(harmonics, start=1):
            v += a * math.sin(2 * math.pi * freq * k * t)
        out.append(gain * env * v)
    return int(start * RATE), out


def mix(length_seconds, parts):
    """sum the parts into one mono buffer of the given length"""
    buf = [0.0] * int(length_seconds * RATE)
    for offset, samples in parts:
        for i, v in enumerate(samples):
            j = offset + i
            if j < len(buf):
                buf[j] += v
    return buf


def echo(buf, delay_seconds, feedback, wet):
    """a single feedback delay line - a little room around the note"""
    d = int(delay_seconds * RATE)
    out = list(buf)
    for i in range(d, len(out)):
        out[i] += wet * out[i - d] * feedback
    return out


def fade_out(buf, seconds):
    n = int(seconds * RATE)
    for i in range(max(0, len(buf) - n), len(buf)):
        buf[i] *= (len(buf) - i) / n
    return buf


def write(path, buf, width=0.0):
    """normalise to PEAK and write 16-bit stereo; `width` (0..1) sends a slightly different level to each
    side so the sound is not dead centre"""
    top = max(1e-9, max(abs(v) for v in buf))
    scale = PEAK / top
    frames = bytearray()
    for i, v in enumerate(buf):
        s = v * scale
        pan = width * math.sin(i / RATE * 3.0)  # a slow drift between the sides
        left = int(max(-32767, min(32767, s * (1.0 - 0.5 * max(0.0, pan)))))
        right = int(max(-32767, min(32767, s * (1.0 - 0.5 * max(0.0, -pan)))))
        frames += struct.pack('<hh', left, right)
    with wave.open(path, 'wb') as w:
        w.setnchannels(2)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(bytes(frames))


# the notes, in Hz (C major: C5 E5 G5 C6 and the D5 the cancel steps down to)
C5, D5, E5, G5, A5, C6 = 523.25, 587.33, 659.25, 783.99, 880.0, 1046.5


def cursor():
    # a tick: a very short high tone with a fast decay and a pinch of a lower one for body
    return fade_out(mix(0.2, [tone(2093.0, 0.12, attack=0.002, decay=0.025, harmonics=(1.0, 0.1)),
                              tone(C6, 0.12, attack=0.002, decay=0.03, gain=0.5)]), 0.05)


def cancel():
    parts = [tone(A5, 0.5, decay=0.12), tone(D5, 0.45, decay=0.18, start=0.11, gain=0.9)]
    return fade_out(echo(mix(0.62, parts), 0.09, 0.5, 0.35), 0.15)


def home_up():
    parts = [tone(C5, 0.6, decay=0.16), tone(G5, 0.7, decay=0.22, start=0.12, gain=0.9)]
    return fade_out(echo(mix(0.9, parts), 0.11, 0.55, 0.4), 0.25)


def home_down():
    parts = [tone(G5, 0.6, decay=0.16), tone(C5, 0.7, decay=0.22, start=0.12, gain=0.9)]
    return fade_out(echo(mix(0.9, parts), 0.11, 0.55, 0.4), 0.25)


def resume():
    # the chime: C5 E5 G5 C6 in turn, then the chord holds under the last note and fades
    parts = []
    for i, f in enumerate((C5, E5, G5, C6)):
        parts.append(tone(f, 1.6, decay=0.5, start=0.13 * i, gain=0.8))
    for f in (C5 / 2, E5 / 2, G5 / 2):  # the pad an octave down, slow attack
        parts.append(tone(f, 2.3, attack=0.35, decay=1.1, harmonics=(1.0, 0.3, 0.12, 0.05), start=0.3, gain=0.35))
    return fade_out(echo(mix(2.5, parts), 0.17, 0.5, 0.35), 0.8)


SOUNDS = {
    'cursor.wav': (cursor, 0.0),
    'cancel.wav': (cancel, 0.3),
    'home_up.wav': (home_up, 0.3),
    'home_down.wav': (home_down, 0.3),
    'resume_new.wav': (resume, 0.5),
}


def main(argv):
    out = argv[1] if len(argv) > 1 else os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'payload',
                                                     'Themes', 'default', 'sounds')
    os.makedirs(out, exist_ok=True)
    for name, (make, width) in SOUNDS.items():
        path = os.path.join(out, name)
        write(path, make(), width)
        print(path, os.path.getsize(path), 'bytes')
    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))
