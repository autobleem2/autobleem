//
// Created by screemer on 2/11/19.
//

#include "evoui_zoom_btn.h"

#include <ab_gui/ambient.h>
#include <ab_gui/screen_stack.h>

//*******************************
// PsZoomBtn::update
//*******************************
// the size is a function of the time since the pulse began (the tween's, not a sum of per-frame steps): a late frame
// lands where it should, and nothing drifts
void PsZoomBtn::update(long time) {
    if (!started) {
        started = true;
        gui->uiContext().stack().tweens().start(abgui::ambient::pulse(zoom, 1.0f, maxZoom, period), owner);
    }
    lastTime = time;
}

//*******************************
// PsZoomBtn::render
//*******************************
void PsZoomBtn::render() {
    if (!visible)
        return;
    renderer.copy(tex, nullptr, drawRect());
}
