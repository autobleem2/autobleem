//
// StartupToast: how long a notification set while the launcher loads its assets (the gamepad notice, the theme
// fallback toast) is held. A bubble counts its hold from the show() call, and the first frame comes only after the
// rest of the loading (covers, sounds, the first set) - a hold of a few seconds could be used up before anyone sees it.
//
#pragma once

#include "core/model/timing.h"

namespace StartupToast {

constexpr long HoldTicks = 10 * TicksPerSecond;

} // namespace StartupToast
