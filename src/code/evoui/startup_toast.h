//
// StartupToast: how long a notification set while the launcher loads its assets (the gamepad notice, the theme
// fallback toast) is held. The hold must count from the first frame that draws it, not from the show() call: the
// first frame comes only after the rest of the loading (covers, sounds, the first set), which on a slow start used up
// most of a hold counted from show() before anyone saw the toast (UIREV-54).
//
#pragma once

#include "core/model/timing.h"

namespace StartupToast {

constexpr long HoldTicks = 10 * TicksPerSecond;

// The Options theme-fallback toast (4 s): set right after a slow theme load, and its own first frame is slow too -
// the same hold-from-the-first-frame rule applies (GuiOptions::loadFor).
constexpr long OptionsHoldTicks = 2 * DefaultShowingTimeout;

// A hold that starts at the first render after it was asked for. arm() on the show(), start() on every render:
// it answers the time the hold ends the first time (0 = nothing pending / never ends) and nothing after that.
class DeferredHold {
public:
    void arm(long holdTicks) { pending_ = holdTicks; }
    void cancel() { pending_ = 0; }
    bool pending() const { return pending_ > 0; }
    // the timestamp the hold ends at when armed (counted from `now`, the frame being drawn), else 0
    long start(long now) {
        const long ends = pending_ > 0 ? now + pending_ : 0;
        pending_ = 0;
        return ends;
    }

private:
    long pending_ = 0;
};

// The timing of one toast, on ONE clock: every time handed in is the platform's (SDL ticks, ms), never the caller's
// own per-pass time (the launcher sets that once a pass, so it lags by a slow pass and is 0 in the first frame).
// NotificationBubble feeds it Gui::platform().ticks(); the tests feed a fake clock.
class HoldTimer {
public:
    // a plain show() at `platformNow`: the hold counts from now (0 = until hide)
    void show(long platformNow, long holdMs) {
        deferred_.cancel();
        shownAt_ = holdStartAt_ = platformNow;
        holdMs_ = holdMs;
        hideAt_ = holdMs > 0 ? platformNow + holdMs : 0;
    }
    // a show whose hold counts from the end of the first frame that draws it (frameDone)
    void showFromFirstRender(long platformNow, long holdMs) {
        show(platformNow, 0);
        holdMs_ = holdMs;
        deferred_.arm(holdMs);
    }
    // the first frame that draws the toast is done at `platformNow`: starts a deferred hold; true when it did
    bool frameDone(long platformNow) {
        if (!deferred_.pending())
            return false;
        holdStartAt_ = platformNow;
        hideAt_ = deferred_.start(platformNow);
        return true;
    }
    // the hold is over at `platformNow`
    bool expired(long platformNow) const { return hideAt_ != 0 && platformNow >= hideAt_; }

    long shownAt() const { return shownAt_; }
    long holdStartAt() const { return holdStartAt_; }
    long hideAt() const { return hideAt_; }
    long holdMs() const { return holdMs_; }

private:
    DeferredHold deferred_;
    long shownAt_ = 0, holdStartAt_ = 0, hideAt_ = 0, holdMs_ = 0;
};

} // namespace StartupToast
