#!/usr/bin/env python3
"""The first-boot installer's screen: the AutoBleem logo, two progress bars and the last lines of output,
drawn straight onto the Linux framebuffer - what an installer with a GUI shows, on a Raspberry Pi OS Lite
that has nothing installed yet (no X, no SDL, no plymouth, no PIL: python3, /dev/fb0 and the console fonts
are all a fresh Lite image has, and all this uses).

    bash install.sh ... 2>&1 | tee -a install.log | AB_UI_MARKERS=1 python3 autobleem-install-ui.py \\
        --logo /opt/autobleem-image/splash.png --tty /dev/tty8

Reads the installer's output on stdin:
  - "@@phase N/M text" lines (install.sh prints them with AB_UI_MARKERS=1) move the first bar and set the
    heading; anything else goes to the output box
  - a percentage in a line ("[ 12/694]  2%", wget's "45%[===>  ]") moves the second bar; with none for a
    while the bar pulses instead
  - a chunk ending in "\\r" (a progress line rewriting itself) replaces the box's last line instead of
    adding one; ANSI colours are stripped
On EOF the last frame stays (the caller reboots). --render FILE writes the frame as a PPM instead of the
framebuffer, for a look on a PC.
"""

import argparse
import glob
import gzip
import os
import re
import select
import struct
import sys
import time
import zlib

try:
    import fcntl  # Linux only; --render on a PC has no console to switch
except ImportError:
    fcntl = None

# the ab2 theme's palette (tools/repo_index.py's page uses the same)
NAVY = (6, 26, 58)
PANEL = (4, 22, 56)
LINE = (60, 140, 190)
CYAN = (79, 200, 255)
INK = (232, 242, 255)
DIM = (150, 175, 205)
BLACK = (0, 0, 0)

KDSETMODE = 0x4B3A
KD_TEXT = 0
KD_GRAPHICS = 1


#*******************************
# PNG (RGB/RGBA, 8-bit, not interlaced)
#*******************************
def load_png(path):
    """-> (width, height, rows) with rows a list of bytes of RGB triplets; None if unreadable."""
    try:
        with open(path, "rb") as f:
            data = f.read()
    except OSError:
        return None
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    pos, idat, width, height, channels = 8, [], 0, 0, 0
    while pos + 8 <= len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            width, height, depth, ctype, _, _, interlace = struct.unpack(">IIBBBBB", body)
            if depth != 8 or interlace != 0 or ctype not in (0, 2, 4, 6):
                return None
            channels = {0: 1, 2: 3, 4: 2, 6: 4}[ctype]
        elif kind == b"IDAT":
            idat.append(body)
        elif kind == b"IEND":
            break
    raw = zlib.decompress(b"".join(idat))
    stride = width * channels
    rows, prev = [], bytearray(stride)
    p = 0
    for _ in range(height):
        ftype = raw[p]
        cur = bytearray(raw[p + 1:p + 1 + stride])
        p += 1 + stride
        if ftype == 1:
            for i in range(channels, stride):
                cur[i] = (cur[i] + cur[i - channels]) & 0xFF
        elif ftype == 2:
            for i in range(stride):
                cur[i] = (cur[i] + prev[i]) & 0xFF
        elif ftype == 3:
            for i in range(stride):
                left = cur[i - channels] if i >= channels else 0
                cur[i] = (cur[i] + ((left + prev[i]) >> 1)) & 0xFF
        elif ftype == 4:
            for i in range(stride):
                a = cur[i - channels] if i >= channels else 0
                b = prev[i]
                c = prev[i - channels] if i >= channels else 0
                pa, pb, pc = abs(b - c), abs(a - c), abs(a + b - 2 * c)
                pred = a if (pa <= pb and pa <= pc) else (b if pb <= pc else c)
                cur[i] = (cur[i] + pred) & 0xFF
        if channels == 3:
            rgb = bytes(cur)
        elif channels == 4:
            rgb = bytes(b for i, b in enumerate(cur) if i % 4 != 3)
        elif channels == 1:
            rgb = bytes(b for v in cur for b in (v, v, v))
        else:
            rgb = bytes(b for i, v in enumerate(cur) if i % 2 == 0 for b in (v, v, v))
        rows.append(rgb)
        prev = cur
    return width, height, rows


