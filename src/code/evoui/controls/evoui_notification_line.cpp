#include "evoui_notification_line.h"
#include "gui/gui.h"

using namespace std;

namespace {
const int Gap = 8;        // between stacked bubbles
const int MaxWidth = 840; // a message grows with its text up to two thirds of the screen, then it is ellipsized
} // namespace

//*******************************
// NotificationLine::setText
//*******************************
void NotificationLine::setText(const string &text, long timeLimit) {
    bubble.show(text, "", 0, 0, timeLimit);
}

//*******************************
// NotificationLine::render
//*******************************
void NotificationLine::render(Gui &gui, long now, int top) {
    bubble.top = top;
    bubble.render(gui, now);
}

//*******************************
// NotificationLines::create
//*******************************
void NotificationLines::create(int count) {
    // a bubble owns its tweens and is never copied or moved (they write its floats): built in place
    lines = vector<NotificationLine>(static_cast<size_t>(count));
    for (NotificationLine &line : lines) {
        line.bubble.fitWidth = true;
        line.bubble.width = MaxWidth;
    }
}

//*******************************
// NotificationLines::render
//*******************************
int NotificationLines::render(Gui &gui, long now, int top) {
    for (NotificationLine &line : lines) {
        line.render(gui, now, top);
        if (line.visible())
            top += line.height() + Gap;
    }
    return top;
}
