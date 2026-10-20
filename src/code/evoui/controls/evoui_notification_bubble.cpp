//
// NotificationBubble: the launcher's top-right corner panels - the scan's, the messages'. See the header.
//
#include "evoui_notification_bubble.h"
#include "gui/gui.h"

#include <ableem/engine/log.h>
#include <ab_gui/screen_stack.h>
#include <ab_gui/transitions.h>

#include <algorithm>

using namespace std;

namespace {
const long SlideMs = abgui::transition::BubbleSlideMs; // in from the edge, out to it
const int TitleHeight = 24;                            // the title's row
const int DetailHeight = 22;                           // the detail's row
const int BarHeight = 4;
const int Pad = 12;
} // namespace

//*******************************
// NotificationBubble::show
//*******************************
void NotificationBubble::show(const string &title, const string &detail, int64_t done, int64_t total, long holdMs) {
    // the clock is the platform's: a show() before the first render() (the launcher's "Showing:" line is
    // set while its assets load) must not count its hold from 0
    now_ = Gui::getInstance()->platform().ticks();
    title_ = title;
    detail_ = detail;
    done_ = max<int64_t>(0, done);
    total_ = max<int64_t>(0, total);
    if (state_ == State::Hidden || state_ == State::FadingOut) {
        // a bubble on its way out comes back from where it is
        const long elapsed = state_ == State::FadingOut ? max(0L, SlideMs - (now_ - stateSince_)) : 0;
        startSlide(State::SlidingIn, now_ - elapsed);
    }
    hold_.show(now_, holdMs);
    logged_ = holdMs > 0 && total_ == 0; // not the scan's / extension's progress, which show() every update
    if (logged_)
        PLOG_INFO << "toast show: '" << title << "' platform=" << now_ << " hold=" << holdMs
                  << " hideAt=" << hold_.hideAt();
}

//*******************************
// NotificationBubble::showFromFirstRender
//*******************************
void NotificationBubble::showFromFirstRender(const string &title, long holdMs) {
    show(title, "", 0, 0, 0);
    hold_.showFromFirstRender(now_, holdMs);
    logged_ = holdMs > 0;
    if (logged_)
        PLOG_INFO << "toast show: '" << title << "' platform=" << hold_.shownAt() << " hold=" << holdMs
                  << " counted from the end of the first frame";
}

//*******************************
// NotificationBubble::hide
//*******************************
void NotificationBubble::hide() {
    if (state_ == State::Hidden || state_ == State::FadingOut)
        return;
    if (logged_) {
        const long platform = Gui::getInstance()->platform().ticks();
        PLOG_INFO << "toast hide: '" << title_ << "' platform=" << platform << " shownAt=" << hold_.shownAt()
                  << " holdStart=" << hold_.holdStartAt() << " hideAt=" << hold_.hideAt() << " renderNow=" << now_
                  << " visible=" << (platform - hold_.shownAt()) << "ms held=" << (platform - hold_.holdStartAt())
                  << "ms of " << hold_.holdMs() << "ms";
        logged_ = false;
    }
    startSlide(State::FadingOut, now_);
}

//*******************************
// NotificationBubble::startSlide
//*******************************
// The slide is a tween of the time into it (linear, to the slide's end), drawn through the old easeOutCubic curve
// (abgui::transition::bubbleProgress) - so it lands where the hand-written timer did at every moment, also when it
// begins late (hide() from a frame after the last render: `since` is that render's time) or resumes (a show() on the
// way out).
// Its end is what moves the state on: a slide in ends Shown, a slide out Hidden. The hold between them is hold_,
// looked at in render() - no tween, so a bubble that stays for seconds is not busy; only the two slides are.
void NotificationBubble::startSlide(State slide, long since) {
    slideOwner_.cancel();
    state_ = slide;
    stateSince_ = since;
    const long gone = max(0L, static_cast<long>(Gui::getInstance()->platform().ticks()) - since);
    slideMs_ = static_cast<float>(min(gone, SlideMs));
    const State ends = slide == State::SlidingIn ? State::Shown : State::Hidden;
    Gui::getInstance()->uiContext().stack().tweens().start(
        abgui::transition::slideClock(slideMs_, slideMs_).onEnd([this, ends]() { state_ = ends; }), slideOwner_);
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
    // the slides end in the tweens (before the frame); the hold ends here
    // (the hold is on the platform's clock, so it is compared with that and not with `now`, which is the caller's own
    // time and may be older - the launcher sets its once a pass)
    if (state_ == State::Shown && hold_.expired(Gui::getInstance()->platform().ticks()))
        hide();
    if (state_ == State::Hidden)
        return;

    // the slide: the bubble's offset past the right edge, eased
    float progress = 1.0f;
    if (state_ != State::Shown)
        progress = abgui::transition::bubbleProgress(state_ == State::FadingOut, slideMs_);
    const int width = panelWidth(gui);
    const int offset = static_cast<int>((1.0f - progress) * (width + 16));

    PanelStyle style = gui.panelStyle();
    Fonts &fonts = gui.assets().themeFonts;
    const bool bar = total_ > 0;
    ableem::Rect panel(right - width + offset, top, width, height());
    style.toast(gui.uiContext(), panel);

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
        // the track in the secondary colour at the theme's barTrack alpha (else 120), the fill in the text colour
        style.progress(gui.uiContext(), track, static_cast<unsigned long long>(min(done_, total_)),
                       static_cast<unsigned long long>(total_), abgui::Tone::Secondary, abgui::Style::StyleAlpha,
                       abgui::Tone::Text, abgui::Style::OwnAlpha);
    }
    // The hold of showFromFirstRender() starts HERE, when the first frame that draws the bubble is done: that frame is
    // the slow one (the new theme's or the launcher's fonts and textures are made as they are first drawn, seconds on
    // the VM), and a hold counted from show() or from the start of the frame had it used up before anyone saw the
    // toast. The platform's clock, not `now`: the launcher's own time is still 0 in its very first frame.
    const long frameEnd = Gui::getInstance()->platform().ticks();
    if (hold_.frameDone(frameEnd))
        PLOG_INFO << "toast first frame done: '" << title_ << "' platform=" << frameEnd << " (show " << hold_.shownAt()
                  << ", +" << (frameEnd - hold_.shownAt()) << "ms) renderNow=" << now << " hideAt=" << hold_.hideAt();
}
