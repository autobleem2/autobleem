//
// A dev tool, not part of the test run: renders the CD player's Repeat (off the Track tag, Disc and Track) and Shuffle
// glyphs like the screen does - 4x supersampled from evoui/screens/cd_glyphs.h, halved twice - next to the old
// rectangle-only drawing, at three key sizes, into a PNG (8x enlarged, nearest, so the edges can be judged).
//   g++ -std=c++14 -O1 -I src/code/evoui/screens tests/tools/cd_glyph_png.cpp -o cd_glyph_png && ./cd_glyph_png out.png
//
#include "cd_glyphs.h"

#include <cstdint>
#include <cstdio>
#include <string>

using namespace cdplayer;

namespace {
typedef std::vector<unsigned char> Bytes;

struct Image {
    int w, h;
    Bytes rgb;
    Image(int w_, int h_) : w(w_), h(h_), rgb(static_cast<size_t>(w_ * h_ * 3), 22) {}
    void put(int x, int y, int r, int g, int b) {
        if (x < 0 || y < 0 || x >= w || y >= h)
            return;
        unsigned char *p = &rgb[static_cast<size_t>((y * w + x) * 3)];
        p[0] = static_cast<unsigned char>(r);
        p[1] = static_cast<unsigned char>(g);
        p[2] = static_cast<unsigned char>(b);
    }
};

uint32_t crcOf(const Bytes &d, size_t from) {
    uint32_t c = 0xffffffffu;
    for (size_t i = from; i < d.size(); i++) {
        c ^= d[i];
        for (int k = 0; k < 8; k++)
            c = (c >> 1) ^ (0xedb88320u & (0u - (c & 1u)));
    }
    return ~c;
}

void put32(Bytes &b, uint32_t v) {
    for (int i = 3; i >= 0; i--)
        b.push_back(static_cast<unsigned char>(v >> (8 * i)));
}

void chunk(Bytes &out, const char *type, const Bytes &data) {
    Bytes c;
    put32(out, static_cast<uint32_t>(data.size()));
    c.insert(c.end(), type, type + 4);
    c.insert(c.end(), data.begin(), data.end());
    out.insert(out.end(), c.begin(), c.end());
    put32(out, crcOf(c, 0));
}

bool writePng(const Image &img, const std::string &path) {
    Bytes raw;
    for (int y = 0; y < img.h; y++) {
        raw.push_back(0);
        raw.insert(raw.end(), img.rgb.begin() + y * img.w * 3, img.rgb.begin() + (y + 1) * img.w * 3);
    }
    Bytes z = {0x78, 0x01};
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < raw.size(); i += 65535) {
        const size_t n = std::min<size_t>(65535, raw.size() - i);
        z.push_back(i + n >= raw.size() ? 1 : 0);
        z.push_back(static_cast<unsigned char>(n & 255));
        z.push_back(static_cast<unsigned char>(n >> 8));
        z.push_back(static_cast<unsigned char>(~n & 255));
        z.push_back(static_cast<unsigned char>((~n >> 8) & 255));
        z.insert(z.end(), raw.begin() + i, raw.begin() + i + n);
    }
    for (unsigned char c : raw) {
        a = (a + c) % 65521;
        b = (b + a) % 65521;
    }
    put32(z, (b << 16) | a);
    Bytes png = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    Bytes head;
    put32(head, static_cast<uint32_t>(img.w));
    put32(head, static_cast<uint32_t>(img.h));
    head.insert(head.end(), {8, 2, 0, 0, 0});
    chunk(png, "IHDR", head);
    chunk(png, "IDAT", z);
    chunk(png, "IEND", Bytes());
    FILE *f = fopen(path.c_str(), "wb");
    if (!f)
        return false;
    fwrite(png.data(), 1, png.size(), f);
    fclose(f);
    return true;
}

typedef std::vector<float> Plane; // coverage 0..1, n x n

Plane rasterise(int n, int kind) { // kind 0 shuffle, 1 repeat disc, 2 repeat track
    Plane p(static_cast<size_t>(n * n), 0.0f);
    for (int row = 0; row < n; row++) {
        const Spans runs = kind == 0 ? shuffleRuns(row, n) : repeatRuns(row, n, kind == 2);
        for (const Span &s : runs)
            for (int x = std::max(0, s.x0); x < std::min(n, s.x1); x++)
                p[static_cast<size_t>(row * n + x)] = 1.0f;
    }
    return p;
}

