//
// NotificationLines: the launcher's messages - which games the carousel shows ("Showing: All Games (50
// games)", line 0) and whatever else it has to say (a resume point saved, a scan already running, the
// first letter of a jump; line 1). Each line is a NotificationBubble at the top-right corner, stacked
// under the scan's bubble, so every notification in the launcher looks the same (2026-09-21; they used to
// be two text lines across the top of the screen).
//
#pragma once

#include "evoui_notification_bubble.h"

#include <string>
#include <vector>

class Gui;

//******************
// NotificationLine
//******************
struct NotificationLine {
    NotificationBubble bubble;

    // timeLimit is in milliseconds; 0 = stays until the next setText
    void setText(const std::string &text, long timeLimit);
    void render(Gui &gui, long now, int top);
    bool visible() const { return bubble.visible(); }
    int height() const { return bubble.height(); }
};

//******************
// NotificationLines
//******************
struct NotificationLines {
    std::vector<NotificationLine> lines;
    NotificationLine &operator[](int i) { return lines[i]; };

    void create(int count);
    // draws the visible lines one under the other from `top`, 8 px apart; returns the y below the last
    int render(Gui &gui, long now, int top);
};
