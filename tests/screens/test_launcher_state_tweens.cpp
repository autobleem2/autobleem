//
// The launcher's state change on tweens (ab_gui G5o3, docs/ab-gui-plan.md): the menu row's slide (200 ms) and its
// option move and zoom (100 ms), the meta panel's slide (200 ms) and the settings band (100 ms) used to be four
// hand-written timers in PsMenu/PsMeta/PsSettingsBack::update(); they are non-ambient abgui::Tweens now, and the
// positions are evoui_motion.h's formulas over the eased progress.
//
// The controls need a live Gui, which no test host builds (the same reason as test_set_picker_layout.cpp), so this
// suite holds the two sides to each other: `Old*` is the frozen copy of the old update() code, `New*` mirrors the
// controls' applyProgress()/completeTransition()/slideTo() over a real Tweens on a settable clock and the real
// evomotion functions. Every millisecond of every transition (and a retarget half-way) must give the same numbers.
//
#include "doctest/doctest.h"

#include "core/model/timing.h"
#include "evoui_motion.h"

#include <ab_gui/tween.h>

#include <memory>

using abgui::Tween;
using abgui::TweenOwner;
using abgui::Tweens;

namespace {

constexpr int Start = 1000;
constexpr float MaxZoom = 1.5f;
constexpr float Gap = 130.0f;

float zoomOff(float scale) {
    return -(118.0f * scale - 118.0f) / 2.0f;
}

// a Tweens on a clock the test sets
struct Clocked {
    unsigned int now = Start;
    Tweens tweens;
    Clocked() {
        tweens.clock = [this]() { return now; };
    }
    void at(unsigned int t) {
        now = t;
        tweens.update();
    }
};

struct MenuState {
    float x = 640 - 118 / 2, y = 520, oy = 520, ox = 640 - 118 / 2;
    float xoff[4] = {0, 0, 0, 0};
    float yoff[4] = {0, 0, 0, 0};
    float scales[4] = {1, 1, 1, 1};
    int sel = 0;
};

void expectSame(const MenuState &a, const MenuState &b) {
    CHECK(a.x == b.x);
    CHECK(a.y == b.y);
    CHECK(a.ox == b.ox);
    CHECK(a.oy == b.oy);
    CHECK(a.sel == b.sel);
    for (int i = 0; i < 4; i++) {
        CHECK(a.scales[i] == b.scales[i]);
        CHECK(a.xoff[i] == b.xoff[i]);
        CHECK(a.yoff[i] == b.yoff[i]);
    }
}

//*******************************
// the old PsMenu::update, verbatim but for the member names
//*******************************
struct OldMenu : MenuState {
    int animationStarted = 0;
    int targety = 0;
    int duration = 0;
    bool active = false;
    int direction = 0;
    bool menuOn = true; // transition == TR_MENUON

