//
// cd_glyphs.h (evoui/screens/, pure): the per-row runs of the Repeat and Shuffle glyphs.
//
#include "doctest/doctest.h"

#include "cd_glyphs.h"

using namespace cdplayer;

namespace {
bool covers(const Spans &runs, int x) {
    for (const Span &s : runs)
        if (x >= s.x0 && x < s.x1)
            return true;
    return false;
}
} // namespace

TEST_CASE("spanOf: the pixels whose centres are inside") {
    CHECK(spanOf(2.0, 5.0).x0 == 2);
    CHECK(spanOf(2.0, 5.0).x1 == 5);
    CHECK(spanOf(2.6, 2.9).x1 == spanOf(2.6, 2.9).x0);
}

TEST_CASE("ringRuns: two runs through the sides, one through the top, none outside") {
    // a 40x40 ring, corner radius 10, width 6
    CHECK(ringRuns(-1.0, 0, 0, 40, 40, 10, 6).empty());
    const Spans mid = ringRuns(20.5, 0, 0, 40, 40, 10, 6);
    REQUIRE(mid.size() == 2);
    CHECK(mid[0].x0 == 0);
    CHECK(mid[0].x1 == 6);
    CHECK(mid[1].x0 == 34);
    CHECK(mid[1].x1 == 40);
    const Spans top = ringRuns(2.5, 0, 0, 40, 40, 10, 6);
    REQUIRE(top.size() == 1);
    CHECK(top[0].x0 > 0); // the rounded corner takes the first pixels
    CHECK(top[0].x1 < 40);
    CHECK(top[0].x1 - top[0].x0 > 20);
}

TEST_CASE("polygonRun: a triangle narrows to its tip, either winding") {
    const Pt cw[3] = {{0, 0}, {10, 5}, {0, 10}};
    const Pt ccw[3] = {{0, 0}, {0, 10}, {10, 5}};
    for (const Pt *p : {cw, ccw}) {
        double lo, hi;
        REQUIRE(polygonRun(5.0, p, 3, lo, hi));
        CHECK(lo == doctest::Approx(0.0));
        CHECK(hi == doctest::Approx(10.0));
        REQUIRE(polygonRun(1.0, p, 3, lo, hi));
        CHECK(hi == doctest::Approx(2.0));
        CHECK_FALSE(polygonRun(11.0, p, 3, lo, hi));
    }
}

TEST_CASE("cutRuns and mergeRuns") {
    const Spans cut = cutRuns(Spans{{0, 10}}, 3, 6);
    REQUIRE(cut.size() == 2);
    CHECK(cut[0].x1 == 3);
    CHECK(cut[1].x0 == 6);
    const Spans merged = mergeRuns(Spans{{8, 12}, {0, 3}, {3, 5}, {4, 4}});
    REQUIRE(merged.size() == 2);
    CHECK(merged[0].x0 == 0);
    CHECK(merged[0].x1 == 5);
    CHECK(merged[1].x0 == 8);
}

TEST_CASE("repeatRuns: a loop with a gap in its top edge, a '1' inside only for Track") {
    const int n = 80;
    // through the middle row: the loop's two sides, nothing between them for Disc, the "1" for Track
    const Spans disc = repeatRuns(n / 2, n, false);
    CHECK(covers(disc, 2));
    CHECK(covers(disc, n - 3));
    CHECK_FALSE(covers(disc, n / 2));
    CHECK(covers(repeatRuns(n / 2, n, true), n / 2));
    // nothing above and below the glyph
    CHECK(repeatRuns(0, n, false).empty());
    CHECK(repeatRuns(n - 1, n, false).empty());
    // the bottom edge is whole, the top edge has its gap (arrow head tip at 0.78, the tail from 0.84)
    const int bottom = static_cast<int>(0.86 * n) - 2;
    CHECK(covers(repeatRuns(bottom, n, false), n / 2));
    const int topRow = static_cast<int>(0.14 * n) + 1;
    CHECK(covers(repeatRuns(topRow, n, false), n / 4));
    CHECK_FALSE(covers(repeatRuns(topRow, n, false), static_cast<int>(0.81 * n)));
    // every row's runs are sorted and apart
    for (int row = 0; row < n; row++) {
        const Spans r = repeatRuns(row, n, true);
        for (size_t i = 1; i < r.size(); i++)
            CHECK(r[i].x0 > r[i - 1].x1);
    }
}

TEST_CASE("shuffleRuns: two arrows crossing, both ending on the right") {
    const int n = 80;
    // each line ends on the right in an arrow head, one on the upper row and one on the lower
    CHECK(covers(shuffleRuns(static_cast<int>(0.27 * n), n), n - 3));
    CHECK(covers(shuffleRuns(static_cast<int>(0.73 * n), n), n - 3));
    // at the crossing both strokes meet in the middle, at the left only the two stubs are apart
    CHECK(covers(shuffleRuns(n / 2, n), n * 3 / 8));
    CHECK(covers(shuffleRuns(static_cast<int>(0.27 * n), n), 2));
    CHECK_FALSE(covers(shuffleRuns(n / 2, n), 2));
    CHECK(shuffleRuns(0, n).empty());
}
