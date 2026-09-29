//
// Created by screemer on 2/11/19.
//

#include "evoui_zoom_btn.h"
#include "core/model/timing.h"

//*******************************
// PsZoomBtn::update
//*******************************
// the size is a function of the time since the pulse began, not a sum of per-frame steps: a late frame
// lands where it should, and nothing drifts
void PsZoomBtn::update(long time) {
    if (started == 0)
        started = time;
    const float zoom = 1.0f + (maxZoom - 1.0f) * pulseWave(time - started, period);
    drawW = ow * zoom;
    drawH = oh * zoom;
    drawX = ox - (drawW - ow) / 2.0f;
    drawY = oy - (drawH - oh) / 2.0f;
    lastTime = time;
}

//*******************************
// PsZoomBtn::render
//*******************************
void PsZoomBtn::render() {
    if (!visible)
        return;
    if (drawW <= 0.0f) // not updated yet
        update(lastTime);
    renderer.copy(tex, nullptr, ableem::FRect(drawX, drawY, drawW, drawH));
}
