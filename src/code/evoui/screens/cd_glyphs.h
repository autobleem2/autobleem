//
// The CD player's Repeat and Shuffle symbols as geometry (evoui_cd_player.cpp draws them supersampled): for one row of
// an n x n picture, the runs of pixels the symbol covers. Shapes are worked out analytically per row - a ring between
// two rounded rectangles, thick strokes and arrow heads as convex polygons - so there is no SDL here and the rows
// can be tested alone.
//
#pragma once

#include <algorithm>
#include <cmath>
#include <vector>

namespace cdplayer {

// a run of pixels [x0, x1) in one row
struct Span {
    int x0, x1;
};
typedef std::vector<Span> Spans;

struct Pt {
    double x, y;
};

// the run of pixels whose centres lie in [lo, hi]; empty (x0 == x1) when none does
inline Span spanOf(double lo, double hi) {
    const int x0 = static_cast<int>(std::ceil(lo - 0.5));
    const int x1 = static_cast<int>(std::floor(hi - 0.5)) + 1;
    return Span{x0, std::max(x0, x1)};
}

// the part of a rounded rectangle (x0,y0)-(x1,y1) with corner radius r on the horizontal line y: false above/below it
inline bool roundedRectRun(double y, double x0, double y0, double x1, double y1, double r, double &lo, double &hi) {
    if (y < y0 || y > y1)
        return false;
    double dy = 0;
    if (y < y0 + r)
        dy = y0 + r - y;
    else if (y > y1 - r)
        dy = y - (y1 - r);
    const double inset = r > 0 ? r - std::sqrt(std::max(0.0, r * r - dy * dy)) : 0;
    lo = x0 + inset;
    hi = x1 - inset;
    return hi >= lo;
}

// a ring of width t between a rounded rectangle and the one t inside it, on the line y: up to two runs
inline Spans ringRuns(double y, double x0, double y0, double x1, double y1, double r, double t) {
    Spans out;
    double olo, ohi, ilo, ihi;
    if (!roundedRectRun(y, x0, y0, x1, y1, r, olo, ohi))
        return out;
    if (roundedRectRun(y, x0 + t, y0 + t, x1 - t, y1 - t, std::max(0.0, r - t), ilo, ihi)) {
        out.push_back(spanOf(olo, ilo));
        out.push_back(spanOf(ihi, ohi));
    } else {
        out.push_back(spanOf(olo, ohi));
    }
    return out;
}

// a convex polygon on the line y: the pixel centres between its edges
inline bool polygonRun(double y, const Pt *p, int count, double &lo, double &hi) {
    double area = 0;
    for (int i = 0; i < count; i++) {
        const Pt &a = p[i], &b = p[(i + 1) % count];
        area += a.x * b.y - b.x * a.y;
    }
    const double sign = area < 0 ? -1.0 : 1.0;
    lo = -1e9;
    hi = 1e9;
    for (int i = 0; i < count; i++) {
        const Pt &a = p[i], &b = p[(i + 1) % count];
        // the inside is where cross(b - a, p - a) * sign >= 0, which on this line reads c0 + cx * x >= 0
        const double cx = -(b.y - a.y) * sign;
        const double c0 = ((b.x - a.x) * (y - a.y) + (b.y - a.y) * a.x) * sign;
        if (cx > 1e-12) {
            lo = std::max(lo, -c0 / cx);
        } else if (cx < -1e-12) {
            hi = std::min(hi, -c0 / cx);
        } else if (c0 < 0) {
            return false;
        }
    }
    return hi >= lo;
}

// a straight stroke of width t from a to b (square ends) as a polygon of four points
inline void strokeQuad(Pt a, Pt b, double t, Pt out[4]) {
    const double dx = b.x - a.x, dy = b.y - a.y;
    const double len = std::sqrt(dx * dx + dy * dy);
    const double nx = -dy / len * t / 2, ny = dx / len * t / 2;
    out[0] = Pt{a.x + nx, a.y + ny};
    out[1] = Pt{b.x + nx, b.y + ny};
    out[2] = Pt{b.x - nx, b.y - ny};
    out[3] = Pt{a.x - nx, a.y - ny};
}

inline void addPolygon(Spans &out, double y, const Pt *p, int count) {
    double lo, hi;
    if (polygonRun(y, p, count, lo, hi))
        out.push_back(spanOf(lo, hi));
}

inline void addStroke(Spans &out, double y, Pt a, Pt b, double t) {
    Pt q[4];
    strokeQuad(a, b, t, q);
    addPolygon(out, y, q, 4);
}

// a disc of radius r at c on the line y (the round joint of two strokes)
inline void addDisc(Spans &out, double y, Pt c, double r) {
    const double dy = y - c.y;
    if (dy * dy > r * r)
        return;
    const double w = std::sqrt(r * r - dy * dy);
    out.push_back(spanOf(c.x - w, c.x + w));
}

// an arrow head pointing right: base at x = baseX centred on cy, half the base's height h, the tip at tipX
inline void addArrowHead(Spans &out, double y, double baseX, double tipX, double cy, double h) {
    const Pt tri[3] = {{baseX, cy - h}, {tipX, cy}, {baseX, cy + h}};
    addPolygon(out, y, tri, 3);
}

// removes [x0, x1) from every run
inline Spans cutRuns(const Spans &in, int x0, int x1) {
    Spans out;
    for (const Span &s : in) {
        if (s.x1 <= x0 || s.x0 >= x1) {
            out.push_back(s);
            continue;
        }
        if (s.x0 < x0)
            out.push_back(Span{s.x0, x0});
        if (s.x1 > x1)
            out.push_back(Span{x1, s.x1});
    }
    return out;
}

// sorted, empty runs dropped, touching and overlapping runs joined
inline Spans mergeRuns(Spans in) {
    in.erase(std::remove_if(in.begin(), in.end(), [](const Span &s) { return s.x1 <= s.x0; }), in.end());
    std::sort(in.begin(), in.end(), [](const Span &a, const Span &b) { return a.x0 < b.x0; });
    Spans out;
    for (const Span &s : in) {
        if (!out.empty() && s.x0 <= out.back().x1)
            out.back().x1 = std::max(out.back().x1, s.x1);
        else
            out.push_back(s);
    }
    return out;
}

// Repeat: a loop (a rounded ring) whose top edge ends in an arrow head pointing right across a gap; with `one`
// (Repeat = Track) a "1" in the middle. `row` of an n x n picture.
inline Spans repeatRuns(int row, int n, bool one) {
    const double y = row + 0.5, k = n;
    const double top = 0.14 * k, bottom = 0.86 * k, t = 0.15 * k;
    Spans out = ringRuns(y, 0.0, top, k, bottom, 0.26 * k, t);
    if (y < top + 1.2 * t)
        out = cutRuns(out, static_cast<int>(std::lround(0.46 * k)), static_cast<int>(std::lround(0.84 * k)));
    addArrowHead(out, y, 0.46 * k, 0.78 * k, top + t / 2, 0.17 * k);
    if (one) {
        addStroke(out, y, Pt{0.5 * k, 0.34 * k}, Pt{0.5 * k, 0.66 * k}, 0.10 * k);
        addStroke(out, y, Pt{0.35 * k, 0.43 * k}, Pt{0.47 * k, 0.35 * k}, 0.08 * k);
    }
    return mergeRuns(out);
}

// Shuffle: two strokes crossing, left to right, each ending in an arrow head on the right. `row` of an n x n picture.
inline Spans shuffleRuns(int row, int n) {
    const double y = row + 0.5, k = n;
    const double t = 0.12 * k, hi = 0.27 * k, lo = 0.73 * k;
    Spans out;
    for (int line = 0; line < 2; line++) {
        const double ya = line == 0 ? hi : lo, yb = line == 0 ? lo : hi;
        const Pt p0{0.0, ya}, p1{0.20 * k, ya}, p2{0.55 * k, yb}, p3{0.74 * k, yb};
        addStroke(out, y, p0, p1, t);
        addStroke(out, y, p1, p2, t);
        addStroke(out, y, p2, p3, t);
        addDisc(out, y, p1, t / 2);
        addDisc(out, y, p2, t / 2);
        addArrowHead(out, y, 0.72 * k, 1.0 * k, yb, 0.17 * k);
    }
    return mergeRuns(out);
}

} // namespace cdplayer
