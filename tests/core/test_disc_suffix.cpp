//
// DiscSuffix: the disc marker at the end of a folder name.
//
#include "doctest/doctest.h"

#include <ableem/engine/disc_suffix.h>

using ableem::DiscSuffix;

TEST_CASE("the bracketed forms: Disc, Disk, CD, with and without a space, any case") {
    DiscSuffix d = DiscSuffix::parse("Final Fantasy VII (USA) (Disc 1)");
    CHECK(d.matched());
    CHECK(d.disc == 1);
    CHECK(d.base == "Final Fantasy VII (USA)");

    CHECK(DiscSuffix::parse("Game (Disk 2)").disc == 2);
    CHECK(DiscSuffix::parse("Game (CD 3)").disc == 3);
    CHECK(DiscSuffix::parse("Game (CD3)").disc == 3);
    CHECK(DiscSuffix::parse("Game (disc 12)").disc == 12);
    CHECK(DiscSuffix::parse("Game ( Disc 1 )").disc == 1);
    CHECK(DiscSuffix::parse("Game (Disc 1)  ").base == "Game");   // trailing whitespace tolerated
}

TEST_CASE("the dash forms") {
    DiscSuffix d = DiscSuffix::parse("Metal Gear Solid - Disc 2");
    CHECK(d.disc == 2);
    CHECK(d.base == "Metal Gear Solid");
    CHECK(DiscSuffix::parse("Game - Disk 1").disc == 1);
    CHECK(DiscSuffix::parse("Game - CD 4").disc == 4);
    CHECK(DiscSuffix::parse("Game-Disc 1").base == "Game");
}

TEST_CASE("names without a marker, or with one that is not a disc, keep their name and say disc 0") {
    const char *names[] = {"Crash Bandicoot", "Crash Bandicoot (USA)", "Game (Rev 1)", "Game (Disc)", "Game (Disc A)",
                           "Game - The Discovery", "Discworld", "Game (Disc 1) (Rev 1)", "CD Game", ""};
    for (const char *n : names) {
        DiscSuffix d = DiscSuffix::parse(n);
        CHECK_FALSE(d.matched());
        CHECK(d.disc == 0);
        CHECK(d.base == n);
    }
}
