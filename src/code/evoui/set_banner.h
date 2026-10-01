//
// The set banner's hold time (BUG-30): the banner ("Showing: <set> (N games)") after a set switch always shows. It
// holds for Options' "Notification timeout" when that is on (> 0); with the timeout Off it uses the default time
// instead - Off hides the OTHER informational notifications only. Pure, so tests/screens/test_set_banner holds it.
//
#pragma once

namespace SetBanner {

// infoTicks: the notification timeout in ticks (0 or less = Off); defaultTicks: the time used when it is Off
inline long holdTicks(long infoTicks, long defaultTicks) {
    return infoTicks > 0 ? infoTicks : defaultTicks;
}

} // namespace SetBanner
