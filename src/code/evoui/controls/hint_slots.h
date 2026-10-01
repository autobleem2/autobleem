//
// The launcher's hint bar as a fixed grid of role slots (UIREV-36, design: autobleem-design
// themes/ab2.0.0/design/uirev36/README.md, "For the developer"). Pure - which item sits in which slot of which line in
// each state, and which of the always-shown second line are dimmed - so tests/screens/test_hint_slots holds it without
// a Gui; GuiLauncher::buildHintLines turns the result into markers and labels, layoutHints measures the columns.
//
// Line 1 / line 2, four columns each. A slot is by role, not by button: column 1 the action, 2 the other action,
// 3 move, 4 leave. Line 1 leaves a slot empty when the state has nothing for it; line 2 is always all four
// (Select, Start, Triangle, L2+R2), an item that does nothing in the state is dimmed.
//
#pragma once

#include <array>

namespace HintSlots {

constexpr int Columns = 4;

// every item that can ever sit in a slot; the column it belongs to is columnOf()
enum class Item {
    None,
    // column 1 - the action
    Play,       // X
    Start,      // X - an App
    Open,       // X - the game menu's icon row
    Resume,     // X - the resume picker, loading
    Save,       // X - the resume picker, saving
    GamesShown, // Select
    // column 2 - the other action
    PlayInRetroArch, // Square
    DeleteSlot,      // Triangle
    Random,          // Start
    // column 3 - move
    GameMenu, // Down
    Choose,   // Left/Right
    Slot,     // Left/Right
    Guide,    // Triangle
    // column 4 - leave
    QuickMenu,   // Up
    BackToGames, // Up
    Back,        // Circle
    DontSave,    // Circle
    System,      // L2+R2
};

constexpr int ItemCount = 19; // the Item values, None included

// the column an item lives in (0..3); None has none (-1)
constexpr int columnOf(Item item) {
    return item == Item::None         ? -1
           : item <= Item::GamesShown ? 0
           : item <= Item::Random     ? 1
           : item <= Item::Guide      ? 2
                                      : 3;
}

// the marker string an item is drawn with (the chips; the labels are translated by the launcher)
constexpr const char *markersOf(Item item) {
    switch (item) {
    case Item::Play:
    case Item::Start:
    case Item::Open:
    case Item::Resume:
    case Item::Save:
        return "|@X|";
    case Item::GamesShown:
        return "|@Select|";
    case Item::PlayInRetroArch:
        return "|@S|";
    case Item::DeleteSlot:
    case Item::Guide:
        return "|@T|";
    case Item::Random:
        return "|@Start|";
    case Item::GameMenu:
        return "|@Down|";
    case Item::Choose:
    case Item::Slot:
        return "|@Left+Right|";
    case Item::QuickMenu:
    case Item::BackToGames:
        return "|@Up|";
    case Item::Back:
    case Item::DontSave:
        return "|@O|";
    case Item::System:
        return "|@L2+R2|";
    case Item::None:
        break;
    }
    return "";
}

// a slot of the grid
struct Cell {
    Item item = Item::None;
    bool dim = false; // drawn at DimAlpha (line 2 only: the item does nothing in this state)
};

constexpr int DimAlpha = 89; // 35 % of 255

struct Grid {
    std::array<Cell, Columns> line1;
    std::array<Cell, Columns> line2;
};

// what the launcher knows about its state
enum class Screen { Games, GameMenu, ResumeLoad, ResumeSave };

struct State {
    Screen screen = Screen::Games;
    bool emptyRoster = false; // Games / GameMenu: the set has no games
    bool app = false;         // Games: the selected game is an App (X Start)
    bool retroArch = false;   // Games: Square plays in RetroArch (a PS1 game, RetroArch installed)
    bool slotUsed = false;    // ResumeLoad: the highlighted slot holds a save (Triangle deletes it)
};

// the line 2 items, in slot order (always shown)
constexpr std::array<Item, Columns> line2Items() {
    return {{Item::GamesShown, Item::Random, Item::Guide, Item::System}};
}

inline Grid gridFor(const State &state) {
    Grid g;
    const std::array<Item, Columns> all = line2Items();
    for (int c = 0; c < Columns; c++)
        g.line2[c].item = all[c];
    auto put = [&g](Item item) { g.line1[columnOf(item)].item = item; };
    auto dim = [&g](Item item) { g.line2[columnOf(item)].dim = true; };
    switch (state.screen) {
    case Screen::Games:
        if (state.emptyRoster) { // an empty set: only the way into the Quick menu
            put(Item::QuickMenu);
            dim(Item::Random);
            dim(Item::Guide);
            break;
        }
        put(state.app ? Item::Start : Item::Play);
        if (state.retroArch)
            put(Item::PlayInRetroArch);
        put(Item::GameMenu);
        put(Item::QuickMenu);
        break;
    case Screen::GameMenu:
        put(Item::Open);
        if (state.emptyRoster) { // Up opens the Quick menu: there is no Games state to go back to
            put(Item::QuickMenu);
            dim(Item::Random);
            dim(Item::Guide);
        } else {
            put(Item::Choose);
            put(Item::BackToGames);
            dim(Item::GamesShown);
            dim(Item::Random);
        }
        break;
    case Screen::ResumeLoad:
        put(Item::Resume);
        if (state.slotUsed)
            put(Item::DeleteSlot);
        put(Item::Slot);
        put(Item::Back);
        dim(Item::GamesShown);
        dim(Item::Random);
        dim(Item::Guide);
        break;
    case Screen::ResumeSave:
        put(Item::Save);
        put(Item::Slot);
        put(Item::DontSave);
        dim(Item::GamesShown);
        dim(Item::Random);
        dim(Item::Guide);
        break;
    }
    return g;
}

// every item that can sit in `column` (0..3), line 1 or 2 - what the column's width is measured over, so it never
// changes with the state. Returns the count; `out` has room for ItemCount.
inline int itemsOfColumn(int column, Item *out) {
    int n = 0;
    for (int i = 1; i < ItemCount; i++)
        if (columnOf(static_cast<Item>(i)) == column)
            out[n++] = static_cast<Item>(i);
    return n;
}

} // namespace HintSlots
