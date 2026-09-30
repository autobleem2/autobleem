//
// The carousel's timing on tweens (autobleem-core docs/ab-gui-plan.md, step G5o5): src/code/evoui/carousel_motion.h
// against the hand-written timer it replaced, position for position.
//
// The Carousel itself needs a live Gui (textures, the cover loader), so, like test_set_picker_layout.cpp, this runs a
// small local copy of its bookkeeping - the row of covers, setInitialPositions, scrollLeft/Right, moveMainCover/
// snapMainCover, updateVisibility, and the launcher loop's input rules (a tap, the tap queued during a scroll, the held
// stick chaining after CarouselHoldDelay, the end of the row) - twice: once timed by the OLD code (copied here as it
// was before G5o5: animationStart/animationDuration/eased per cover, delta = elapsed / duration, easeOutCubic while
// delta < 1, the destination once delta > 1), once by the NEW code - carousel_motion.h itself (stepStart, Moves, the
// run, advance) on a real abgui::Tweens with a clock the test sets. The same scripted input is played into both at
// the same frame times (every millisecond, 60 and 30 fps, and a jittery schedule), and every cover's place (x, y,
// scale, shade, angle - compared exactly, not approximately), whether it is visible and moving, the row's `scrolling`
// and the selection must be the same at every frame. The new side also checks the DebugDriver's busy rule: busy (and
// the frame need Active) while a cover moves, not busy once everything rests.
//
#include "doctest/doctest.h"

#include "core/model/timing.h"
#include "evoui/carousel_motion.h"

#include <ab_gui/tween.h>
#include <ableem/ui/debug_driver.h>

#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <string>
#include <vector>

using namespace std;
using ableem::DebugDriver;

namespace {

// PsScreenpoint's fields, with its types
struct Point {
    int x = 0;
    int y = 0;
    float scale = 0.0f;
    int shade = 0;
    float angle = 0.0f;
};

// within 1e-3 (a thousandth of a pixel): the Release build fuses the float operations differently from Debug, so
// the old timer's inline formula and the tween's out-of-line one may differ in the last bits
bool near(float a, float b) {
    return std::fabs(a - b) <= 1e-3f;
}

// x/y are whole pixels rounded from a float: a last-bit difference can flip one across .5 on a single mid-move
// frame (seen: linear, 110 ms, 35 ms in, Release) - one pixel for one frame, never at rest
bool nearPx(int a, int b) {
    return std::abs(a - b) <= 1;
}

bool samePoint(const Point &a, const Point &b) {
    return nearPx(a.x, b.x) && nearPx(a.y, b.y) && near(a.scale, b.scale) && near(a.shade, b.shade) &&
           near(a.angle, b.angle);
}

// one cover, with both timers' fields: the old side uses the first three, the new side `move`
struct Cover {
    Point current, destination, actual;
    int screenPointIndex = -1;
    bool visible = false;
    long animationStart = 0;
    long animationDuration = 0;
    bool eased = true;
    CarouselMotion::MoveRef move;
};

// the real slot layout: PsCarousel::createCoverPoint and initCoverPositions (carousel_game.cpp), copied
const int SideCovers = 14;
const int SlotCount = 2 * SideCovers + 1;
const int MiddleSlot = SideCovers;

Point createCoverPoint(int distance, int side) {
    static const float turnByDistance[] = {0, 40, 52, 60, 66, 70, 72};
    const float turn = distance <= 6 ? turnByDistance[distance] : 72.0f;
    const float nearestScale = 0.5f, shrinkPerCover = 0.035f;
    const int nearestOffset = 190, nearestStep = 50;
    const int nearestShade = 255, darkenPerCover = 15;
    const int middleY = 100 + static_cast<int>(226 * nearestScale) / 2;
    float scale = nearestScale;
    int offset = nearestOffset;
    for (int d = 2; d <= distance; d++) {
        scale = nearestScale * (1.0f - shrinkPerCover * (d - 1));
        offset += static_cast<int>(nearestStep * scale / nearestScale);
    }
    const int boxWidth = static_cast<int>(226 * scale);
    Point point;
    point.scale = scale;
    point.shade = nearestShade - darkenPerCover * (distance - 1);
    point.y = middleY - boxWidth / 2;
    if (side == 0) {
        point.x = 640 - offset - boxWidth / 2;
        point.angle = -turn;
    } else {
        point.x = 640 + offset - boxWidth / 2;
        point.angle = turn;
    }
    return point;
}

vector<Point> coverPositions() {
    vector<Point> slots;
    for (int distance = SideCovers; distance >= 1; distance--)
        slots.push_back(createCoverPoint(distance, 0));
    Point middle;
    middle.x = 640 - 113;
    middle.y = 180;
    middle.scale = 1;
    middle.shade = 255;
    slots.push_back(middle);
    for (int distance = 1; distance <= SideCovers; distance++)
        slots.push_back(createCoverPoint(distance, 1));
    return slots;
}

// Carousel's mainCoverPoint
Point mainCoverPoint(bool toGamesRow) {
    Point point;
    point.x = 640 - 113;
    point.y = toGamesRow ? 180 : 90;
    point.scale = 1;
    point.shade = toGamesRow ? 255 : 220;
    return point;
}

//*******************************
// the two timers
//*******************************
// The old one, as carousel.cpp had it before G5o5
struct OldTiming {
    long now = 0;

