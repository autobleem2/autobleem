//
// NotificationBubble: a small panel at the launcher's right edge for something the user should know -
// the scan's progress, which games the carousel shows, a message. A title, a line of detail (elided to
// the bubble), a thin progress bar when there is a count; it slides in from the edge when it appears,
// stays while it is fed, and fades out a while after the last message (or at once when told to). Drawn in
// PanelStyle's look, so it reads as one of the launcher's panels. The bubbles stack under each other at
// the top-right corner (GuiLauncher::render): the scan's first, the notification lines' under it.
//
#pragma once

#include "gui/panel_style.h"

#include <ab_gui/tween.h>

#include <cstdint>
#include <string>

class Gui;

class NotificationBubble {
public:
    // shows (or updates) the bubble: `title` bold, `detail` under it, done/total as a bar when total > 0.
    // holdMs: how long it stays after this call (0 = until hide() or the next show). done/total are 64-bit:
    // an extension reports bytes, and a download past 2 GB must not turn negative on the 32-bit console
    void show(const std::string &title, const std::string &detail, int64_t done, int64_t total, long holdMs);
    // the fade-out starts now (a scan that finished, its summary shown for holdMs, then this)
    void hide();
    bool visible() const { return state_ != State::Hidden; }
    // the title as last shown (it stays after the bubble hides) - a caller that wants to hide only its own message
    const std::string &title() const { return title_; }
    // sliding in or fading out: the screen draws every frame meanwhile (a shown bubble is still)
    bool animating() const { return state_ == State::SlidingIn || state_ == State::FadingOut; }

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
    // starts the slide in or out as though it had begun at `since` (ticks): a linear tween of slideMs_, the time into
    // the slide, from what is gone of it now to its end (ab_gui/transitions.h); its end moves the state on. Any slide
    // still running stops first.
    void startSlide(State slide, long since);

    State state_ = State::Hidden;
    long stateSince_ = 0; // when the slide began (for a slide that resumes: when it would have)
    long hideAt_ = 0;     // Shown: when to start fading (0 = never) - a timestamp, not a tween: the hold is not busy

    std::string title_, detail_;
    int64_t done_ = 0, total_ = 0;
    long now_ = 0;

    float slideMs_ = 0;            // the time into the current slide - the tween's value
    abgui::TweenOwner slideOwner_; // after the float it writes: it goes first and stops the tween
};
