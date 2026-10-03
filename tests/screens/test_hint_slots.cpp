//
// The launcher's hint bar as a fixed grid of role slots (UIREV-36, evoui/controls/hint_slots.h): which item sits in
// which slot in each state, which line 2 items are dimmed, and the column every item belongs to (what a column's width
// is measured over). The drawing and the measuring need a live Gui and are the walk's job; the grid's own layout is
// autobleem-core's test_ab_gui_hint_bar.
//
#include "doctest/doctest.h"

#include "hint_slots.h"

#include <set>
#include <string>
#include <vector>

using namespace HintSlots;

namespace {

State games(bool app = false, bool retroArch = false) {
    State s;
    s.screen = Screen::Games;
    s.app = app;
    s.retroArch = retroArch;
    return s;
}

State gameMenu(bool empty) {
    State s;
    s.screen = Screen::GameMenu;
    s.emptyRoster = empty;
    return s;
}

State resume(bool load, bool used = false) {
    State s;
    s.screen = load ? Screen::ResumeLoad : Screen::ResumeSave;
    s.slotUsed = used;
    return s;
}

// line 1 as the design's table reads it: four items, None for an empty slot
void checkLine1(const Grid &g, Item a, Item b, Item c, Item d) {
    CHECK(g.line1[0].item == a);
    CHECK(g.line1[1].item == b);
    CHECK(g.line1[2].item == c);
    CHECK(g.line1[3].item == d);
}

// line 2: always Select, Start, Triangle, L2+R2; which of them are dimmed
void checkLine2(const Grid &g, bool dimSelect, bool dimStart, bool dimGuide) {
    CHECK(g.line2[0].item == Item::GamesShown);
    CHECK(g.line2[1].item == Item::Random);
    CHECK(g.line2[2].item == Item::Guide);
    CHECK(g.line2[3].item == Item::System);
    CHECK(g.line2[0].dim == dimSelect);
    CHECK(g.line2[1].dim == dimStart);
    CHECK(g.line2[2].dim == dimGuide);
    CHECK_FALSE(g.line2[3].dim); // System always works
}

} // namespace

TEST_CASE("Games, a PS1 game with RetroArch installed: Play, Play in RetroArch, Game menu, Quick menu") {
    const Grid g = gridFor(games(false, true));
    checkLine1(g, Item::Play, Item::PlayInRetroArch, Item::GameMenu, Item::QuickMenu);
    checkLine2(g, false, false, false);
}

TEST_CASE("Games, a PS1 game without RetroArch or a RetroArch entry: slot 2 stays empty") {
    const Grid g = gridFor(games());
    checkLine1(g, Item::Play, Item::None, Item::GameMenu, Item::QuickMenu);
    checkLine2(g, false, false, false);
}

TEST_CASE("Games, an App: X Start, slot 2 empty") {
    const Grid g = gridFor(games(true));
    checkLine1(g, Item::Start, Item::None, Item::GameMenu, Item::QuickMenu);
    checkLine2(g, false, false, false);
}

TEST_CASE("the game menu row: Open, Choose, Back to games; Select and Start dimmed") {
    const Grid g = gridFor(gameMenu(false));
    checkLine1(g, Item::Open, Item::None, Item::Choose, Item::BackToGames);
    checkLine2(g, true, true, false);
}

TEST_CASE("the game menu row on an empty set: Open and Up Quick menu; Start and Guide dimmed") {
    const Grid g = gridFor(gameMenu(true));
    checkLine1(g, Item::Open, Item::None, Item::None, Item::QuickMenu);
    checkLine2(g, false, true, true);
}

TEST_CASE("Games on an empty set: Up Quick menu only; Start and Guide dimmed") {
    const Grid g = gridFor(games());
    State s = games();
    s.emptyRoster = true;
    const Grid e = gridFor(s);
    checkLine1(e, Item::None, Item::None, Item::None, Item::QuickMenu);
    checkLine2(e, false, true, true);
    CHECK(g.line1[0].item == Item::Play); // a roster with games is untouched
}

TEST_CASE("Resume, loading: Resume, Delete slot only on a used slot, Slot, Back; Select, Start, Guide dimmed") {
    checkLine1(gridFor(resume(true, true)), Item::Resume, Item::DeleteSlot, Item::Slot, Item::Back);
    checkLine1(gridFor(resume(true, false)), Item::Resume, Item::None, Item::Slot, Item::Back);
    checkLine2(gridFor(resume(true, true)), true, true, true);
    checkLine2(gridFor(resume(true, false)), true, true, true);
}

TEST_CASE("Resume, saving: Save, Slot, Don't save; Select, Start, Guide dimmed") {
    const Grid g = gridFor(resume(false));
    checkLine1(g, Item::Save, Item::None, Item::Slot, Item::DontSave);
    checkLine2(g, true, true, true);
}

TEST_CASE("an item in a state never leaves its column: line 1 and line 2, every state") {
    std::vector<State> states = {games(),        games(true),  games(false, true), gameMenu(false),
                                 gameMenu(true), resume(true), resume(true, true), resume(false)};
    State empty = games();
    empty.emptyRoster = true;
    states.push_back(empty);
    for (const State &s : states) {
        const Grid g = gridFor(s);
        for (int c = 0; c < Columns; c++) {
            if (g.line1[c].item != Item::None)
                CHECK(columnOf(g.line1[c].item) == c);
            CHECK(columnOf(g.line2[c].item) == c);
            CHECK_FALSE(g.line1[c].dim); // only line 2 dims
        }
    }
}

TEST_CASE("every item has a column and a marker string; None has neither") {
    CHECK(columnOf(Item::None) == -1);
    CHECK(std::string(markersOf(Item::None)).empty());
    for (int i = 1; i < ItemCount; i++) {
        CAPTURE(i);
        const Item item = static_cast<Item>(i);
        CHECK(columnOf(item) >= 0);
        CHECK(columnOf(item) < Columns);
        CHECK(std::string(markersOf(item)).size() > 3);
    }
}

TEST_CASE("itemsOfColumn lists each item once, in its own column, the four columns together all of them") {
    std::set<int> seen;
    for (int c = 0; c < Columns; c++) {
        Item items[ItemCount];
        const int n = itemsOfColumn(c, items);
        CHECK(n > 0);
        for (int i = 0; i < n; i++) {
            CHECK(columnOf(items[i]) == c);
            CHECK(seen.insert(static_cast<int>(items[i])).second);
        }
    }
    CHECK(static_cast<int>(seen.size()) == ItemCount - 1);
}

TEST_CASE("the chips: the face buttons, the d-pad as arrows, L2+R2 one chip") {
    CHECK(std::string(markersOf(Item::Play)) == "|@X|");
    CHECK(std::string(markersOf(Item::PlayInRetroArch)) == "|@S|");
    CHECK(std::string(markersOf(Item::Guide)) == "|@T|");
    CHECK(std::string(markersOf(Item::Choose)) == "|@Left+Right|");
    CHECK(std::string(markersOf(Item::Slot)) == "|@Left+Right|");
    CHECK(std::string(markersOf(Item::System)) == "|@L2+R2|");
    CHECK(std::string(markersOf(Item::Random)) == "|@Start|");
    CHECK(std::string(markersOf(Item::GamesShown)) == "|@Select|");
}

TEST_CASE("the dim is 35 % of 255") {
    CHECK(DimAlpha == 89);
}