    long stepStart(long &chainEnd, int speed, bool eased) {
        long start = now;
        if (!eased && chainEnd != 0 && now >= chainEnd && now - chainEnd < speed)
            start = chainEnd;
        chainEnd = eased ? 0 : start + speed;
        return start;
    }
    struct Move {
        long start;
        int duration;
        bool eased;
    };
    Move begin(long start, int duration, bool eased) { return Move{start, duration, eased}; }
    void assign(Cover &game, const Move &move) {
        game.animationDuration = move.duration;
        game.animationStart = move.start;
        game.eased = move.eased;
    }
    void beginFrame() {}
    // Carousel::updatePositions' body for one visible cover
    void advance(Cover &game) {
        if (game.animationStart != 0) {
            long position = now - game.animationStart;
            float delta = position * 1.0f / game.animationDuration;
            if (game.eased && delta < 1.0f)
                delta = easeOutCubic(delta);
            game.actual.x = game.current.x + (game.destination.x - game.current.x) * delta;
            game.actual.y = game.current.y + (game.destination.y - game.current.y) * delta;
            game.actual.scale = game.current.scale + (game.destination.scale - game.current.scale) * delta;
            game.actual.shade = game.current.shade + (game.destination.shade - game.current.shade) * delta;
            game.actual.angle = game.current.angle + (game.destination.angle - game.current.angle) * delta;
            if (delta > 1.0f) {
                game.actual = game.destination;
                game.current = game.destination;
                game.animationStart = 0;
            }
        }
    }
    bool moving(const Cover &game) const { return game.animationStart != 0; }
    void markMain(const Move &) {}
    void snap(Cover &game) { game.animationStart = 0; }
};

// The new one: carousel_motion.h on a Tweens with the test's clock, as carousel.cpp uses it
struct NewTiming {
    long now = 0;
    abgui::Tweens tweens;
    CarouselMotion::Moves moves;
    CarouselMotion::MoveRef mainMove;
    abgui::TweenOwner owner; // after the floats, as in Carousel

