//
// CarouselMotion: the timing of the carousel's moves - a scroll step (eased for a tap, linear for a held stick, the
// held stick's steps running into each other with no pause) and the selected cover's raise and lowering - on ab_gui's
// tweens (autobleem-core docs/ab-gui-plan.md, G5o5). Pure apart from the tweens, so tests/screens/test_carousel_motion
// holds it to the hand-written timer it replaced, position for position; Carousel (carousel.cpp) is its one user.
//
// A move is one tween run for all the covers that set off together: it writes the fraction of the way (0 -> 1) into a
// float, and each cover goes from its `current` point to its `destination` by that fraction. The old timer, per cover,
// was: delta = (now - start) / duration, easeOutCubic for an eased move while delta < 1; the point lerped by delta;
// once delta > 1 the cover put on its destination and its timer dropped. The run below gives the same fraction at
// every time - ease::outCubic is timing.h's easeOutCubic formula for formula, tweenLerp(0, 1, f) is f exactly - and
// holds 1 for one more millisecond, so, the ticks being whole milliseconds, the covers are drawn at the curve's end
// at exactly the duration (delta == 1) and put on their destination from the first tick past it (delta > 1), as
// before. The run is not ambient: the DebugDriver is busy while it runs, so `wait_ready` after a move waits for the
// carousel to rest.
//
#pragma once

#include <ab_gui/tween.h>

namespace CarouselMotion {

// when a scroll step begins: `now` - or, for a held stick's step (not eased) that follows the last one when that one
// ended no more than a step ago, exactly where the last one ended, so the row keeps its speed instead of losing the
// part of a frame between the end of one step and the start of the next. chainEnd is then the end of this step for a
// held one, 0 after a tap.
inline long stepStart(long now, long &chainEnd, int speed, bool eased) {
    long start = now;
    if (!eased && chainEnd != 0 && now >= chainEnd && now - chainEnd < speed)
        start = chainEnd;
    chainEnd = eased ? 0 : start + speed;
    return start;
}

// one move's run: `fraction` from 0 to 1 over durationMs - easeOutCubic for a tap and the main cover, linear for a
// held stick - then held at 1 for one millisecond (see above)
inline abgui::Timeline move(float &fraction, unsigned int durationMs, bool eased) {
    abgui::Timeline run = abgui::Timeline::sequence();
    run.add(abgui::Tween(fraction, 0.0f, 1.0f, durationMs).ease(eased ? &abgui::ease::outCubic : &abgui::ease::linear));
    run.add(abgui::Tween(fraction, 1.0f, 1.0f, 1).ease(&abgui::ease::linear));
    return run;
}

// a point `delta` of the way from `from` to `to`: every field as the old timer computed it (an int field truncated)
template <class Point> void lerp(Point &actual, const Point &from, const Point &to, float delta) {
    actual.x = from.x + (to.x - from.x) * delta;
    actual.y = from.y + (to.y - from.y) * delta;
    actual.scale = from.scale + (to.scale - from.scale) * delta;
    actual.shade = from.shade + (to.shade - from.shade) * delta;
    actual.angle = from.angle + (to.angle - from.angle) * delta;
}

// a cover's move: the Moves slot it runs in and its run there (a slot is taken again once its run is over, so the id
// tells whether the move is still the cover's). Set = the cover is moving (the old timer's start != 0) until the first
// frame that finds the run over
struct MoveRef {
    int slot = -1;
    abgui::TweenId id = abgui::NoTween;
    bool set() const { return slot >= 0; }
};

// the moves under way - a scroll step, the main cover's raise or lowering, a step whose covers a jump (a new
// selection mid-scroll) left behind: a few at once, each a float its run writes, in a fixed pool, so no frame
// allocates (a run per move is made when the move starts)
class Moves {
public:
    static const int Slots = 4;

    // starts a move at `startedAt` on the tweens' clock (not after now) for `owner`: a slot whose run is over, else the
    // oldest one's (its run cancelled: its covers go to their destination on the next frame)
    MoveRef start(abgui::Tweens &tweens, const abgui::TweenOwner &owner, unsigned int startedAt,
                  unsigned int durationMs, bool eased) {
        int pick = -1;
        for (int i = 0; i < Slots && pick < 0; ++i)
            if (!tweens.running(slots_[i].id))
                pick = i;
        if (pick < 0) {
            pick = 0;
            for (int i = 1; i < Slots; ++i)
                if (slots_[i].order < slots_[pick].order)
                    pick = i;
            tweens.cancel(slots_[pick].id);
        }
        Slot &slot = slots_[pick];
        slot.fraction = 0.0f; // a tween writes only from its first update
        slot.order = ++started_;
        slot.id = tweens.startAt(startedAt, CarouselMotion::move(slot.fraction, durationMs, eased), owner);
        MoveRef ref;
        ref.slot = pick;
        ref.id = slot.id;
        return ref;
    }

    // the move is still under way (after the tweens' update for this frame): its fraction is fraction(ref)
    bool running(const abgui::Tweens &tweens, const MoveRef &ref) const {
        return ref.set() && slots_[ref.slot].id == ref.id && tweens.running(ref.id);
    }
    float fraction(const MoveRef &ref) const { return slots_[ref.slot].fraction; }
    // stops the move now (the slot's run, if it is still this one's)
    void cancel(abgui::Tweens &tweens, const MoveRef &ref) {
        if (ref.set() && slots_[ref.slot].id == ref.id)
            tweens.cancel(ref.id);
    }

private:
    struct Slot {
        float fraction = 0.0f;
        abgui::TweenId id = abgui::NoTween;
        unsigned long order = 0; // when it was started, counted
    };
    Slot slots_[Slots];
    unsigned long started_ = 0;
};

// one cover's frame, after the tweens' update (the old timer's body): part way along its move, or - the move over -
// on its destination with the move dropped. `Item` has the points `current`, `destination`, `actual` and a MoveRef
// `move`; the caller does this for the visible covers only, as before
template <class Item> void advance(Item &item, const Moves &moves, const abgui::Tweens &tweens) {
    if (!item.move.set())
        return;
    if (moves.running(tweens, item.move)) {
        CarouselMotion::lerp(item.actual, item.current, item.destination, moves.fraction(item.move));
        return;
    }
    item.actual = item.destination;
    item.current = item.destination;
    item.move = MoveRef();
}

} // namespace CarouselMotion
