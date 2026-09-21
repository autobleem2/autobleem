//
// NotificationBubble: the launcher's top-right corner panels - the scan's, the messages'. See the header.
//
#include "evoui_notification_bubble.h"
#include "../../gui/gui.h"
#include "../../core/model/timing.h"

#include <algorithm>

using namespace std;

namespace {
const int SlideMs = 250;     // in from the edge, out to it
const int TitleHeight = 24;  // the title's row
const int DetailHeight = 22; // the detail's row
const int BarHeight = 4;
const int Pad = 12;
} // namespace

//*******************************
// NotificationBubble::show
//*******************************
void NotificationBubble::show(const string &title, const string &detail, int done, int total, long holdMs) {
    // the clock is the platform's: a show() before the first render() (the launcher's "Showing:" line is
    // set while its assets load) must not count its hold from 0
    now_ = Gui::getInstance()->platform().ticks();
    title_ = title;
    detail_ = detail;
    done_ = done;
    total_ = total;
    if (state_ == State::Hidden || state_ == State::FadingOut) {
        // a bubble on its way out comes back from where it is
        const long elapsed = state_ == State::FadingOut ? max(0L, SlideMs - (now_ - stateSince_)) : 0;
        state_ = State::SlidingIn;
        stateSince_ = now_ - elapsed;
    }
    hideAt_ = holdMs > 0 ? now_ + holdMs : 0;
}

//*******************************
// NotificationBubble::hide
//*******************************
void NotificationBubble::hide() {
    if (state_ == State::Hidden || state_ == State::FadingOut)
        return;
    state_ = State::FadingOut;
    stateSince_ = now_;
}

//*******************************
// NotificationBubble::height / panelWidth
//*******************************
int NotificationBubble::height() const {
    if (state_ == State::Hidden)
        return 0;
    const bool bar = total_ > 0;
    return Pad + TitleHeight + (detail_.empty() ? 0 : DetailHeight) + (bar ? BarHeight + 8 : 0) + Pad;
}

int NotificationBubble::panelWidth(Gui &gui) const {
    if (!fitWidth)
        return width;
    Fonts &fonts = gui.assets().themeFonts;
    int text = gui.text().textWidth(fonts[FONT_20_BOLD], title_);
    if (!detail_.empty())
        text = max(text, gui.text().textWidth(fonts[FONT_15_BOLD], detail_));
    return min(width, text + 2 * Pad);
}

//*******************************
// NotificationBubble::render
//*******************************
void NotificationBubble::render(Gui &gui, long now) {
    now_ = now;
    if (state_ == State::Hidden)
        return;
    if (state_ == State::SlidingIn && now - stateSince_ >= SlideMs) {
        state_ = State::Shown;
        stateSince_ = now;
    }
    if (state_ == State::Shown && hideAt_ != 0 && now >= hideAt_)
        hide();
    if (state_ == State::FadingOut && now - stateSince_ >= SlideMs) {
        state_ = State::Hidden;
        return;
    }

    // the slide: the bubble's offset past the right edge, eased
    float progress = 1.0f;
    if (state_ == State::SlidingIn)
        progress = easeOutCubic(static_cast<float>(now - stateSince_) / SlideMs);
    else if (state_ == State::FadingOut)
        progress = 1.0f - easeOutCubic(static_cast<float>(now - stateSince_) / SlideMs);
    const int width = panelWidth(gui);
    const int offset = static_cast<int>((1.0f - progress) * (width + 16));

    PanelStyle style = gui.panelStyle();
    Fonts &fonts = gui.assets().themeFonts;
    const bool bar = total_ > 0;
    ableem::Rect panel(right - width + offset, top, width, height());
    style.sheet(gui.renderer(), panel);

    const int textWidth = width - 2 * Pad;
    int y = panel.y + Pad;
    gui.text().renderText_WithColor(fonts[FONT_20_BOLD], gui.text().elide(fonts[FONT_20_BOLD], title_, textWidth),
                                    panel.x + Pad, y, style.text, XALIGN_LEFT);
    y += TitleHeight;
    if (!detail_.empty()) {
        gui.text().renderText_WithColor(fonts[FONT_15_BOLD], gui.text().elide(fonts[FONT_15_BOLD], detail_, textWidth),
                                        panel.x + Pad, y + 2, style.secondary, XALIGN_LEFT);
        y += DetailHeight;
    }
    if (bar) {
        y += 6;
        ableem::Rect track(panel.x + Pad, y, textWidth, BarHeight);
        gui.renderer().setBlendMode(ableem::BlendMode::Blend);
        gui.renderer().setDrawColor(ableem::Color(style.secondary.r, style.secondary.g, style.secondary.b, 120));
        gui.renderer().fillRect(track);
        const int fill = static_cast<int>(static_cast<long>(textWidth) * min(done_, total_) / total_);
        gui.renderer().setDrawColor(style.text);
        gui.renderer().fillRect(ableem::Rect(track.x, track.y, fill, BarHeight));
    }
}
