//
// Created by screemer on 2/11/19.
//

#include "evoui_settings_back.h"
#include "core/model/timing.h"
#include "gui/gui.h"

//*******************************
// PsSettingsBack::setCurLen
//*******************************
void PsSettingsBack::setCurLen(int len) {
    y = bottom - len;
    h = len;
    x = 0;
    w = width;
    nextLen = len;
    prevLen = len;
    owner_.cancel();
    sliding_ = false;
}

//*******************************
// PsSettingsBack::update
//*******************************
void PsSettingsBack::update(long time) {
    lastTime = time;
}

//*******************************
// PsSettingsBack::slideTo
//*******************************
void PsSettingsBack::slideTo(int len) {
    owner_.cancel();
    prevLen = h;
    nextLen = len;
    progress_ = 0;
    sliding_ = true;
    gui->uiContext().stack().tweens().start(
        abgui::Tween(progress_, 0.0f, 1.0f, evomotion::SettingsBandMs).onEnd([this]() {
            sliding_ = false;
            y = bottom - nextLen;
            h = nextLen;
            x = 0;
            w = width;
        }),
        owner_);
}

//*******************************
// PsSettingsBack::render
//*******************************
void PsSettingsBack::render() {
    // the band's length at this frame (a hidden band did not move before either)
    if (visible && sliding_) {
        const int newSize = evomotion::slidInt(prevLen, nextLen, progress_);
        y = bottom - newSize;
        h = newSize;
        x = 0;
        w = width;
    }
    PsObj::render();
}
