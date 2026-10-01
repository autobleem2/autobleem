//
// The game details section's layout (UIREV-35, evoui/controls/evoui_meta_layout.h): which rows each kind of game
// shows and where the right-aligned badges sit. The drawing (PsMeta::render) needs a live Gui and is the walk's job.
//
#include "doctest/doctest.h"

#include "evoui_meta_layout.h"

using namespace MetaLayout;

TEST_CASE("a PS1 game shows publisher, serial and last played") {
    const auto rows = facts(Kind::Ps1, "Made-up Studio", "1997", "SLUS-00000", "USA", "yesterday", "", true);
    REQUIRE(rows.size() == 3);
    CHECK(rows[0].label == "PUBLISHER");
    CHECK(rows[0].value == "Made-up Studio, 1997");
    CHECK(rows[1].label == "SERIAL");
    CHECK(rows[1].value == "SLUS-00000  \xC2\xB7  USA");
    CHECK(rows[2].label == "LAST PLAYED");
    CHECK(rows[2].value == "yesterday");
}

TEST_CASE("a row with nothing to show is left out and the rest move up") {
    auto rows = facts(Kind::Ps1, "Studio", "0", "", "", "", "", true);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].value == "Studio");
    rows = facts(Kind::Ps1, "Studio", "1999", "SLES-1", "", "yesterday", "", false);
    REQUIRE(rows.size() == 2);
    CHECK(rows[1].label == "SERIAL");
    CHECK(rows[1].value == "SLES-1");
    CHECK(facts(Kind::Ps1, "", "", "", "", "", "", true).empty());
}

TEST_CASE("a RetroArch game shows its core only when the publisher is known") {
    auto rows = facts(Kind::RetroArch, "Studio", "1994", "", "", "", "snes9x", true);
    REQUIRE(rows.size() == 2);
    CHECK(rows[1].label == "CORE");
    CHECK(rows[1].value == "snes9x");
    // no database publisher: the core is the only row, labelled CORE, never PUBLISHER
    rows = facts(Kind::RetroArch, "", "", "", "", "", "snes9x", true);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].label == "CORE");
    CHECK(rows[0].value == "snes9x");
}

TEST_CASE("an App shows its text as an unlabelled, wrapped description") {
    auto rows = facts(Kind::App, "Someone", "2020", "x", "y", "today", "core", true);
    REQUIRE(rows.size() == 1);
    CHECK(rows[0].label.empty());
    CHECK(rows[0].wrapped);
    CHECK(rows[0].value == "Someone, 2020");
    CHECK(facts(Kind::App, "", "0", "", "", "", "", true).empty());
    // only the description wraps
    rows = facts(Kind::RetroArch, "Studio", "1994", "", "", "", "snes9x", true);
    CHECK_FALSE(rows[0].wrapped);
}

TEST_CASE("wrapped lines past the cap are joined onto the last one") {
    const std::vector<std::string> lines = {"a b", "c d", "e f", "g h"};
    CHECK(capLines(lines, 4) == lines);
    CHECK(capLines(lines, 9) == lines);
    const auto cut = capLines(lines, 2);
    REQUIRE(cut.size() == 2);
    CHECK(cut[0] == "a b");
    CHECK(cut[1] == "c d e f g h");
    CHECK(capLines({}, 2).empty());
}

TEST_CASE("the description's lines fit under the grid's top, inside the section") {
    CHECK(DescriptionLines == 4);
    CHECK(GridY + DescriptionLines * RowPitch <= Height);
}

TEST_CASE("the badges are right-aligned, 38 apart, the last ending at x + 470") {
    for (int count = 1; count <= 6; count++) {
        CHECK(badgeX(count, count - 1) + IconSize == 470);
        for (int i = 1; i < count; i++)
            CHECK(badgeX(count, i) - badgeX(count, i - 1) == 38);
    }
    CHECK(badgeX(5, 0) == 288);
}

TEST_CASE("the grid fits the section's height") {
    CHECK(GridY + (MaxFacts - 1) * RowPitch + 20 <= IconRowY);
    CHECK(IconRowY + IconSize == Height);
}
