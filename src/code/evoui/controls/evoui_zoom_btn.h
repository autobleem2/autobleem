//
// Created by screemer on 2/11/19.
//
#pragma once

#include "evoui_obj.h"

#include <ab_gui/tween.h>

//******************
// PsZoomBtn
//******************
// The play button's pulse: from its own size to maxZoom and back every period, slowing into both turns,
// grown about its centre and drawn at fractional pixels so neither the size nor the centre steps. The zoom is an
// ambient loop on the program's tweens (G5o2, abgui::ambient::pulse) that starts with the first update; the frame
// reads it when it draws.
class PsZoomBtn : public PsObj {

    float maxZoom = 1.20f;
    unsigned int period = 2000; // ms, the whole grow-and-shrink
    float zoom = 1.0f;          // written by the tween; its own size until the first update
    bool started = false;
    abgui::TweenOwner owner; // after the float it writes: it goes first and stops the tween

    void update(long time) override;
    void render() override;

    using PsObj::PsObj;

public:
    // where the pulse draws it this frame (its own place and size until the first update) - Play's outline follows it
    ableem::FRect drawRect() const {
        const float drawW = ow * zoom;
        const float drawH = oh * zoom;
        return ableem::FRect(ox - (drawW - ow) / 2.0f, oy - (drawH - oh) / 2.0f, drawW, drawH);
    }
};
