//
// GuiAppStart: the screen before an App runs - its README on the left, scrolling (Up/Down a line, L2/R2
// a page), the App's icon and facts in the detail pane on the right; Cross starts it, Circle does not.
//
#ifndef AUTOBLEEM_GUI_GUI_APP_START_H
#define AUTOBLEEM_GUI_GUI_APP_START_H

#include "../../app.h"
#include "../../gui/game_detail_pane.h"
#include "gui/gui_screen.h"
#include "core/model/ps_game.h"

#include <string>
#include <vector>

class GuiAppStart : public GuiScreen {
private:
    App &app = App::get();
    ableem::Font font;
    PsGamePtr game;
    std::string appName;
    std::vector<std::string> lines; // the README's
    bool readmeLoaded = false;
    int firstLine = 0;
    int lastLineShown = 0; // one past the last line render() fitted in
    int rowsThatFit = 1;
    GameDetailPane pane;

public:
    bool result = false;
    void setGame(PsGamePtr game) { this->game = game; };
    void init() override;
    void render() override;
    void loop() override;

    using GuiScreen::GuiScreen;
};

#endif // AUTOBLEEM_GUI_GUI_APP_START_H