    void update(long time) {
        if (animationStarted != 0) {
            float progress = time - animationStarted;
            progress = progress / (duration * 1.0f);
            if (progress > 1)
                progress = 1;
            if (progress < 0)
                progress = 0;
            progress = easeOutCubic(progress);

            if (menuOn) {
                y = oy + (progress * (targety - oy));

                if (active) {
                    scales[sel] = 1 + progress * (MaxZoom - 1);
                    xoff[sel] = zoomOff(scales[sel]);
                    yoff[sel] = zoomOff(scales[sel]);
                } else {
                    scales[sel] = 1 + (1 - progress) * (MaxZoom - 1);
                    xoff[sel] = zoomOff(scales[sel]);
                    yoff[sel] = zoomOff(scales[sel]);
                }

                if (progress == 1) {
                    oy = y;
                    animationStarted = 0;
                    if (active) {
                        scales[sel] = 1 + (MaxZoom - 1);
                        xoff[sel] = zoomOff(scales[sel]);
                        yoff[sel] = zoomOff(scales[sel]);
                    } else {
                        scales[sel] = 1;
                        xoff[sel] = zoomOff(scales[sel]);
                        yoff[sel] = zoomOff(scales[sel]);
                    }
                }
            } else {
                if (direction == 0) {
                    float progress = time - animationStarted;
                    progress = progress / (duration * 1.0f);
                    if (progress > 1)
                        progress = 1;
                    if (progress < 0)
                        progress = 0;
                    progress = easeOutCubic(progress);

                    x = ox + progress * Gap;

                    scales[sel] = 1 + (1 - progress) * (MaxZoom - 1);
                    xoff[sel] = zoomOff(scales[sel]);
                    yoff[sel] = zoomOff(scales[sel]);

                    if (progress >= 1.0f) {
                        scales[sel] = 1.0;
                        xoff[sel] = 0;
                        yoff[sel] = 0;

                        sel--;
                        scales[sel] = MaxZoom;
                        xoff[sel] = zoomOff(scales[sel]);
                        yoff[sel] = zoomOff(scales[sel]);

                        x = ox + Gap;
                        animationStarted = 0;
                        ox = x;
                    }
                } else {
                    float progress = time - animationStarted;
                    progress = progress / (duration * 1.0f);
                    if (progress > 1)
                        progress = 1;
                    if (progress < 0)
                        progress = 0;
                    progress = easeOutCubic(progress);

                    x = ox - progress * Gap;

                    scales[sel] = 1 + progress * (MaxZoom - 1);
                    xoff[sel] = zoomOff(scales[sel]);
                    yoff[sel] = zoomOff(scales[sel]);

                    if (progress >= 1.0f) {
                        scales[sel] = 1.0;
                        xoff[sel] = 0;
                        yoff[sel] = 0;
                        sel++;
                        scales[sel] = MaxZoom;
                        xoff[sel] = zoomOff(scales[sel]);
                        yoff[sel] = zoomOff(scales[sel]);
                        x = ox - Gap;
                        animationStarted = 0;
                        ox = x;
                    }
                }
            }
        }
    }
};

//*******************************
// the new PsMenu, as a model over the real Tweens and evomotion
//*******************************
struct NewMenu : MenuState {
    Clocked &clock;
    int targety = 0;
    unsigned int duration = 0;
    bool active = false;
    int direction = 0;
    bool menuOn = true;
    float progress_ = 0;
    bool moving_ = false;
    TweenOwner owner_;

    explicit NewMenu(Clocked &c) : clock(c) {}

    void setScale(int option, float scale) {
        scales[option] = scale;
        xoff[option] = evomotion::zoomOffset(scale);
        yoff[option] = evomotion::zoomOffset(scale);
    }
    void start() {
        owner_.cancel();
        progress_ = 0;
        moving_ = true;
        clock.tweens.start(Tween(progress_, 0.0f, 1.0f, duration).onEnd([this]() { complete(); }), owner_);
    }
    void apply() {
        if (!moving_)
            return;
        const float progress = progress_;
        if (menuOn) {
            y = evomotion::rowY(oy, targety, progress);
            setScale(sel,
                     active ? evomotion::openingScale(progress, MaxZoom) : evomotion::closingScale(progress, MaxZoom));
        } else if (direction == 0) {
            x = evomotion::optionX(0, ox, progress);
            setScale(sel, evomotion::closingScale(progress, MaxZoom));
        } else {
            x = evomotion::optionX(1, ox, progress);
            setScale(sel, evomotion::openingScale(progress, MaxZoom));
        }
    }
    void complete() {
        moving_ = false;
        if (menuOn) {
            y = evomotion::rowY(oy, targety, 1.0f);
            oy = y;
            setScale(sel, active ? 1 + (MaxZoom - 1) : 1.0f);
        } else if (direction == 0) {
            setScale(sel, 1.0f);
            xoff[sel] = 0;
            yoff[sel] = 0;
            sel--;
            setScale(sel, MaxZoom);
            x = ox + evomotion::IconGap;
            ox = x;
        } else {
            setScale(sel, 1.0f);
            xoff[sel] = 0;
            yoff[sel] = 0;
            sel++;
            setScale(sel, MaxZoom);
            x = ox - evomotion::IconGap;
            ox = x;
        }
    }
};

// both menus started the same way at Start; every millisecond until past the end must agree
void runMenu(OldMenu &old, NewMenu &fresh, Clocked &clock) {
    old.animationStarted = Start;
    fresh.start();
    const int length = old.duration + 40;
    for (int t = Start; t <= Start + length; t++) {
        old.update(t);
        clock.at(static_cast<unsigned int>(t));
        fresh.apply();
        expectSame(old, fresh);
        CHECK((old.animationStarted != 0) == fresh.moving_);
        // the tweens are busy exactly while the transition runs
        CHECK(clock.tweens.busy() == fresh.moving_);
    }
    CHECK_FALSE(fresh.moving_);
    CHECK_FALSE(clock.tweens.busy());
}

} // namespace

