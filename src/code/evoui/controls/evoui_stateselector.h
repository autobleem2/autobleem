//
// Created by screemer on 2019-02-22.
//

#pragma once

#include "evoui_obj.h"
#include "core/model/ps_game.h"
#include "gui/gui_font.h"
#include "resume_layout.h"

#include <string>

#define OP_LOAD 0
#define OP_SAVE 1

//******************
// PsStateSelector
//******************
// The resume-slot screen, in the theme's look (UIREV-37; the layout numbers are ResumeLayout's).
class PsStateSelector : public PsObj {
public:
    int operation = 0;
    void render() override;

    void loadSaveStateImages(PsGamePtr &game, bool saving);
    void cleanSaveStateImages();

    void freeImages();

    ableem::Texture slotImg[4];
    bool slotActive[4];                      // the slot can be picked (load: it holds a state; save: always)
    bool slotUsed[4] = {};                   // the slot holds a state
    std::string slotDate[4];                 // its date as shown, "" when the console has no clock
    int newest = -1;                         // the slot with the latest time (two or more used), else -1
    std::string gameTitle;                   // under the heading

    ableem::Font font30;

    int selSlot = 0;

    using PsObj::PsObj;

private:
    // the slot's picture in `box`, clipped to the well's cut corners
    void drawPicture(const ableem::Texture &picture, const ResumeLayout::Box &box);
};
