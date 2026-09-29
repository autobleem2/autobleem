//
// Created by screemer on 2/15/19.
//

#pragma once

#include "evoui_obj.h"

//******************
// PsMoveBtn
//******************
// The arrow that bobs: maxMove pixels down from originaly and back every period, slowing into both turns,
// drawn at fractional pixels.
class PsMoveBtn : public PsObj {
public:
    int maxMove = 15;
    int originaly = 0;
    long period = 1000;        // ms, down and back up
    long animationStarted = 0; // when the bob began; the launcher restarts it each time the arrow shows
    float drawY = 0;

    void update(long time) override;
    void render() override;

    using PsObj::PsObj;
};