def crop_dark_border(png, threshold=16):
    """The logo without the black around it: (x0, y0, x1, y1) of the pixels brighter than threshold."""
    width, height, rows = png
    x0, y0, x1, y1 = width, height, 0, 0
    for y, row in enumerate(rows):
        bright = [x for x in range(width) if max(row[x * 3], row[x * 3 + 1], row[x * 3 + 2]) > threshold]
        if bright:
            x0, x1 = min(x0, bright[0]), max(x1, bright[-1] + 1)
            y0, y1 = min(y0, y), max(y1, y + 1)
    return (0, 0, width, height) if x1 <= x0 else (x0, y0, x1, y1)


#*******************************
# PSF console fonts
#*******************************
class Font:
    def __init__(self, path):
        with (gzip.open if path.endswith(".gz") else open)(path, "rb") as f:
            data = f.read()
        if data[:2] == b"\x36\x04":                                  # PSF1
            self.height, self.width = data[3], 8
            count = 512 if data[2] & 1 else 256
            has_table = bool(data[2] & 6)
            glyphs = data[4:4 + count * self.height]
            table = data[4 + count * self.height:]
            self.glyph_bytes = self.height
        elif data[:4] == b"\x72\xb5\x4a\x86":                       # PSF2
            _, hdr, flags, count, charsize, self.height, self.width = struct.unpack("<IIIIIII", data[4:32])
            has_table = bool(flags & 1)
            glyphs = data[hdr:hdr + count * charsize]
            table = data[hdr + count * charsize:]
            self.glyph_bytes = charsize
        else:
            raise ValueError("not a PSF font: " + path)
        self.glyphs = glyphs
        self.count = count
        self.index = {}
        if has_table:
            self._read_table(table, data[:2] == b"\x36\x04")
        self.cache = {}

    def _read_table(self, table, psf1):
        # PSF2: UTF-8 sequences per glyph, 0xFE separates sequences, 0xFF ends the glyph's entry;
        # PSF1: UCS-2 little-endian, 0xFFFE / 0xFFFF the same way
        glyph, pos = 0, 0
        if psf1:
            while pos + 1 < len(table) and glyph < self.count:
                code = table[pos] | (table[pos + 1] << 8)
                pos += 2
                if code == 0xFFFF:
                    glyph += 1
                elif code != 0xFFFE:
                    self.index.setdefault(code, glyph)
            return
        while pos < len(table) and glyph < self.count:
            start = pos
            while pos < len(table) and table[pos] not in (0xFE, 0xFF):
                pos += 1
            for ch in table[start:pos].decode("utf-8", "replace"):
                self.index.setdefault(ord(ch), glyph)
            if pos < len(table) and table[pos] == 0xFF:
                glyph += 1
            pos += 1

    def glyph_rows(self, ch):
        """-> list of per-row bitmasks (bit width-1 .. 0 = left .. right), cached."""
        rows = self.cache.get(ch)
        if rows is None:
            code = ord(ch)
            g = self.index.get(code, code if code < self.count else ord("?"))
            base = g * self.glyph_bytes
            per_row = (self.width + 7) // 8
            rows = []
            for r in range(self.height):
                v = 0
                for b in range(per_row):
                    v = (v << 8) | self.glyphs[base + r * per_row + b]
                rows.append(v >> (per_row * 8 - self.width))
            self.cache[ch] = rows
        return rows


FONT_DIR = "/usr/share/consolefonts"


def find_font(px):
    """The Terminus console font of that height (Lat15 first), else any font of that height, else None."""
    for pattern in ("Lat15-Terminus%d*.psf*", "Uni2-Terminus%d*.psf*", "*Terminus%d*.psf*", "*%d*.psf*"):
        hits = sorted(glob.glob(os.path.join(FONT_DIR, pattern % px)))
        hits = [h for h in hits if "Bold" not in h] or hits
        if hits:
            return Font(hits[0])
    return None


