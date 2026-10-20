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

TEST_CASE("the Options toast is held 4 seconds, counted from the end of the frame that first draws it") {
    CHECK(StartupToast::OptionsHoldTicks == 4000);
    StartupToast::DeferredHold hold;
    hold.arm(StartupToast::OptionsHoldTicks); // show() at t = 1000
    // the theme's first frame takes 3 s on the VM and ends at t = 4000: the toast must last to 8000, not to 5000
    // (a hold counted from show() left 1 s of 4)
    CHECK(hold.start(4000) == 4000 + StartupToast::OptionsHoldTicks);
}

TEST_CASE("a hold that was not armed, or was cancelled, never ends the bubble") {
    StartupToast::DeferredHold hold;
    CHECK(hold.start(500) == 0);
    hold.arm(StartupToast::HoldTicks);
    hold.cancel(); // a later show() with its own hold
    CHECK(hold.start(500) == 0);
}

// HoldTimer: the bubble's timing on one clock, here a fake one (the bubble gives it Gui::platform().ticks())
TEST_CASE("HoldTimer: a plain show holds the full time from the platform clock") {
    StartupToast::HoldTimer t;
    // the launcher's own time lags the platform's by 3 s (a slow pass) - it is never handed to the timer
    const long platformNow = 20000;
    t.show(platformNow, 4000);
    CHECK(t.hideAt() == 24000);
    CHECK_FALSE(t.expired(23999));
    CHECK(t.expired(24000));
    // a frame drawn with the stale caller time (17000) changes nothing: only the platform's clock is asked
    CHECK_FALSE(t.frameDone(21000));
    CHECK(t.hideAt() == 24000);
}

TEST_CASE("HoldTimer: a hold of 0 never ends") {
    StartupToast::HoldTimer t;
    t.show(5000, 0);
    CHECK(t.hideAt() == 0);
    CHECK_FALSE(t.expired(1000000));
}

TEST_CASE("HoldTimer: a first-render hold starts at the end of the first frame") {
    StartupToast::HoldTimer t;
    t.showFromFirstRender(1735, StartupToast::HoldTicks);
    CHECK(t.hideAt() == 0);          // nothing runs before the frame
    CHECK_FALSE(t.expired(60000));
    CHECK(t.frameDone(4735));        // the first frame took 3 s
    CHECK(t.holdStartAt() == 4735);
    CHECK(t.hideAt() == 14735);      // a full ten seconds from there, not 11735
    CHECK_FALSE(t.expired(11735));
    CHECK_FALSE(t.frameDone(4800));  // later frames do not move it
    CHECK(t.hideAt() == 14735);
    CHECK(t.expired(14735));
}

TEST_CASE("HoldTimer: a later plain show cancels a pending first-render hold") {
    StartupToast::HoldTimer t;
    t.showFromFirstRender(1000, StartupToast::HoldTicks);
    t.show(2000, 4000);
    CHECK_FALSE(t.frameDone(9000));
    CHECK(t.hideAt() == 6000);
}
