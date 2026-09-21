//
// NotificationBubble: a small panel in the launcher's top-right corner for something happening in the
// background - the scan. A title, a line of detail (elided to the bubble), a thin progress bar when there
// is a count; it slides in from the edge when it appears, stays while it is fed, and fades out a while
// after the last message (or at once when told to). Drawn in PanelStyle's look, so it reads as one of the
// launcher's panels.
//
#pragma once

#include "../../gui/panel_style.h"

#include <string>

class Gui;

class NotificationBubble {
public:
    // shows (or updates) the bubble: `title` bold, `detail` under it, done/total as a bar when total > 0.
    // holdMs: how long it stays after this call (0 = until hide() or the next show)
    void show(const std::string &title, const std::string &detail, int done, int total, long holdMs);
    // the fade-out starts now (a scan that finished, its summary shown for holdMs, then this)
    void hide();
    bool visible() const { return state_ != State::Hidden; }

    // once a frame, over the launcher's other elements
    void render(Gui &gui, long now);

    int right = 1280 - 16; // the bubble's right edge
    int top = 16;
    int width = 440;

private:
    enum class State { Hidden, SlidingIn, Shown, FadingOut };
    State state_ = State::Hidden;
    long stateSince_ = 0; // when the state began
    long hideAt_ = 0;     // Shown: when to start fading (0 = never)

    std::string title_, detail_;
    int done_ = 0, total_ = 0;
    long now_ = 0;
};
