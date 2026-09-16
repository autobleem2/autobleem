//
// Created by screemer on 2019-02-22.
//

#pragma once

#include "evoui_obj.h"
#include "../../core/model/ps_game.h"
#include "../../gui/gui_font.h"

#define OP_LOAD 0
#define OP_SAVE 1

//******************
// PsStateSelector
//******************
class PsStateSelector : public PsObj {
public:
    int operation=0;
    void render();

    void loadSaveStateImages(PsGamePtr & game, bool saving);
    void cleanSaveStateImages();

    void freeImages();

    ableem::Texture frame;

    ableem::Texture slotImg[4];
    bool slotActive[4];

    ableem::Font font30;
    ableem::Font font24;

    int selSlot = 0;

    using PsObj::PsObj;
};
