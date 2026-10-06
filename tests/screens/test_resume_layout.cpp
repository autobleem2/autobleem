//
// The resume-slot screen's layout and small rules (UIREV-37, evoui/controls/resume_layout.h): the four cards' places,
// the NEWEST slot, the picture's cut corners and the date format. The drawing (PsStateSelector::render) needs a live
// Gui and is the walk's job.
//
#include "doctest/doctest.h"

#include "resume_layout.h"

using namespace ResumeLayout;

TEST_CASE("the four cards sit at x 55, 353, 651, 949 and fit the 1280 canvas") {
    CHECK(cardBox(0).x == 55);
    CHECK(cardBox(1).x == 353);
    CHECK(cardBox(2).x == 651);
    CHECK(cardBox(3).x == 949);
    for (int i = 0; i < SlotCount; i++) {
        CHECK(cardBox(i).y == 212);
        CHECK(cardBox(i).w == 276);
        CHECK(cardBox(i).h == 300);
        CHECK(cardBox(i).x + cardBox(i).w <= 1280 - 55 + 1);
    }
    CHECK(BandY + BandH == 584);
}

TEST_CASE("the well and the picture sit inside the card, the picture inside the well") {
    const Box card = cardBox(2);
    const Box well = wellBox(card);
    const Box pic = pictureBox(card);
    CHECK(well.x == card.x + 14);
    CHECK(well.y == card.y + 14);
    CHECK(well.w == 248);
    CHECK(well.h == 186);
    CHECK(pic.x == card.x + 16);
    CHECK(pic.w == 244);
    CHECK(pic.h == 182);
    CHECK(pic.x >= well.x);
    CHECK(pic.y >= well.y);
    CHECK(pic.x + pic.w <= well.x + well.w);
    CHECK(pic.y + pic.h <= well.y + well.h);
    CHECK(well.y + well.h < card.y + SlotNameY); // the texts are below the well
}

TEST_CASE("the NEWEST chip's right edge is the card's, whatever the word's width") {
    const Box card = cardBox(0);
    for (int wordW : {40, 62, 90}) {
        const Box chip = chipBox(card, wordW);
        CHECK(chip.x + chip.w == card.x + 260);
        CHECK(chip.w == wordW + 14);
        CHECK(chip.h == 24);
        CHECK(chip.y == card.y + 222);
    }
}

TEST_CASE("NEWEST: the latest of two or more used slots") {
    const bool used[4] = {true, true, true, false};
    const time_t times[4] = {1000, 500, 2000, 0};
    CHECK(newestSlot(times, used) == 2);
}

TEST_CASE("NEWEST: none with one used slot, none with no slot, none when no time is known") {
    const time_t times[4] = {1000, 0, 0, 0};
    CHECK(newestSlot(times, {true, false, false, false}) == -1);
    CHECK(newestSlot(times, {false, false, false, false}) == -1);
    const time_t unknown[4] = {0, 0, 0, 0};
    CHECK(newestSlot(unknown, {true, true, false, false}) == -1);
}

TEST_CASE("NEWEST: an unused slot's time never counts, a tie goes to the lower slot") {
    const time_t times[4] = {100, 300, 300, 900};
    CHECK(newestSlot(times, {true, true, true, false}) == 1);
}

TEST_CASE("the picture's cut corners: 6 px at the top right and the bottom left, a diagonal") {
    CHECK(rowInset(0, 182).right == 6);
    CHECK(rowInset(5, 182).right == 1);
    CHECK(rowInset(6, 182).right == 0);
    CHECK(rowInset(0, 182).left == 0);
    CHECK(rowInset(181, 182).left == 6);
    CHECK(rowInset(176, 182).left == 1);
    CHECK(rowInset(175, 182).left == 0);
    CHECK(rowInset(90, 182).left == 0);
    CHECK(rowInset(90, 182).right == 0);
    CHECK(rowInset(0, 182, 0).right == 0); // no cut, no inset
}

TEST_CASE("the date's format is the user's when there is one") {
    CHECK(dateFormat("") == "%d.%m.%Y   %H:%M");
    CHECK(dateFormat("%F %I:%M %p") == "%F %I:%M %p");
}

TEST_CASE("the 4:3 design: four cards fit the 640 x 480 canvas, above the hint bar, the picture keeps its 4:3 shape") {
    for (int i = 0; i < SlotCount; i++) {
        const Box card = cardBox(i, true);
        const Box well = wellBox(card, true);
        const Box pic = pictureBox(card, true);
        CHECK(card.x >= 0);
        CHECK(card.x + card.w <= 640);
        CHECK(card.y + card.h <= Narrow::BandY + Narrow::BandH);
        CHECK(Narrow::BandY + Narrow::BandH < 418); // the hint bar starts at 418
        CHECK(pic.w * 3 == pic.h * 4);
        CHECK(pic.x >= well.x);
        CHECK(pic.y >= well.y);
        CHECK(pic.x + pic.w <= well.x + well.w);
        CHECK(pic.y + pic.h <= well.y + well.h);
        CHECK(well.x + well.w <= card.x + card.w);
        if (i > 0)
            CHECK(card.x == cardBox(i - 1, true).x + card.w + Narrow::CardGap);
    }
}

TEST_CASE("the 4:3 NEWEST chip sits on the card's right inside it, beside the slot name") {
    const Box card = cardBox(1, true);
    const Box chip = chipBox(card, 46, true);
    CHECK(chip.x + chip.w == card.x + Narrow::ChipRight);
    CHECK(chip.x >= card.x + Narrow::TextInset + 50); // clear of "Slot 2"
    CHECK(chip.y + chip.h <= card.y + Narrow::DateY);
}
