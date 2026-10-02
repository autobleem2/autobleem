//
// ra_gates.h (evoui/screens/, pure): which emulator a PS1 game starts in with and without the RetroArch program, and
// the Lightgun set's availability.
//
#include "doctest/doctest.h"

#include "ra_gates.h"

TEST_CASE("PS1 start: with RetroArch, any of the flags sends the game to RetroArch") {
    CHECK_FALSE(startPs1InRetroArch(true, false, false, false));
    CHECK(startPs1InRetroArch(true, true, false, false)); // the Lightgun set
    CHECK(startPs1InRetroArch(true, false, true, false)); // Play using RA
    CHECK(startPs1InRetroArch(true, false, false, true)); // Play all PSX games with RA
}

TEST_CASE("PS1 start: without RetroArch every flag combination starts in PCSX") {
    for (int bits = 0; bits < 8; bits++)
        CHECK_FALSE(startPs1InRetroArch(false, bits & 1, bits & 2, bits & 4));
}

TEST_CASE("the Lightgun set exists only with RetroArch, a remembered one falls back to PlayStation") {
    CHECK(lightgunSetAvailable(true));
    CHECK_FALSE(lightgunSetAvailable(false));
    CHECK(setOrFallback(GameSet::Lightgun, true) == GameSet::Lightgun);
    CHECK(setOrFallback(GameSet::Lightgun, false) == GameSet::PS1);
    for (GameSet s : {GameSet::PS1, GameSet::RetroArch, GameSet::Apps}) {
        CHECK(setOrFallback(s, true) == s);
        CHECK(setOrFallback(s, false) == s);
    }
}
