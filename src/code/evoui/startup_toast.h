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

} // namespace StartupToast
