//
// Created by screemer on 2/15/19.
//

#include "evoui_move_btn.h"

#include <ab_gui/ambient.h>
#include <ab_gui/screen_stack.h>

//*******************************
// PsMoveBtn::restart
//*******************************
void PsMoveBtn::restart() {
    owner.cancel();
    started = true;
    drawY = static_cast<float>(originaly);
    gui->uiContext().stack().tweens().start(
        abgui::ambient::pulse(drawY, static_cast<float>(originaly), static_cast<float>(originaly + maxMove), period),
        owner);
}

//*******************************
// PsMoveBtn::update
//*******************************
void PsMoveBtn::update(long time) {
    if (!started)
        restart();
    y = originaly; // where the arrow stands; the bob is drawY
    lastTime = time;
}

//*******************************
// PsMoveBtn::render
//*******************************
void PsMoveBtn::render() {
    if (!visible)
        return;
    const float top = started ? drawY : static_cast<float>(originaly); // not updated yet
    renderer.copy(tex, nullptr,
                  ableem::FRect(static_cast<float>(x), top, static_cast<float>(w), static_cast<float>(h)));
}
