//
// Created by screemer on 2019-01-24.
//
#pragma once

#include "gui_two_column_string_menu.h"
#include "../../core/model/ps_game.h"
#include <ableem/ui/texture.h>

//********************
// GuiManager
//********************
class GuiManager : public GuiTwoColumnStringMenu {
public:
    GuiManager(ableem::GuiBase &_gui) : GuiTwoColumnStringMenu(_gui) {}

    void init() override;
    void render() override;

    virtual std::string getTitle() override;
    std::string getStatusLine() override;

    virtual void doCircle_Pressed() override;
    virtual void doSquare_Pressed() override;
    virtual void doTriangle_Pressed() override;
    virtual void doCross_Pressed() override;

    virtual void doEnter() override { doCross_Pressed(); }
    virtual void doEscape() override { doCircle_Pressed(); }
    virtual void doDelete() override { doSquare_Pressed(); }

    PsGames psGames;
    static bool sortByTitle(PsGamePtr i, PsGamePtr j) { return lessCaseInsensitive(i->title, j->title); }

private:
    // the selected game's cover and screenshot, on the left where the editor puts its cover; loaded when
    // the selection changes (a Texture dies with this stack object, before any launch)
    void renderPreview();
    int previewFor = -1;
    ableem::Texture previewCover;
    ableem::Texture previewSnap;
    static const int PreviewWidth = 300;   // what the rows leave free on the left
};