TEST_CASE("the evomotion lengths are the old timers' lengths") {
    CHECK(evomotion::MenuSlideMs == 200u);
    CHECK(evomotion::OptionMoveMs == 100u);
    CHECK(evomotion::MetaSlideMs == 200u);
    CHECK(evomotion::SettingsBandMs == 100u);
    CHECK(evomotion::IconGap == 130.0f);
    CHECK(evomotion::zoomOffset(1.0f) == zoomOff(1.0f));
    CHECK(evomotion::zoomOffset(1.5f) == zoomOff(1.5f));
}

TEST_CASE("a transition's tween is the old timer's eased progress, millisecond for millisecond") {
    for (unsigned int duration : {100u, 200u}) {
        Clocked clock;
        float progress = 0;
        TweenOwner owner;
        clock.tweens.start(Tween(progress, 0.0f, 1.0f, duration), owner);
        for (unsigned int ms = 0; ms <= duration + 30; ms++) {
            clock.at(Start + ms);
            float old = static_cast<float>(ms) / (duration * 1.0f);
            if (old > 1)
                old = 1;
            CHECK(progress == easeOutCubic(old));
        }
        CHECK(progress == 1.0f);
    }
}

TEST_CASE("the menu row slides up and zooms, then slides down and unzooms") {
    SUBCASE("opening: 520 -> 440, the selected icon zooms in") {
        Clocked clock;
        OldMenu old;
        NewMenu fresh(clock);
        old.duration = 200;
        fresh.duration = 200;
        old.targety = fresh.targety = 440;
        old.active = fresh.active = true;
        runMenu(old, fresh, clock);
        CHECK(fresh.y == 440.0f);
        CHECK(fresh.scales[0] == MaxZoom);
        CHECK(fresh.xoff[0] == zoomOff(MaxZoom));
    }
    SUBCASE("closing: 440 -> 520, the selected icon zooms out") {
        Clocked clock;
        OldMenu old;
        NewMenu fresh(clock);
        old.y = old.oy = fresh.y = fresh.oy = 440;
        old.scales[0] = fresh.scales[0] = MaxZoom;
        old.xoff[0] = old.yoff[0] = fresh.xoff[0] = fresh.yoff[0] = zoomOff(MaxZoom);
        old.duration = 200;
        fresh.duration = 200;
        old.targety = fresh.targety = 520;
        old.active = fresh.active = false;
        runMenu(old, fresh, clock);
        CHECK(fresh.y == 520.0f);
        CHECK(fresh.scales[0] == 1.0f);
    }
    SUBCASE("on the third icon") {
        Clocked clock;
        OldMenu old;
        NewMenu fresh(clock);
        old.sel = fresh.sel = 2;
        old.duration = 200;
        fresh.duration = 200;
        old.targety = fresh.targety = 440;
        old.active = fresh.active = true;
        runMenu(old, fresh, clock);
        CHECK(fresh.scales[2] == MaxZoom);
    }
}

