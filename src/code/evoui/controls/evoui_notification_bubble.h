//
// NotificationBubble: a small panel at the launcher's right edge for something the user should know -
// the scan's progress, which games the carousel shows, a message. A title, a line of detail (elided to
// the bubble), a thin progress bar when there is a count; it slides in from the edge when it appears,
// stays while it is fed, and fades out a while after the last message (or at once when told to). Drawn in
// PanelStyle's look, so it reads as one of the launcher's panels. The bubbles stack under each other at
// the top-right corner (GuiLauncher::render): the scan's first, the notification lines' under it.
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
    // the panel's height as render() draws it (0 while hidden) - the next bubble stacks under it
    int height() const;
    // the panel's width: `width`, or the text's own plus the padding when fitWidth is set (never wider)
    int panelWidth(Gui &gui) const;

    int right = 1280 - 16; // the bubble's right edge
    int top = 16;
    int width = 440;
    bool fitWidth = false; // a message bubble is as wide as its text; the scan's keeps its width

private:
    enum class State { Hidden, SlidingIn, Shown, FadingOut };
    State state_ = State::Hidden;
    long stateSince_ = 0; // when the state began
    long hideAt_ = 0;     // Shown: when to start fading (0 = never)

    std::string title_, detail_;
    int done_ = 0, total_ = 0;
    long now_ = 0;
};
