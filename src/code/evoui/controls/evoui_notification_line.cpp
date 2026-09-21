#include "evoui_notification_line.h"
#include "../../gui/gui.h"

using namespace std;

namespace {
const int Gap = 8; // between stacked bubbles
}

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
    lines.clear();
    for (int line = 0; line < count; ++line) {
        NotificationLine notificationLine;
        notificationLine.bubble.fitWidth = true;
        lines.push_back(notificationLine);
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
