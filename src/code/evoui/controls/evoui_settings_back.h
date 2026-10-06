//
// Created by screemer on 2/11/19.
//

#pragma once

#include <ab_gui/tween.h>
#include "evoui_motion.h"
#include "evoui_obj.h"

//******************
// PsSettingsBack
//******************
class PsSettingsBack : public PsObj {
public:
    void setCurLen(int len);
    // the band to another length (100 ms, easeOutCubic): a non-ambient tween (ab_gui G5o3) started here from the
    // length it has now; a new one replaces the one running
    void slideTo(int len);
    bool sliding() const { return sliding_; }
    void update(long time) override;
    void render() override;

    int nextLen = 0;
    int prevLen = 0;
    // where the band ends and how wide it is: 632 and the 1280 canvas, or the 4:3 layout's
    int bottom = 632;
    int width = SCREEN_WIDTH;

    using PsObj::PsObj;

private:
    float progress_ = 0; // the slide's eased progress, written by its tween
    bool sliding_ = false;
    abgui::TweenOwner owner_; // the tween stops with the band
};