#*******************************
# the canvas
#*******************************
class Canvas:
    def __init__(self, width, height, bpp, stride):
        self.width, self.height, self.bpp, self.stride = width, height, bpp, stride
        self.bytes_pp = bpp // 8
        self.buf = bytearray(stride * height)
        self.pixel_cache = {}

    def pixel(self, rgb):
        p = self.pixel_cache.get(rgb)
        if p is None:
            r, g, b = rgb
            if self.bpp == 16:
                p = struct.pack("<H", ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3))
            else:
                p = struct.pack("<I", (r << 16) | (g << 8) | b)
            self.pixel_cache[rgb] = p
        return p

    def fill(self, x, y, w, h, rgb):
        x0, y0 = max(0, x), max(0, y)
        x1, y1 = min(self.width, x + w), min(self.height, y + h)
        if x1 <= x0 or y1 <= y0:
            return
        row = self.pixel(rgb) * (x1 - x0)
        for yy in range(y0, y1):
            off = yy * self.stride + x0 * self.bytes_pp
            self.buf[off:off + len(row)] = row

    def frame(self, x, y, w, h, rgb, t=2):
        self.fill(x, y, w, t, rgb)
        self.fill(x, y + h - t, w, t, rgb)
        self.fill(x, y, t, h, rgb)
        self.fill(x + w - t, y, t, h, rgb)

    def blit_rgb(self, x, y, png, crop, scale_num, scale_den):
        """Draw a crop of a decoded PNG, scaled by scale_num/scale_den (nearest neighbour)."""
        width, height, rows = png
        cx0, cy0, cx1, cy1 = crop
        out_w = (cx1 - cx0) * scale_num // scale_den
        out_h = (cy1 - cy0) * scale_num // scale_den
        for oy in range(out_h):
            sy = cy0 + oy * scale_den // scale_num
            row = rows[sy]
            line = bytearray()
            for ox in range(out_w):
                sx = cx0 + ox * scale_den // scale_num
                line += self.pixel((row[sx * 3], row[sx * 3 + 1], row[sx * 3 + 2]))
            yy = y + oy
            if 0 <= yy < self.height:
                off = yy * self.stride + max(0, x) * self.bytes_pp
                self.buf[off:off + len(line)] = line

    def text(self, x, y, s, font, fg, bg=None):
        """Draw s with the PSF font; bg None leaves the background as it is (slower). Returns the width."""
        if font is None:
            return 0
        fgp = self.pixel(fg)
        bgp = self.pixel(bg) if bg is not None else None
        cx = x
        for ch in s:
            rows = font.glyph_rows(ch)
            for r, bits in enumerate(rows):
                yy = y + r
                if not 0 <= yy < self.height:
                    continue
                if bgp is not None:
                    line = b"".join(fgp if bits & (1 << (font.width - 1 - i)) else bgp for i in range(font.width))
                    off = yy * self.stride + cx * self.bytes_pp
                    self.buf[off:off + len(line)] = line
                else:
                    for i in range(font.width):
                        if bits & (1 << (font.width - 1 - i)):
                            off = yy * self.stride + (cx + i) * self.bytes_pp
                            self.buf[off:off + len(fgp)] = fgp
            cx += font.width
            if cx >= self.width:
                break
        return cx - x


