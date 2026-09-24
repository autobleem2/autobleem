//
// Created by screemer on 2019-01-24.
//
#pragma once

#include "gui/menus/gui_two_column_string_menu.h"
#include "../game_detail_pane.h"
#include "../../app.h"
#include "core/model/ps_game.h"
#include <ableem/ui/texture.h>

//********************
// GuiManager
//********************
class GuiManager : public GuiTwoColumnStringMenu {
public:
    App &app = App::get(); // the game model, over GuiScreen's AppBase (see gui_screen.h)
    explicit GuiManager(ableem::GuiBase &_gui) : GuiTwoColumnStringMenu(_gui) {}

    void init() override;
    void render() override;

    std::string getTitle() override;
    std::string getStatusLine() override;

    void doCircle_Pressed() override;
    void doSquare_Pressed() override;
    void doTriangle_Pressed() override;
    void doCross_Pressed() override;

    void doEnter() override { doCross_Pressed(); }
    void doEscape() override { doCircle_Pressed(); }
    void doDelete() override { doSquare_Pressed(); }

    PsGames psGames;
    // the folders the scan found but did not add, after the games (regional.db's FAILED_GAMES - they used to
    // be listed in gamesThatFailedVerifyCheck.txt): the folder, and why, in the detail pane; Square deletes one
    ableem::FailedGames failed;
    static bool sortByTitle(PsGamePtr i, PsGamePtr j) { return lessCaseInsensitive(i->title, j->title); }
    // UsbGame::verify()'s reasons, which the scan keeps in English, in the language on screen
    static std::string translatedReason(const std::string &reason);

private:
    bool onFailed() const { return selected >= static_cast<int>(psGames.size()); }
    const ableem::FailedGame &selectedFailed() const { return failed[selected - psGames.size()]; }
    void deleteFailedFolder();
    void settleSelection(); // after a delete: the cursor inside the list again

    // the selected game's cover and screenshot, on the left where the editor puts its cover; loaded when
    // the selection changes (a Texture dies with this stack object, before any launch)
    void renderPreview();
    int previewFor = -1;
    ableem::Texture previewCover;
    ableem::Texture previewSnap;
    GameDetailPane pane; // the selected game's cover, screenshot and facts, on the right
};