    NewTiming() {
        tweens.clock = [this]() { return static_cast<unsigned int>(now); };
    }
    long stepStart(long &chainEnd, int speed, bool eased) {
        return CarouselMotion::stepStart(now, chainEnd, speed, eased);
    }
    using Move = CarouselMotion::MoveRef;
    Move begin(long start, int duration, bool eased) {
        return moves.start(tweens, owner, static_cast<unsigned int>(start), static_cast<unsigned int>(duration), eased);
    }
    void assign(Cover &game, const Move &move) { game.move = move; }
    void beginFrame() { tweens.update(); }
    void advance(Cover &game) { CarouselMotion::advance(game, moves, tweens); }
    bool moving(const Cover &game) const { return game.move.set(); }
    void markMain(const Move &move) { mainMove = move; }
    void snap(Cover &game) {
        if (game.move.set() && game.move.id == mainMove.id)
            moves.cancel(tweens, mainMove);
        game.move = CarouselMotion::MoveRef();
    }
};

//*******************************
// the row (Carousel's bookkeeping) over a timer
//*******************************
template <class Timing> struct Row {
    Timing timing;
    vector<Point> positions = coverPositions();
    vector<Cover> games;
    int selected = 0;
    bool scrolling = false;
    long chainEnd = 0;

    explicit Row(int count) : games(static_cast<size_t>(count)) { setInitialPositions(0); }

    bool canSelectNext() const { return selected + 1 < static_cast<int>(games.size()); }
    bool canSelectPrevious() const { return selected > 0; }

    void setInitialPositions(int selectedIndex) {
        for (auto &item : games)
            item.visible = false;
        for (int slot = 0; slot < SlotCount; slot++) {
            const int index = selectedIndex + slot - MiddleSlot;
            if (index < 0 || index >= static_cast<int>(games.size()))
                continue;
            games[index].current = positions[slot];
            games[index].visible = true;
            games[index].screenPointIndex = slot;
        }
        for (auto &item : games) {
            item.actual = item.current;
            item.destination = item.current;
        }
    }

    void scroll(bool left, int speed, bool eased) {
        scrolling = true;
        const typename Timing::Move move = timing.begin(timing.stepStart(chainEnd, speed, eased), speed, eased);
        for (auto &game : games) {
            if (!game.visible)
                continue;
            int nextIndex = game.screenPointIndex;
            if (left) {
                if (game.screenPointIndex != 0)
                    nextIndex = game.screenPointIndex - 1;
                else
                    game.visible = false;
            } else {
                if (game.screenPointIndex != SlotCount - 1)
                    nextIndex = game.screenPointIndex + 1;
                else
                    game.visible = false;
            }
            game.destination = positions[nextIndex];
            timing.assign(game, move);
            game.screenPointIndex = nextIndex;
            game.current = game.actual;
        }
    }

    void moveMainCover(bool toGamesRow) {
        games[selected].destination = mainCoverPoint(toGamesRow);
        const typename Timing::Move move = timing.begin(timing.now, 200, true);
        timing.markMain(move);
        timing.assign(games[selected], move);
    }

    void snapMainCover(bool toGamesRow) {
        const Point point = mainCoverPoint(toGamesRow);
        games[selected].destination = point;
        games[selected].actual = point;
        games[selected].current = point;
        timing.snap(games[selected]);
    }

    void updatePositions() {
        timing.beginFrame();
        for (auto &game : games)
            if (game.visible)
                timing.advance(game);
        bool allFinished = true;
        for (const auto &game : games)
            if (timing.moving(game) && game.visible)
                allFinished = false;
        if (allFinished && scrolling) {
            setInitialPositions(selected);
            scrolling = false;
        }
    }

    bool animating() const {
        if (scrolling)
            return true;
        for (const auto &game : games)
            if (game.visible && timing.moving(game))
                return true;
        return false;
    }
};

//*******************************
// the script and the launcher loop's input rules
//*******************************
enum class Ev { PressRight, PressLeft, Release, RaiseCover, LowerCover, SnapRaised };
struct Event {
    long at;
    Ev what;
};

// one frame's record: every cover and the row's state
struct Frame {
    long t;
    vector<Point> actual;
    vector<bool> visible, moving;
    bool scrolling;
    int selected;
};

template <class Timing> struct Launcher {
    Row<Timing> row;
    long motionStart = 0;
    int motionDir = 0;
    int queuedScroll = 0;
    int dpad = 0; // the live d-pad: 1 right, -1 left, 0 centred

    explicit Launcher(int count) : row(count) {}

    void nextGame(int speed, bool eased = true) {
        if (!row.canSelectNext()) {
            motionStart = 0;
            return;
        }
        row.scroll(true, speed, eased);
        row.selected++;
    }
    void prevGame(int speed, bool eased = true) {
        if (!row.canSelectPrevious()) {
            motionStart = 0;
            return;
        }
        row.scroll(false, speed, eased);
        row.selected--;
    }

    // one pass of GuiLauncher::loop at time t, the script's events due by then handled after the chaining
    void pass(long t, const vector<Event> &script, size_t &next) {
        row.timing.now = t;
        row.updatePositions();
        if (motionStart != 0 && dpad == 0)
            motionStart = 0;
        if (!row.scrolling) {
            if (queuedScroll != 0) {
                if (queuedScroll > 0)
                    nextGame(CarouselScrollDuration);
                else
                    prevGame(CarouselScrollDuration);
                queuedScroll = 0;
            } else if (motionStart != 0 && t - motionStart > CarouselHoldDelay) {
                if (motionDir == 0)
                    nextGame(CarouselHeldScrollDuration, false);
                else
                    prevGame(CarouselHeldScrollDuration, false);
            }
        }
        while (next < script.size() && script[next].at <= t) {
            const Ev what = script[next].what;
            ++next;
            if (what == Ev::PressRight || what == Ev::PressLeft) {
                const bool right = what == Ev::PressRight;
                dpad = right ? 1 : -1;
                if (!row.scrolling) {
                    motionStart = t;
                    motionDir = right ? 0 : 1;
                    if (right)
                        nextGame(CarouselScrollDuration);
                    else
                        prevGame(CarouselScrollDuration);
                } else {
                    queuedScroll = right ? 1 : -1;
                }
            } else if (what == Ev::Release) {
                dpad = 0;
                motionStart = 0;
            } else if (what == Ev::RaiseCover) {
                row.moveMainCover(false);
            } else if (what == Ev::LowerCover) {
                row.moveMainCover(true);
            } else if (what == Ev::SnapRaised) {
                row.snapMainCover(false);
            }
        }
    }

    Frame record(long t) const {
        Frame f;
        f.t = t;
        for (const auto &game : row.games) {
            f.actual.push_back(game.actual);
            f.visible.push_back(game.visible);
            f.moving.push_back(row.timing.moving(game));
        }
        f.scrolling = row.scrolling;
        f.selected = row.selected;
        return f;
    }
};

// the frame times: every ms, 60 fps (16/17 ms), 30 fps, and a jittery 1..40 ms schedule
vector<long> frameTimes(int schedule, long from, long to) {
    vector<long> ts;
    unsigned int seed = 12345u;
    long t = from;
    int k = 0;
    while (t <= to) {
        ts.push_back(t);
        switch (schedule) {
        case 0:
            t += 1;
            break;
        case 1:
            t += (k++ % 3 == 2) ? 16 : 17;
            break;
        case 2:
            t += 33;
            break;
        default:
            seed = seed * 1103515245u + 12345u;
            t += 1 + static_cast<long>((seed >> 16) % 40u);
            break;
        }
    }
    return ts;
}

string describe(const Point &p) {
    char buf[128];
    snprintf(buf, sizeof(buf), "(%d, %d, %.9g, %d, %.9g)", p.x, p.y, static_cast<double>(p.scale), p.shade,
             static_cast<double>(p.angle));
    return buf;
}

// plays the script into both timers and compares every frame; the new side's busy/frame-need rule at every frame
void compare(const char *name, int games, long base, const vector<Event> &relative, long lengthMs) {
    vector<Event> script;
    for (const Event &e : relative)
        script.push_back(Event{base + e.at, e.what});
    const int level = DebugDriver::busyLevel();
    for (int schedule = 0; schedule < 4; ++schedule) {
        CAPTURE(name);
        CAPTURE(schedule);
        {
            Launcher<OldTiming> old(games);
            Launcher<NewTiming> neu(games);
            size_t oldNext = 0, newNext = 0;
            int framesMoving = 0;
            for (long t : frameTimes(schedule, base - 50, base + lengthMs)) {
                old.pass(t, script, oldNext);
                neu.pass(t, script, newNext);
                const Frame a = old.record(t), b = neu.record(t);
                bool same = a.scrolling == b.scrolling && a.selected == b.selected;
                string where;
                for (size_t i = 0; i < a.actual.size() && same; ++i) {
                    if (!samePoint(a.actual[i], b.actual[i]) || a.visible[i] != b.visible[i] ||
                        a.moving[i] != b.moving[i]) {
                        same = false;
                        where =
                            "cover " + to_string(i) + " old " + describe(a.actual[i]) + " new " + describe(b.actual[i]);
                    }
                }
                CAPTURE(t - base);
                CAPTURE(where);
                REQUIRE(same);
                // the DebugDriver's busy: held while anything moves, so `wait_ready` waits the carousel out
                if (neu.row.animating()) {
                    ++framesMoving;
                    bool coverMoving = false;
                    for (size_t i = 0; i < b.moving.size(); ++i)
                        coverMoving = coverMoving || (b.visible[i] && b.moving[i]);
                    if (coverMoving) {
                        REQUIRE(neu.row.timing.tweens.busy());
                        REQUIRE(neu.row.timing.tweens.frameNeed() == ableem::Input::FrameNeed::Active);
                        REQUIRE(DebugDriver::busyLevel() == level + 1);
                    }
                }
            }
            CHECK(framesMoving > 0);
            // at rest once the script is over: not moving, not busy, the DebugDriver's step given back
            CHECK_FALSE(neu.row.animating());
            CHECK_FALSE(neu.row.timing.tweens.busy());
            CHECK(DebugDriver::busyLevel() == level);
        }
        CHECK(DebugDriver::busyLevel() == level);
    }
}

} // namespace

