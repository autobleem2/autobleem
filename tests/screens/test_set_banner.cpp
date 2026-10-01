//
// The set banner's hold time (BUG-30, evoui/set_banner.h): the notification timeout when it is on, the default time
// when it is Off. The drawing (GuiLauncher::showSetName) needs a live Gui and is the walk's job.
//
#include "doctest/doctest.h"

#include "set_banner.h"

TEST_CASE("the set banner holds for the notification timeout when it is on") {
    CHECK(SetBanner::holdTicks(5000, 2000) == 5000);
    CHECK(SetBanner::holdTicks(1000, 2000) == 1000);
}

TEST_CASE("with the notification timeout Off the set banner still shows, for the default time") {
    CHECK(SetBanner::holdTicks(0, 2000) == 2000);
    CHECK(SetBanner::holdTicks(-1000, 2000) == 2000);
}
