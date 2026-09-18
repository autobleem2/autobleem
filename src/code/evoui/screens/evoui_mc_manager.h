//
// Created by screemer on 2019-07-30.
//

#pragma once

#include <memory>

#include "../../gui/gui_screen.h"
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
    const int mc1XStart = 150;
    const int mc2XStart = 1010;
    const int mcYStart = 150;

    int pencilColumn;
    int pencilRow;
    int pencilMemcard;

    ableem::Rect pencilPos;
    ableem::Texture mcGrid;
    ableem::Texture mcPencil;
};
