#include "evoui_disc_rip.h"
#include "disc_rip_logic.h"
#include "gui/gui.h"

#include <algorithm>

using namespace std;

namespace {
const int PanelWidth = 720;
const int TextInset = PanelStyle::RowInset + 8; // a line's x in the panel, and its right margin
const int LineHeight = 30;
const int BarHeight = 22;
} // namespace

//*******************************
// GuiDiscRip::init
//*******************************
void GuiDiscRip::init() {
    style = gui->panelStyle();
    progress = RipJob::Progress();
    finished = false;
    cancelling = false;
}

//*******************************
// GuiDiscRip::draw
//*******************************
void GuiDiscRip::draw() {
    RipJob::Progress now;
    {
        lock_guard<mutex> guard(lock);
        now = progress;
    }
    Fonts &fonts = gui->assets().themeFonts;
    const int percent = discrip::percentOf(now.sectorsDone, now.sectorsTotal);
    const double fraction =
        now.sectorsTotal == 0 ? 0.0 : min(1.0, static_cast<double>(now.sectorsDone) / now.sectorsTotal);

    const string title = cancelling ? _("Cancelling...") : _("Copying the disc");
    const string percentLine = to_string(percent) + " %";
    const string speedLine = _("Speed") + ": " + discrip::speedText(now.bytesPerSecond);
    const string leftLine =
        _("Time left") + ": " +
        discrip::timeText(discrip::secondsLeft(now.sectorsDone, now.sectorsTotal, now.bytesPerSecond));
    const string badLine = _("Unreadable sectors") + ": " + to_string(now.badSectors);

    // sized to its content: the title's band, the percent, the bar and three lines, the footer's band
    const int bodyHeight = 20 + LineHeight + BarHeight + 16 + 3 * LineHeight + 12;
    const int panelHeight = PanelStyle::HeaderHeight + bodyHeight + PanelStyle::FooterHeight;
    gui->renderBackground();
    style.dim(gui->uiContext());
    const int width = min(PanelWidth, renderer.width() - 2 * PanelStyle::Margin);
    const ableem::Rect panel{(renderer.width() - width) / 2, (renderer.height() - panelHeight) / 2, width,
                             panelHeight};
    style.sheet(gui->uiContext(), panel);

    const TextRenderer::Shadow classicShadow = gui->text().shadow();
    TextRenderer::Shadow shadow;
    shadow.enabled = style.textShadow;
    gui->text().setShadow(shadow);

    gui->text().renderText_WithColor(fonts[FONT_28_BOLD], title, panel.x + TextInset, panel.y + 18, style.text,
                                     XALIGN_LEFT);
    style.rule(gui->uiContext(), panel, panel.y + PanelStyle::HeaderHeight - 8);
    int y = panel.y + PanelStyle::HeaderHeight + 20;
    gui->text().renderText_WithColor(fonts[FONT_22_MED], percentLine, panel.x + TextInset, y, style.text, XALIGN_LEFT);
    y += LineHeight;
    style.progressBox(gui->uiContext(), ableem::Rect(panel.x + TextInset, y + 4, panel.w - 2 * TextInset, BarHeight),
                      fraction);
    y += BarHeight + 16;
    for (const string &line : {speedLine, leftLine, badLine}) {
        gui->text().renderText_WithColor(fonts[FONT_22_MED], line, panel.x + TextInset, y, style.description,
                                         XALIGN_LEFT);
        y += LineHeight;
    }

    // the one action; once pressed there is nothing left to press
    style.footer(*gui,
                 ableem::Rect(panel.x, panel.y + panel.h - PanelStyle::FooterHeight, panel.w, PanelStyle::FooterHeight),
                 cancelling ? vector<PanelStyle::HintItem>{} : vector<PanelStyle::HintItem>{{{"O"}, _("Abort")}}, "",
                 true);

    gui->text().setShadow(classicShadow);
}

//*******************************
// GuiDiscRip::loop
//*******************************
// The rip runs on a worker thread; this thread draws and reads the buttons. Everything but Circle is dropped, and the
// exit hold (the power-off handler) is set aside until the rip is over - a half-written disc is not left behind by a
// quit in the middle.
void GuiDiscRip::loop() {
    menuVisible = true;
    gui->input().setFrameNeed(ableem::Input::FrameNeed::Active);
    const function<void()> powerOff = gui->platform().setPowerOffHandler(nullptr);
    job.writeSbi = writeSbi;
    thread worker([this]() {
        RipJob::Result done = job.start(*drive, target, [this](const RipJob::Progress &p) {
            lock_guard<mutex> guard(lock);
            progress = p;
        });
        result = done;
        finished = true;
    });
    while (!finished) {
        if (gui->input().frameDue())
            render();
        Event e;
        while (gui->input().poll(e)) {
            if (e.type != Event::Type::Quit) // a quit waits for the rip, which cannot be left half done
                handle(e);
        }
        gui->platform().delay(10);
    }
    worker.join();
    gui->platform().setPowerOffHandler(powerOff);
    menuVisible = false;
}

//*******************************
// GuiDiscRip::onAction / onUnmapped
//*******************************
// Circle (and Escape): stop at once, no second question. Nothing else does anything - not the d-pad, not Cross.
void GuiDiscRip::onAction(const abgui::ActionEvent &action) {
    const Event &e = action.event;
    if (action.action == abgui::Action::Back &&
        (e.type == Event::Type::ButtonDown || (e.type == Event::Type::KeyDown && e.key == Key::Escape))) {
        if (!cancelling.exchange(true))
            job.cancel();
    }
}

void GuiDiscRip::onUnmapped(const ableem::Event &event) {
    if (event.type == Event::Type::KeyDown && event.key == Key::Escape && !cancelling.exchange(true))
        job.cancel();
}
