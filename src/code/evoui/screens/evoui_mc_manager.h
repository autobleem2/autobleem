//
// Created by screemer on 2019-07-30.
//

#pragma once

#include <memory>

#include "gui/gui_screen.h"
#include "../card_edit.h"
#include <ableem/ui/texture.h>
#include <ableem/ui/font.h>
#include <string>

using namespace std;

class GuiMcManager : public GuiScreen {
public:
    string leftCardName;
    string rightCardName;
    ableem::Texture backgroundImg;
    string card1path;
    string card2path;

    string rightCardName_ori;
    string cardPath_ori;
    void render() override;
    void loop() override;
    void init() override;

    void loadAssets();
    using GuiScreen::GuiScreen;
    void trySave();

    bool changes = false;

private:
    int counter = 0;
    int animFrame = 0;
    ableem::Font fontJIS;
    void renderStatic();
    void renderPencil(int memcard, int row, int column);
    void renderMemCardIcons(int memcard);
    void renderMetaInfo();

    void pencilUp();
    void pencilDown();
    void pencilLeft();
    void pencilRight();

    std::unique_ptr<CardEdit> memcard1, memcard2;
    // the two grids (256x420) sit inside the panel's content, one at each side, the card names over
    // them; the icons and the pencil are placed from the grids' origins
    static const int GridW = 256, GridH = 420, Slot = 80, IconInset = 10, PencilInset = 70;
    ableem::Rect gridRect(int card) const; // 1 = left, 2 = right

    int pencilColumn;
    int pencilRow;
    int pencilMemcard;

    ableem::Rect pencilPos;
    ableem::Texture mcGrid;
    ableem::Texture mcPencil;
};
