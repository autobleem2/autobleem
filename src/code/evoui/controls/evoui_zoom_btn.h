//
// Created by screemer on 2/11/19.
//
#pragma once

#include "evoui_obj.h"

//******************
// PsZoomBtn
//******************
// The play button's pulse: from its own size to maxZoom and back every period, slowing into both turns,
// grown about its centre and drawn at fractional pixels so neither the size nor the centre steps.
class PsZoomBtn : public PsObj {

    float maxZoom = 1.20f;
    long period = 2000; // ms, the whole grow-and-shrink
    long started = 0;   // when the pulse began; 0 = at the next update
    float drawX = 0, drawY = 0, drawW = 0, drawH = 0;

    void update(long time) override;
    void render() override;

    using PsObj::PsObj;
};