TEST_CASE("the run gives the old timer's fraction at every millisecond: eased and linear, each duration") {
    for (int duration : {CarouselScrollDuration, CarouselHeldScrollDuration, 200, 1, 7}) {
        for (bool eased : {true, false}) {
            for (long start : {1000L, 1L, 3600000L}) {
                CAPTURE(duration);
                CAPTURE(eased);
                CAPTURE(start);
                OldTiming oldT;
                NewTiming newT;
                Cover a, b;
                a.current = b.current = createCoverPoint(3, 0);
                a.destination = b.destination = createCoverPoint(2, 0);
                a.actual = b.actual = a.current;
                oldT.now = newT.now = start;
                oldT.assign(a, oldT.begin(start, duration, eased));
                newT.assign(b, newT.begin(start, duration, eased));
                for (long e = 0; e <= duration + 3; ++e) {
                    oldT.now = newT.now = start + e;
                    oldT.advance(a);
                    newT.beginFrame();
                    newT.advance(b);
                    CAPTURE(e);
                    REQUIRE(samePoint(a.actual, b.actual));
                    REQUIRE(samePoint(a.current, b.current));
                    REQUIRE(oldT.moving(a) == newT.moving(b));
                    // at exactly the duration both draw the curve's end and still move; from the next tick on, rest
                    if (e == duration)
                        REQUIRE(newT.moving(b));
                    if (e > duration)
                        REQUIRE_FALSE(newT.moving(b));
                }
            }
        }
    }
}

