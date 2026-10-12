//
// cd_player_logic.h (evoui/screens/, pure): the CD player screen's times, grid cursor, bar segments and repeat cycle.
//
#include "doctest/doctest.h"

#include "cd_player_logic.h"

using namespace cdplayer;

TEST_CASE("clockText: minutes and seconds of CD frames, rounded down") {
    CHECK(clockText(0) == "0:00");
    CHECK(clockText(74) == "0:00");
    CHECK(clockText(75) == "0:01");
    CHECK(clockText(75 * 241) == "4:01");
    CHECK(clockText(75 * (75 * 60 + 10)) == "75:10");
    CHECK(twoDigits(4) == "04");
    CHECK(twoDigits(12) == "12");
}

TEST_CASE("moveFocus: left/right wrap, up/down stop at the rows") {
    CHECK(moveFocus(0, -1, 0, 14) == 13);
    CHECK(moveFocus(13, 1, 0, 14) == 0);
    CHECK(moveFocus(3, 1, 0, 14) == 4);
    CHECK(moveFocus(3, 0, 1, 14) == 13);   // the short second row: its end
    CHECK(moveFocus(2, 0, 1, 14) == 12);
    CHECK(moveFocus(12, 0, 1, 14) == 12);  // already on the last row
    CHECK(moveFocus(12, 0, -1, 14) == 2);
    CHECK(moveFocus(2, 0, -1, 14) == 2);   // the first row
    CHECK(moveFocus(0, 1, 0, 1) == 0);
    CHECK(moveFocus(0, 0, 1, 0) == 0);
}

TEST_CASE("firstRowFor keeps the focused row in view") {
    CHECK(firstRowFor(0, 0, 2, 14) == 0);
    CHECK(firstRowFor(12, 0, 1, 14) == 1);
    CHECK(firstRowFor(5, 1, 1, 14) == 0);
    CHECK(firstRowFor(25, 0, 2, 99) == 1);
    CHECK(firstRowFor(98, 0, 2, 99) == 8);
}

TEST_CASE("litSegments: nothing only for silence, a whole bar at 1") {
    CHECK(litSegments(0.0f, 16) == 0);
    CHECK(litSegments(0.001f, 16) == 1);
    CHECK(litSegments(0.5f, 16) == 8);
    CHECK(litSegments(1.0f, 16) == 16);
    CHECK(litSegments(7.0f, 16) == 16);
}

TEST_CASE("the repeat cycle and the seek step") {
    CHECK(nextRepeat(RepeatMode::Off) == RepeatMode::Disc);
    CHECK(nextRepeat(RepeatMode::Disc) == RepeatMode::Track);
    CHECK(nextRepeat(RepeatMode::Track) == RepeatMode::Off);
    CHECK(seekTarget(1000, 1) == 1375);
    CHECK(seekTarget(1000, -1) == 625);
    CHECK(seekTarget(100, -1) == 0);
}