#*******************************
# the screen
#*******************************
class Screen:
    def __init__(self, canvas, logo, big, small):
        self.c = canvas
        self.big, self.small = big, small
        w, h = canvas.width, canvas.height
        self.margin = w // 8
        # the logo: its bright part, scaled to at most 40% of the height and 60% of the width, centred
        self.logo_bottom = h // 12
        if logo:
            crop = crop_dark_border(logo)
            cw, ch = crop[2] - crop[0], crop[3] - crop[1]
            num, den = 1, 1
            max_w, max_h = w * 6 // 10, h * 4 // 10
            if cw > max_w or ch > max_h:
                den, num = max(cw * 1000 // max_w, ch * 1000 // max_h), 1000
            # blitted once into the canvas, then kept as raw rows: a frame copies them back with slice
            # assignments instead of scaling the PNG again (seconds per frame on a Pi in pure Python)
            x, y = (w - cw * num // den) // 2, h // 16
            canvas.blit_rgb(x, y, logo, crop, num, den)
            out_w, out_h = cw * num // den, ch * num // den
            self.logo = []
            for yy in range(y, min(canvas.height, y + out_h)):
                off = yy * canvas.stride + x * canvas.bytes_pp
                self.logo.append((off, bytes(canvas.buf[off:off + out_w * canvas.bytes_pp])))
            self.logo_bottom = y + out_h + h // 24
        else:
            self.logo = None
        self.phase, self.phase_index, self.phase_count = "Preparing", 0, 1
        self.percent = None
        self.percent_at = time.monotonic()
        self.lines = []
        self.transient = None
        self.done = False
        self.line_height = (small.height + 4) if small else 20
        self.box_lines = 8

    def draw(self):
        c = self.c
        w, h = c.width, c.height
        c.fill(0, 0, w, h, BLACK)
        if self.logo:
            for off, line in self.logo:
                c.buf[off:off + len(line)] = line
        y = self.logo_bottom
        x0, bw = self.margin, w - 2 * self.margin
        # heading: the phase
        heading = "Setting up AutoBleem" if not self.done else "AutoBleem is installed - restarting"
        c.text(x0, y, heading, self.big, INK, BLACK)
        y += (self.big.height if self.big else 32) + h // 60
        # bar 1: the phases
        label = "%s  (step %d of %d)" % (self.phase, self.phase_index, self.phase_count) if self.phase_count else self.phase
        c.text(x0, y, label, self.small, DIM, BLACK)
        y += self.line_height + 4
        self._bar(x0, y, bw, self.phase_index * 1000 // max(1, self.phase_count) if not self.done else 1000)
        y += 28 + h // 40
        # bar 2: inside the phase
        if self.percent is not None:
            c.text(x0, y, "%d%%" % self.percent, self.small, DIM, BLACK)
        else:
            c.text(x0, y, "working...", self.small, DIM, BLACK)
        y += self.line_height + 4
        if self.percent is not None:
            self._bar(x0, y, bw, self.percent * 10)
        else:
            self._pulse(x0, y, bw)
        y += 28 + h // 30
        # the output box
        box_h = self.box_lines * self.line_height + 16
        c.fill(x0, y, bw, box_h, PANEL)
        c.frame(x0, y, bw, box_h, LINE, 2)
        shown = list(self.lines[-(self.box_lines - (1 if self.transient else 0)):])
        if self.transient:
            shown.append(self.transient)
        max_chars = (bw - 24) // (self.small.width if self.small else 8)
        ty = y + 8
        for line in shown:
            c.text(x0 + 12, ty, line[:max_chars], self.small, DIM, PANEL)
            ty += self.line_height

    def _bar(self, x, y, w, permille):
        c = self.c
        c.fill(x, y, w, 28, PANEL)
        c.frame(x, y, w, 28, LINE, 2)
        fill = (w - 8) * max(0, min(1000, permille)) // 1000
        if fill > 0:
            c.fill(x + 4, y + 4, fill, 20, CYAN)

    def _pulse(self, x, y, w):
        c = self.c
        c.fill(x, y, w, 28, PANEL)
        c.frame(x, y, w, 28, LINE, 2)
        span = (w - 8) // 5
        t = time.monotonic() % 2.0
        pos = int((w - 8 - span) * (t if t < 1.0 else 2.0 - t))
        c.fill(x + 4 + pos, y + 4, span, 20, CYAN)


#*******************************
# input parsing
#*******************************
ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")
PHASE = re.compile(r"^@@phase (\d+)/(\d+) (.*)$")
PERCENT = re.compile(r"(?<![\d.])(\d{1,3})%")


def feed(screen, chunk, ends_with_cr):
    text = ANSI.sub("", chunk).replace("\t", "    ").strip()
    if not text:
        return
    m = PHASE.match(text)
    if m:
        screen.phase_index, screen.phase_count, screen.phase = int(m.group(1)), int(m.group(2)), m.group(3)
        screen.percent = None
        screen.transient = None
        return
    pct = PERCENT.findall(text)
    if pct:
        screen.percent = min(100, int(pct[-1]))
        screen.percent_at = time.monotonic()
    if text.startswith("==>"):
        text = text[3:].strip()
    if ends_with_cr:
        screen.transient = text
    else:
        screen.transient = None
        screen.lines.append(text)
        screen.lines = screen.lines[-50:]


#*******************************
# main
#*******************************
def main():
    global FONT_DIR
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--logo", default="", help="the PNG with the logo (the plymouth splash)")
    ap.add_argument("--fb", default="/dev/fb0")
    ap.add_argument("--tty", default="", help="the console to switch to graphics mode while drawing")
    ap.add_argument("--render", default="", help="write the final frame to this PPM file instead of the framebuffer")
    ap.add_argument("--size", default="", help="WxH for --render (default 1920x1080)")
    ap.add_argument("--fps", type=float, default=4.0)
    ap.add_argument("--fonts", default=FONT_DIR, help="where the PSF console fonts are")
    ap.add_argument("--keep-graphics", action="store_true",
                    help="leave the console in graphics mode at exit (the caller reboots from the picture)")
    args = ap.parse_args()
    FONT_DIR = args.fonts

    if args.render:
        width, height = (int(v) for v in (args.size or "1920x1080").split("x"))
        bpp, stride = 32, width * 4
        fb = None
    else:
        def sysfs(name, default):
            try:
                with open("/sys/class/graphics/%s/%s" % (os.path.basename(args.fb), name)) as f:
                    return f.read().strip()
            except OSError:
                return default
        width, height = (int(v) for v in sysfs("virtual_size", "1920,1080").split(","))
        bpp = int(sysfs("bits_per_pixel", "32"))
        stride = int(sysfs("stride", str(width * bpp // 8)))
        fb = open(args.fb, "r+b", buffering=0)

    canvas = Canvas(width, height, bpp, stride)
    logo = load_png(args.logo) if args.logo else None
    scale = height / 1080.0
    big = find_font(32 if scale >= 0.9 else 24)
    small = find_font(20 if scale >= 0.9 else 16) or find_font(16)
    screen = Screen(canvas, logo, big, small)

    tty = None
    if args.tty and fb is not None and fcntl is not None:
        try:
            tty = os.open(args.tty, os.O_RDWR)
            fcntl.ioctl(tty, KDSETMODE, KD_GRAPHICS)
        except OSError:
            tty = None

    def present():
        screen.draw()
        if fb is not None:
            fb.seek(0)
            fb.write(canvas.buf)

    try:
        present()
        stdin = sys.stdin.buffer
        pending = b""
        last_draw = time.monotonic()
        interval = 1.0 / max(0.5, args.fps)
        while True:
            if os.name == "nt":       # no select() on a pipe there: a plain blocking read (only for --render)
                data = stdin.read1(65536) if hasattr(stdin, "read1") else stdin.read(65536)
                ready = True
            else:
                ready, _, _ = select.select([stdin], [], [], interval)
                data = os.read(stdin.fileno(), 65536) if ready else b""
            if ready:
                if not data:
                    break
                pending += data
                # split on newlines and carriage returns; a trailing "\r" chunk is a progress line
                while True:
                    nl, cr = pending.find(b"\n"), pending.find(b"\r")
                    if nl < 0 and cr < 0:
                        break
                    if nl >= 0 and (cr < 0 or nl < cr):
                        chunk, pending = pending[:nl], pending[nl + 1:]
                        feed(screen, chunk.decode("utf-8", "replace"), False)
                    else:
                        chunk, pending = pending[:cr], pending[cr + 1:]
                        if pending[:1] == b"\n":
                            pending = pending[1:]
                            feed(screen, chunk.decode("utf-8", "replace"), False)
                        else:
                            feed(screen, chunk.decode("utf-8", "replace"), True)
            now = time.monotonic()
            if now - last_draw >= interval:
                present()
                last_draw = now
        if pending.strip():
            feed(screen, pending.decode("utf-8", "replace"), False)
        screen.done = True
        screen.percent = 100
        present()
        if args.render:
            with open(args.render, "wb") as f:
                f.write(b"P6\n%d %d\n255\n" % (width, height))
                out = bytearray()
                for y in range(height):
                    for x in range(width):
                        off = y * stride + x * 4
                        v = struct.unpack_from("<I", canvas.buf, off)[0]
                        out += bytes(((v >> 16) & 255, (v >> 8) & 255, v & 255))
                f.write(out)
    finally:
        if tty is not None:
            if not args.keep_graphics:
                try:
                    fcntl.ioctl(tty, KDSETMODE, KD_TEXT)
                except OSError:
                    pass
            os.close(tty)
    return 0


if __name__ == "__main__":
    sys.exit(main())
