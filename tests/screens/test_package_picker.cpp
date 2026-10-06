//
// package_picker_logic.h (evoui/screens/, pure): how a start with an engine goes (autobleem-main docs/packages.md
// 5.4), the picker's rows and where its cursor starts (6), and which installed Apps run a package (7).
//
#include "doctest/doctest.h"

#include "package_picker_logic.h"

using namespace packagepicker;
using std::string;
using std::vector;

namespace {
// 10 px a character (a UTF-8 character counts once): the widths below are characters x 10
int tenPerChar(const string &text) {
    int chars = 0;
    for (unsigned char c : text)
        if ((c & 0xC0) != 0x80)
            chars++;
    return chars * 10;
}

PackageEntry entry(const string &package, const string &game, const string &variant = "") {
    PackageEntry e;
    e.packageId = package;
    e.packageTitle = package;
    e.game.id = game;
    e.game.title = game;
    e.game.variant = variant;
    return e;
}
} // namespace

TEST_CASE("0 entries show the message, 1 starts at once, 2 or more open the picker") {
    CHECK(decide(0) == Start::NoData);
    CHECK(decide(1) == Start::Single);
    CHECK(decide(2) == Start::Pick);
    CHECK(decide(40) == Start::Pick);
}

TEST_CASE("the cursor starts on the last choice, else on the first row") {
    vector<PackageEntry> list = {entry("a", "one"), entry("b", "two"), entry("c", "three")};
    CHECK(startRow(list, "") == 0);
    CHECK(startRow(list, "b/two") == 1);
    CHECK(startRow(list, "c/three") == 2);
    CHECK(startRow(list, "gone/x") == 0); // a choice no longer offered
    CHECK(startRow({}, "b/two") == 0);
}

TEST_CASE("the first line is the game's title, with its variant after it") {
    CHECK(firstLine(entry("a", "Doom II")) == "Doom II");
    CHECK(firstLine(entry("a", "Freedoom", "Phase 2")) == "Freedoom (Phase 2)");
}

TEST_CASE("elide: whole characters, one line, the ellipsis only when something was cut") {
    CHECK(elide("short", 100, tenPerChar) == "short");
    CHECK(elide("abcdefghij", 100, tenPerChar) == "abcdefghij"); // exactly fits
    CHECK(elide("abcdefghijk", 100, tenPerChar) ==
          "abcdefg..."); // 7 + 3 dots = 10 chars = 100 px: the longest that fits
    CHECK(elide("abcdefghijklmnop", 100, tenPerChar) == "abcdefg...");
    // a multi-byte character is never cut in the middle
    const string cut = elide("zażółć gęślą jaźń", 100, tenPerChar);
    CHECK(cut.size() > 3);
    CHECK(cut.substr(cut.size() - 3) == "...");
    CHECK(tenPerChar(cut) <= 100);
    CHECK(elide("anything", 20, tenPerChar) == "...");     // no room: just the dots
    CHECK(elide("anything", 0, tenPerChar) == "anything"); // no width known: as it is
}

TEST_CASE("clipChars: whole characters, the dots count, nothing cut when it fits") {
    CHECK(clipChars("short", 26) == "short");
    CHECK(clipChars("exactly ten", 11) == "exactly ten");
    CHECK(clipChars("abcdefghijklmnop", 10) == "abcdefg...");
    CHECK(clipChars("zażółć gęślą jaźń", 10) == "zażółć ...");
    CHECK(clipChars("abc", 2) == "...");
    CHECK(clipChars("", 5) == "");
}

TEST_CASE("the second line: package, source and kind; the kind goes first, then the package title shortens") {
    // 16:9: plenty of room for all three
    CHECK(secondLine("Freedoom", "Store", "Doom data", 600, tenPerChar) == "Freedoom (Store) - Doom data");
    // 4:3: the kind is dropped before the package's title is touched
    const int narrow = tenPerChar("Freedoom (Store)") + 10;
    CHECK(secondLine("Freedoom", "Store", "Doom data", narrow, tenPerChar) == "Freedoom (Store)");
    // narrower still: the title is elided, the source label stays
    const string tight = secondLine("The Ultimate Doom Collection", "Your files", "Doom data", 200, tenPerChar);
    CHECK(tenPerChar(tight) <= 200);
    CHECK(tight.find("(Your files)") != string::npos);
    CHECK(tight.find("...") != string::npos);
    CHECK(tight.find("Doom data") == string::npos);
    // no kind name: no dangling dash
    CHECK(secondLine("Mine", "Your files", "", 600, tenPerChar) == "Mine (Your files)");
    // a very long title never wraps: one line within the width
    const string longTitle(300, 'x');
    CHECK(tenPerChar(secondLine(longTitle, "Store", "Doom data", 500, tenPerChar)) <= 500);
}

TEST_CASE("runsWith: the Apps whose Uses= names a kind of the package, in order, each once") {
    const vector<std::pair<string, vector<string>>> apps = {
        {"Crispy Doom", {"doom-iwad", "heretic-iwad"}},
        {"TyrQuake", {"quake-id1"}},
        {"LZDoom", {"doom-iwad"}},
        {"Crispy Doom", {"hexen-iwad"}}, // the same title twice: listed once
    };
    CHECK(runsWith({"doom-iwad"}, apps) == vector<string>{"Crispy Doom", "LZDoom"});
    CHECK(runsWith({"quake-id1", "hexen-iwad"}, apps) == vector<string>{"TyrQuake", "Crispy Doom"});
    CHECK(runsWith({"dos-game"}, apps).empty());
    CHECK(runsWith({}, apps).empty());
    CHECK(runsWith({"doom-iwad"}, {}).empty());
}