Plane halve(const Plane &in, int n) {
    Plane out(static_cast<size_t>(n / 2 * (n / 2)));
    for (int y = 0; y < n / 2; y++)
        for (int x = 0; x < n / 2; x++)
            out[static_cast<size_t>(y * (n / 2) + x)] =
                (in[static_cast<size_t>(2 * y * n + 2 * x)] + in[static_cast<size_t>(2 * y * n + 2 * x + 1)] +
                 in[static_cast<size_t>((2 * y + 1) * n + 2 * x)] + in[static_cast<size_t>((2 * y + 1) * n + 2 * x + 1)]) /
                4;
    return out;
}

// the old drawing: fillRect-only, as drawGlyph did it
Plane oldGlyph(int s, int kind) {
    Plane p(static_cast<size_t>(s * s), 0.0f);
    auto fill = [&](int x, int y, int w, int h) {
        for (int j = y; j < y + h; j++)
            for (int i = x; i < x + w; i++)
                if (i >= 0 && j >= 0 && i < s && j < s)
                    p[static_cast<size_t>(j * s + i)] = 1.0f;
    };
    auto tri = [&](int bx, int by, int bw, int bh) {
        for (int row = 0; row < bh; row++) {
            const int half = bh / 2, dist = row < half ? row : bh - 1 - row;
            fill(bx, by + row, std::max(1, bw * (dist + 1) / (half + 1)), 1);
        }
    };
    const int t = std::max(2, s / 6), c = s / 2, half = s / 2;
    if (kind == 0) {
        for (int i = 0; i < s; i++) {
            fill(c - half + i, c - s / 3 + (s * 2 / 3) * i / s, t, t);
            fill(c - half + i, c + s / 3 - (s * 2 / 3) * i / s, t, t);
        }
        tri(c + half - t, c - s / 3 - t, s / 3, s / 3);
        tri(c + half - t, c + s / 3 - s / 3 + t, s / 3, s / 3);
    } else {
        fill(c - half, c - s / 3, s, t);
        fill(c - half, c + s / 3 - t, s, t);
        fill(c - half, c - s / 3, t, s * 2 / 3);
        fill(c + half - t, c - s / 3, t, s * 2 / 3);
        tri(c + half - t, c - half - t, s / 3, s / 3);
    }
    return p;
}

void blit(Image &img, const Plane &p, int s, int ox, int oy, int zoom) {
    for (int y = 0; y < s; y++)
        for (int x = 0; x < s; x++) {
            const float a = p[static_cast<size_t>(y * s + x)];
            const int v = static_cast<int>(22 + a * (235 - 22));
            for (int j = 0; j < zoom; j++)
                for (int i = 0; i < zoom; i++)
                    img.put(ox + x * zoom + i, oy + y * zoom + j, v, v, v);
        }
}
} // namespace

int main(int argc, char **argv) {
    const std::string path = argc > 1 ? argv[1] : "cd_glyphs.png";
    const int sizes[] = {14, 20, 30}; // key 56 at 480p-ish, 720p (the PSC), 1080p: s = key * 5 / 14
    const int zoom = 8, pad = 8;
    Image img(2 * (3 * 30 * zoom + 4 * pad) + 3 * pad, 3 * (30 * zoom + pad) + pad);
    int rowY = pad;
    for (int s : sizes) {
        for (int col = 0; col < 2; col++) { // 0 old, 1 new
            int x = pad + col * (3 * 30 * zoom + 4 * pad);
            for (int kind = 0; kind < 3; kind++) {
                Plane p = col == 0 ? oldGlyph(s, kind == 0 ? 0 : 1) : halve(halve(rasterise(4 * s, kind), 4 * s), 2 * s);
                blit(img, p, s, x, rowY, zoom);
                x += 30 * zoom + pad;
            }
        }
        rowY += 30 * zoom + pad;
    }
    return writePng(img, path) ? 0 : 1;
}
