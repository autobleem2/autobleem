#include "evoui_notification_line.h"
#include "gui/gui.h"
#include <ableem/engine/log.h>

using namespace std;

namespace {
const int Gap = 8; // between stacked bubbles
} // namespace

//*******************************
// NotificationLine::setText
//*******************************
void NotificationLine::setText(const string &text, long timeLimit) {
    PLOG_INFO << "DBG54 NotificationLine::setText '" << text << "' limit=" << timeLimit << " this=" << (const void *)this;
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
void NotificationLines::create(int count, int maxWidth, int right) {
    // a bubble owns its tweens and is never copied or moved (they write its floats): built in place
    PLOG_INFO << "DBG54 NotificationLines::create count=" << count;
    lines = vector<NotificationLine>(static_cast<size_t>(count));
    for (NotificationLine &line : lines) {
        line.bubble.fitWidth = true;
        line.bubble.width = maxWidth;
        line.bubble.right = right;
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
