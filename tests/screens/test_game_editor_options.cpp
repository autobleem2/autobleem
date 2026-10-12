//
// game_editor_options.h (gui/menus/, pure): which of the game editor's rows are greyed for a disc in the drive.
//
#include "doctest/doctest.h"

#include "game_editor_options.h"

TEST_CASE("a disc in the drive: Favorite, Lightgun game, Play using RA and Lock data are greyed") {
    CHECK(greyedForDisc(true, OPT_FAVORITE));
    CHECK(greyedForDisc(true, OPT_LIGHTGUN));
    CHECK(greyedForDisc(true, OPT_PLAY_USING_RA));
    CHECK(greyedForDisc(true, OPT_LOCK));
}

TEST_CASE("a disc in the drive: Resume and the picture settings stay changeable") {
    for (int opt = OPT_FIRST; opt <= OPT_RESUME; opt++) {
        if (opt == OPT_FAVORITE || opt == OPT_LIGHTGUN || opt == OPT_PLAY_USING_RA || opt == OPT_LOCK)
            continue;
        CHECK_FALSE(greyedForDisc(true, opt));
    }
    CHECK_FALSE(greyedForDisc(true, OPT_RESUME));
}

TEST_CASE("a game that is not a disc in the drive has no greyed row") {
    for (int opt = OPT_FIRST; opt <= OPT_RESUME; opt++)
        CHECK_FALSE(greyedForDisc(false, opt));
}