TEST_CASE("the selection moves one icon, left and right") {
    SUBCASE("left, from the second icon") {
        Clocked clock;
        OldMenu old;
        NewMenu fresh(clock);
        old.menuOn = fresh.menuOn = false;
        old.direction = fresh.direction = 0;
        old.duration = 100;
        fresh.duration = 100;
        old.sel = fresh.sel = 1;
        old.scales[1] = fresh.scales[1] = MaxZoom;
        old.xoff[1] = old.yoff[1] = fresh.xoff[1] = fresh.yoff[1] = zoomOff(MaxZoom);
        runMenu(old, fresh, clock);
        CHECK(fresh.sel == 0);
        CHECK(fresh.x == 640 - 118 / 2 + Gap);
        CHECK(fresh.scales[0] == MaxZoom);
        CHECK(fresh.scales[1] == 1.0f);
    }
    SUBCASE("right, from the first icon, and on to the third") {
        Clocked clock;
        OldMenu old;
        NewMenu fresh(clock);
        old.menuOn = fresh.menuOn = false;
        old.direction = fresh.direction = 1;
        old.duration = 100;
        fresh.duration = 100;
        old.scales[0] = fresh.scales[0] = MaxZoom;
        old.xoff[0] = old.yoff[0] = fresh.xoff[0] = fresh.yoff[0] = zoomOff(MaxZoom);
        runMenu(old, fresh, clock);
        CHECK(fresh.sel == 1);
        CHECK(fresh.x == 640 - 118 / 2 - Gap);
        // a second move starts from the settled state, later in time
        Clocked clock2;
        NewMenu next(clock2);
        static_cast<MenuState &>(next) = fresh;
        next.menuOn = false;
        next.direction = 1;
        next.duration = 100;
        OldMenu old2;
        static_cast<MenuState &>(old2) = old;
        old2.menuOn = false;
        old2.direction = 1;
        old2.duration = 100;
        runMenu(old2, next, clock2);
        CHECK(next.sel == 2);
        CHECK(next.x == 640 - 118 / 2 - 2 * Gap);
    }
}

TEST_CASE("a new transition replaces the one running, from where the old left off") {
    // the slide of the row restarted half-way: the old code rewrote animationStarted/targety/active and went on from
    // `oy` (the rest position) - the tween stops where it is and the new one starts over the same numbers
    Clocked clock;
    OldMenu old;
    NewMenu fresh(clock);
    old.duration = fresh.duration = 200;
    old.targety = fresh.targety = 440;
    old.active = fresh.active = true;
    old.animationStarted = Start;
    fresh.start();
    for (int t = Start; t <= Start + 80; t++) {
        old.update(t);
        clock.at(static_cast<unsigned int>(t));
        fresh.apply();
    }
    // back the other way at +80 ms
    old.targety = fresh.targety = 520;
    old.active = fresh.active = false;
    old.animationStarted = Start + 80;
    clock.now = Start + 80;
    fresh.start();
    for (int t = Start + 80; t <= Start + 80 + 240; t++) {
        old.update(t);
        clock.at(static_cast<unsigned int>(t));
        fresh.apply();
        expectSame(old, fresh);
    }
    CHECK_FALSE(fresh.moving_);
    CHECK_FALSE(clock.tweens.busy());
}

//*******************************
// the meta panel and the settings band
//*******************************
namespace {

// the old PsMeta::update / PsSettingsBack::update, verbatim but for the member names
struct OldSlide {
    int y = 285;
    int prevPos = 0;
    int nextPos = 0;
    long animEndTime = 0;
    long animStarted = 0;
    // `h` mode: the band (y = 632 - length)
    bool band = false;
    int h = 100;
    int prevLen = 0;
    int nextLen = 0;

    void startMeta(int time, int to) {
        animEndTime = time + 200;
        animStarted = time;
        nextPos = to;
        prevPos = y;
    }
    void startBand(int time, int to) {
        animEndTime = time + 100;
        animStarted = time;
        prevLen = h;
        nextLen = to;
    }
    void update(long time) {
        if (animEndTime != 0) {
            if (animStarted == 0) {
                animStarted = time;
                if (band)
                    prevLen = h;
            }
            if (animStarted != 0) {
                long currentAnim = time - animStarted;
                long totalAnimTime = animEndTime - animStarted;
                float position = easeOutCubic(currentAnim * 1.0f / totalAnimTime);
                if (band) {
                    int newSize = prevLen + ((nextLen - prevLen) * position);
                    y = 632 - newSize;
                    h = newSize;
                } else {
                    int newPos = prevPos + ((nextPos - prevPos) * position);
                    y = newPos;
                }
            }
            if (time >= animEndTime) {
                animStarted = 0;
                animEndTime = 0;
                if (band) {
                    y = 632 - nextLen;
                    h = nextLen;
                } else {
                    y = nextPos;
                }
            }
        }
    }
};

struct NewSlide {
    Clocked &clock;
    bool band;
    int y = 285;
    int h = 100;
    int prevPos = 0, nextPos = 0, prevLen = 0, nextLen = 0;
    float progress_ = 0;
    bool sliding_ = false;
    TweenOwner owner_;

