//
// The disc view's pure rules (evoui/screens/disc_view_logic.h): the chip for each drive state, the blink, when the
// view opens and closes by itself, the Circle footer rule, the crossfade, the audio CD's total time.
//
#include "doctest/doctest.h"

#include "disc_view_logic.h"

#include <memory>
#include <string>

using namespace discview;

TEST_CASE("the chip: nothing without a drive, dim when empty, blinking while reading") {
    CHECK(chipFor(Drive::NoDrive) == Chip{ChipLook::Hidden, ChipText::None});
    CHECK(chipFor(Drive::Empty) == Chip{ChipLook::Dim, ChipText::None});
    CHECK(chipFor(Drive::Reading) == Chip{ChipLook::Blink, ChipText::Reading});
}

TEST_CASE("the chip: lit with the title when a disc is ready, dim for another disc") {
    CHECK(chipFor(Drive::ReadyGame) == Chip{ChipLook::Lit, ChipText::Disc});
    CHECK(chipFor(Drive::ReadyAudio) == Chip{ChipLook::Lit, ChipText::Disc});
    CHECK(chipFor(Drive::NotPs1) == Chip{ChipLook::Dim, ChipText::NotPs1});
    CHECK(chipFor(Drive::Unreadable) == Chip{ChipLook::Dim, ChipText::Unreadable});
}

TEST_CASE("the chip's alpha: hidden 0, dim, lit 255, the blink stays between dim and full") {
    CHECK(chipAlpha(ChipLook::Hidden, 100) == 0);
    CHECK(chipAlpha(ChipLook::Dim, 100) == DimAlpha);
    CHECK(chipAlpha(ChipLook::Lit, 100) == 255);
    for (long ms = -2000; ms < 3000; ms += 37) {
        const int a = chipAlpha(ChipLook::Blink, ms);
        CHECK(a >= DimAlpha);
        CHECK(a <= 255);
    }
    CHECK(blinkAlpha(0) == DimAlpha);
    CHECK(blinkAlpha(BlinkPeriodMs / 2) == 255);
    CHECK(blinkAlpha(BlinkPeriodMs) == DimAlpha);
}

TEST_CASE("the view opens when a disc becomes ready and not again after Circle") {
    CHECK(opensByItself(Drive::ReadyGame, 3, 2, false));
    CHECK(opensByItself(Drive::ReadyAudio, 3, 2, false));
    CHECK_FALSE(opensByItself(Drive::ReadyGame, 3, 3, false)); // Circle: nothing changed since
    CHECK_FALSE(opensByItself(Drive::ReadyGame, 3, 2, true));  // already open
    CHECK_FALSE(opensByItself(Drive::Reading, 3, 2, false));
    CHECK_FALSE(opensByItself(Drive::NotPs1, 3, 2, false));
}

TEST_CASE("the view closes when the disc goes") {
    CHECK(closesByItself(Drive::Empty, true));
    CHECK(closesByItself(Drive::NoDrive, true));
    CHECK(closesByItself(Drive::Reading, true));
    CHECK_FALSE(closesByItself(Drive::ReadyGame, true));
    CHECK_FALSE(closesByItself(Drive::Empty, false));
}

TEST_CASE("Circle Disc is in the footer only with a ready disc and the carousel shown") {
    CHECK(circleShowsDisc(Drive::ReadyGame, false));
    CHECK(circleShowsDisc(Drive::ReadyAudio, false));
    CHECK_FALSE(circleShowsDisc(Drive::ReadyGame, true));
    CHECK_FALSE(circleShowsDisc(Drive::Empty, false));
    CHECK_FALSE(circleShowsDisc(Drive::NotPs1, false));
    CHECK_FALSE(circleShowsDisc(Drive::Reading, false));
}

TEST_CASE("the crossfade: covers up to the middle, swaps once, clears at the end") {
    CHECK(fadeAlpha(0) == 0);
    CHECK(fadeAlpha(FadeHalfMs) == 255);
    CHECK(fadeAlpha(2 * FadeHalfMs) == 0);
    CHECK(fadeAlpha(FadeHalfMs / 2) > 0);
    CHECK(fadeAlpha(FadeHalfMs / 2) < 255);
    CHECK_FALSE(fadeSwapDue(FadeHalfMs - 1, false));
    CHECK(fadeSwapDue(FadeHalfMs, false));
    CHECK_FALSE(fadeSwapDue(FadeHalfMs, true));
    CHECK_FALSE(fadeDone(2 * FadeHalfMs - 1));
    CHECK(fadeDone(2 * FadeHalfMs));
}

TEST_CASE("the total time of an audio CD") {
    CHECK(totalTimeText(0) == "0:00");
    CHECK(totalTimeText(75 * 61) == "1:01");
    CHECK(totalTimeText(75 * (47 * 60 + 12) + 74) == "47:12");
    CHECK(totalTimeText(75 * 3600) == "60:00");
}

TEST_CASE("the resume screen after a run is about the disc that played, else the carousel's game") {
    auto disc = std::make_shared<std::string>("disc");
    auto carousel = std::make_shared<std::string>("carousel");
    const std::shared_ptr<std::string> none;
    CHECK(resumeGameFor(disc, carousel).get() == disc.get());
    CHECK(resumeGameFor(none, carousel).get() == carousel.get());
    CHECK(resumeGameFor(disc, none).get() == disc.get());
    CHECK(resumeGameFor(none, none).get() == nullptr);
}
