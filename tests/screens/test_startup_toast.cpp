//
// The start-up toast's hold (evoui/startup_toast.h, UIREV-54): a notification line set while the assets load stays
// for a full ten seconds from its show(), not the 2 s of an ordinary message, which the load before the first frame
// (the bubble needs a live Gui, so only the hold is tested here) could use up.
//
#include "doctest/doctest.h"

#include "startup_toast.h"

TEST_CASE("a notification set before the first render is held for a full 10 seconds") {
    CHECK(StartupToast::HoldTicks == 10000);
    CHECK(StartupToast::HoldTicks == 5 * DefaultShowingTimeout);
    CHECK(StartupToast::HoldTicks > 2 * DefaultShowingTimeout); // what the theme toast used to get
}
