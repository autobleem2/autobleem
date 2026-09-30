//
// Created by screemer on 2/15/19.
//

#pragma once

#include "evoui_obj.h"

#include <ab_gui/tween.h>

//******************
// PsMoveBtn
//******************
// The arrow that bobs: maxMove pixels down from originaly and back every period, slowing into both turns,
// drawn at fractional pixels. The bob is an ambient loop on the program's tweens (G5o2, abgui::ambient::pulse),
// started by the first update and again by restart() - the launcher restarts it each time the arrow shows or hides.
class PsMoveBtn : public PsObj {
public:
    int maxMove = 15;
    int originaly = 0;
    unsigned int period = 1000; // ms, down and back up
    float drawY = 0;            // written by the tween; originaly until the first update
    bool started = false;
    abgui::TweenOwner owner; // after the float it writes: it goes first and stops the tween

    // the bob starts again from the top now
    void restart();

    void update(long time) override;
    void render() override;

    using PsObj::PsObj;
};