TEST_CASE("a held stick's step starts where the last one ended (stepStart), as before") {
    long oldChain = 0, newChain = 0;
    OldTiming oldT;
    struct Case {
        long now;
        int speed;
        bool eased;
    };
    const Case cases[] = {{1000, 110, true}, {1110, 80, false}, {1195, 80, false}, {1260, 80, false},
                          {1400, 80, false}, {1405, 80, false}, {1480, 110, true}, {1600, 80, false},
                          {1680, 80, false}, {1759, 80, false}};
    for (const Case &c : cases) {
        oldT.now = c.now;
        CHECK(oldT.stepStart(oldChain, c.speed, c.eased) ==
              CarouselMotion::stepStart(c.now, newChain, c.speed, c.eased));
        CHECK(oldChain == newChain);
    }
}

TEST_CASE("a tap: one eased step, the same positions at every frame") {
    compare("tap right", 40, 1000, {{0, Ev::PressRight}, {60, Ev::Release}}, 600);
    compare("tap left", 40, 1000, {{0, Ev::PressRight}, {40, Ev::Release}, {300, Ev::PressLeft}, {350, Ev::Release}},
            900);
    compare("tap at an hour's uptime", 40, 3600000, {{0, Ev::PressRight}, {60, Ev::Release}}, 600);
}

TEST_CASE("a queued tap: pressed during a scroll, it runs the moment the scroll ends") {
    compare("queued tap", 40, 1000, {{0, Ev::PressRight}, {30, Ev::Release}, {60, Ev::PressRight}, {80, Ev::Release}},
            800);
    compare("three quick taps", 40, 1000,
            {{0, Ev::PressRight},
             {20, Ev::Release},
             {50, Ev::PressRight},
             {70, Ev::Release},
             {140, Ev::PressRight},
             {150, Ev::Release}},
            900);
}

TEST_CASE("a held stick: linear steps chained with no pause, to the end of the row") {
    compare("held right", 60, 1000, {{0, Ev::PressRight}, {1500, Ev::Release}}, 2200);
    compare("held into the row's end", 12, 1000, {{0, Ev::PressRight}, {2000, Ev::Release}}, 2600);
    compare("held left from the middle", 60, 1000,
            {{0, Ev::PressRight}, {1200, Ev::Release}, {1500, Ev::PressLeft}, {2400, Ev::Release}}, 3000);
}

TEST_CASE("a direction change mid-move: the other way queued during a held run, and during a tap") {
    compare("held right, left pressed mid-step", 60, 1000,
            {{0, Ev::PressRight}, {900, Ev::PressLeft}, {1400, Ev::Release}}, 2200);
    compare("tap right, tap left mid-move", 40, 1000,
            {{0, Ev::PressRight}, {30, Ev::Release}, {50, Ev::PressLeft}, {70, Ev::Release}}, 800);
    compare("released and pressed the other way between steps", 60, 1000,
            {{0, Ev::PressRight}, {700, Ev::Release}, {701, Ev::PressLeft}, {1300, Ev::Release}}, 2000);
}

TEST_CASE("the main cover: raised, lowered mid-move, a scroll during its move, a snap") {
    compare("raise then lower", 40, 1000, {{0, Ev::RaiseCover}, {400, Ev::LowerCover}}, 900);
    compare("lowered mid-raise", 40, 1000, {{0, Ev::RaiseCover}, {90, Ev::LowerCover}}, 700);
    compare("a scroll during the lowering", 40, 1000,
            {{0, Ev::RaiseCover}, {300, Ev::LowerCover}, {350, Ev::PressRight}, {380, Ev::Release}}, 1000);
    compare("snapped mid-raise", 40, 1000, {{0, Ev::RaiseCover}, {120, Ev::SnapRaised}, {400, Ev::LowerCover}}, 900);
}
