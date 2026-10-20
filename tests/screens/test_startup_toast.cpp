//
// The start-up toast's hold (evoui/startup_toast.h, UIREV-54): a notification line set while the assets load stays
// for a full ten seconds from the FIRST FRAME that draws it, not from its show(): the load before that frame (the
// bubble needs a live Gui, so only the hold's arithmetic is tested here) used up most of a hold counted from show().
//
#include "doctest/doctest.h"

#include "startup_toast.h"

TEST_CASE("the start-up hold is ten seconds") {
    CHECK(StartupToast::HoldTicks == 10000);
    CHECK(StartupToast::HoldTicks > 2 * DefaultShowingTimeout);
}

TEST_CASE("a start-up hold counts from the first render, not from the show") {
    StartupToast::DeferredHold hold;
    hold.arm(StartupToast::HoldTicks); // at t = 1000, show()
    CHECK(hold.pending());
    // the load takes 7 s: the first frame is at t = 8000 and the toast must last to 18000, not to 11000
    CHECK(hold.start(8000) == 8000 + StartupToast::HoldTicks);
    CHECK_FALSE(hold.pending());
    CHECK(hold.start(8040) == 0); // later frames do not move it
}

TEST_CASE("a hold that was not armed, or was cancelled, never ends the bubble") {
    StartupToast::DeferredHold hold;
    CHECK(hold.start(500) == 0);
    hold.arm(StartupToast::HoldTicks);
    hold.cancel(); // a later show() with its own hold
    CHECK(hold.start(500) == 0);
}
