//
// GuiSetPicker: what Select opens in the launcher - one screen for "which games are shown": three tabs,
// PlayStation, RetroArch and Apps (icons), L1/R1 between them; inside a tab the groups as rows - all
// games, internal, favorites, history, light-gun games and every games folder on the PlayStation tab, a
// playlist each on the RetroArch tab, the one Apps group on the last - Up/Down over them, L2/R2 a page,
// Cross picks, Circle leaves things as they are. It replaces Select cycling the sets and L2+Select opening
// a folder or playlist picker (2026-09-21).
//
#pragma once

#include "../../app.h"
#include "core/model/game_set.h"
#include "gui/gui_screen.h"
#include "gui/panel_style.h"

#include <string>
#include <vector>

class GuiSetPicker : public GuiScreen {
public:
    App &app = App::get();
    void init() override;
    void render() override;
    void loop() override;

    GameSetSelection selection; // in: what shows now; out: what was picked
    std::vector<std::string> raPlaylists;
    ableem::Texture background; // the launcher's frame, drawn dimmed under the panel
    bool cancelled = true;

    using GuiScreen::GuiScreen;

private:
    struct Entry {
        std::string title;
        std::string detail; // "21 games"
        int indent = 0;     // a folder's depth
        // the selection it stands for
        GameSet set = GameSet::PS1;
        Ps1SelectState ps1State = Ps1SelectState::AllGames;
        int index = 0;                              // the folder row or the playlist
        std::string name;                           // the folder's or playlist's name
        AppCategory appCategory = AppCategory::All; // the Apps tab's row
    };
    struct Tab {
        std::string title;
        ableem::Texture icon;
        std::vector<Entry> entries;
        int selected = 0;
        int firstVisible = 0;
    };
    std::vector<Tab> tabs;
    int tab = 0;
    PanelStyle style;

    void buildTabs();
    int visibleRows() const;
    void moveSelection(int step);
    void keepSelectedVisible();
    void pick();
};