    NewSlide(Clocked &c, bool isBand) : clock(c), band(isBand) {}

    void slideTo(int to) {
        owner_.cancel();
        if (band) {
            prevLen = h;
            nextLen = to;
        } else {
            prevPos = y;
            nextPos = to;
        }
        progress_ = 0;
        sliding_ = true;
        clock.tweens.start(
            Tween(progress_, 0.0f, 1.0f, band ? evomotion::SettingsBandMs : evomotion::MetaSlideMs).onEnd([this]() {
                sliding_ = false;
                if (band) {
                    y = 632 - nextLen;
                    h = nextLen;
                } else {
                    y = nextPos;
                }
            }),
            owner_);
    }
    void render() {
        if (!sliding_)
            return;
        if (band) {
            const int newSize = evomotion::slidInt(prevLen, nextLen, progress_);
            y = 632 - newSize;
            h = newSize;
        } else {
            y = evomotion::slidInt(prevPos, nextPos, progress_);
        }
    }
};

void runSlide(OldSlide &old, NewSlide &fresh, Clocked &clock, int from, int length) {
    for (int t = from; t <= from + length; t++) {
        old.update(t);
        clock.at(static_cast<unsigned int>(t));
        fresh.render();
        CHECK(old.y == fresh.y);
        CHECK(old.h == fresh.h);
        CHECK((old.animEndTime != 0) == fresh.sliding_);
        CHECK(clock.tweens.busy() == fresh.sliding_);
    }
}

} // namespace

TEST_CASE("the meta panel slides in 200 ms, and a slide started half-way goes on from where the panel is") {
    Clocked clock;
    OldSlide old;
    NewSlide fresh(clock, false);
    old.startMeta(Start, 215);
    fresh.slideTo(215);
    runSlide(old, fresh, clock, Start, 120); // up to 120 ms in
    CHECK(fresh.sliding_);
    // back down at +120 ms
    clock.now = Start + 120;
    old.startMeta(Start + 120, 285);
    fresh.slideTo(285);
    runSlide(old, fresh, clock, Start + 120, 260);
    CHECK(fresh.y == 285);
    CHECK_FALSE(fresh.sliding_);
    CHECK_FALSE(clock.tweens.busy());
}

TEST_CASE("the settings band changes length in 100 ms, and a retarget starts from the length it has") {
    Clocked clock;
    OldSlide old;
    old.band = true;
    old.y = 632 - 100;
    NewSlide fresh(clock, true);
    fresh.y = 632 - 100;
    old.startBand(Start, 280);
    fresh.slideTo(280);
    runSlide(old, fresh, clock, Start, 60);
    clock.now = Start + 60;
    old.startBand(Start + 60, 100);
    fresh.slideTo(100);
    runSlide(old, fresh, clock, Start + 60, 140);
    CHECK(fresh.h == 100);
    CHECK(fresh.y == 532);
    CHECK_FALSE(clock.tweens.busy());
}

TEST_CASE("the DebugDriver's busy follows the tweens and a dead control stops holding it") {
    Clocked clock;
    CHECK_FALSE(clock.tweens.busy());
    {
        NewSlide fresh(clock, false);
        fresh.slideTo(215);
        CHECK(clock.tweens.busy());
        clock.at(Start + 50);
        CHECK(clock.tweens.busy());
        // the control goes (the launcher closes) in the middle of its slide
    }
    clock.at(Start + 60);
    CHECK_FALSE(clock.tweens.busy());
    CHECK_FALSE(clock.tweens.animating());
}

TEST_CASE("a press during a transition: finishNonAmbient jumps every transition to its exact end") {
    Clocked clock;
    NewMenu menu(clock);
    NewSlide meta(clock, false);
    menu.duration = 200;
    menu.targety = 440;
    menu.active = true;
    menu.start();
    meta.slideTo(215);
    clock.at(Start + 30);
    clock.tweens.finishNonAmbient();
    CHECK_FALSE(menu.moving_);
    CHECK_FALSE(meta.sliding_);
    CHECK(menu.y == 440.0f);
    CHECK(menu.scales[0] == MaxZoom);
    CHECK(meta.y == 215);
    CHECK_FALSE(clock.tweens.busy());
}
