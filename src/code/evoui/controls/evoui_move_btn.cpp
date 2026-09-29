//
// Created by screemer on 2/15/19.
//

#include "evoui_move_btn.h"
#include "core/model/timing.h"

//*******************************
// PsMoveBtn::update
//*******************************
void PsMoveBtn::update(long time) {
    if (animationStarted == 0)
        animationStarted = time;
    drawY = originaly + maxMove * pulseWave(time - animationStarted, period);
    y = originaly; // where the arrow stands; the bob is drawY
    lastTime = time;
}

//*******************************
// PsMoveBtn::render
//*******************************
void PsMoveBtn::render() {
    if (!visible)
        return;
    if (drawY == 0.0f) // not updated yet
        drawY = static_cast<float>(originaly);
    renderer.copy(tex, nullptr,
                  ableem::FRect(static_cast<float>(x), drawY, static_cast<float>(w), static_cast<float>(h)));
}
