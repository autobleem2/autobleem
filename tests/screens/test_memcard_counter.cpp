//
// The Memory Cards list's counter (BUG-41, gui/menus/memcard_counter.h): "Card n/N" with cards, "0/0" without.
//
#include "doctest/doctest.h"

#include "menus/memcard_counter.h"

TEST_CASE("the counter names the card and how many there are") {
    CHECK(MemcardCounter::text("Card", 0, 12) == "Card 1/12");
    CHECK(MemcardCounter::text("Card", 11, 12) == "Card 12/12");
}

TEST_CASE("with no cards the counter reads 0/0") {
    CHECK(MemcardCounter::text("Card", 0, 0) == "0/0");
}
