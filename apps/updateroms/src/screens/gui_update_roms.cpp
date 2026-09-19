//
// GuiUpdateRoms - see the header.
//
#include "gui_update_roms.h"
#include "gui/gui.h"

#include <ableem/engine/log.h>

#include <algorithm>

using namespace std;

//*******************************
// GuiUpdateRoms::~GuiUpdateRoms
//*******************************
GuiUpdateRoms::~GuiUpdateRoms() {
    stopJob();
}

//*******************************
// GuiUpdateRoms::onScanProgress
//*******************************
// on the worker thread: the scanner's per-folder and per-cover reports
void GuiUpdateRoms::onScanProgress(ableem::ScanStage stage, const string &detail, int done, int total) {
    lock_guard<mutex> lock(mutex_);
    switch (stage) {
    case ableem::ScanStage::ScanningRoms:
        stageText_ = _("Scanning") + " " + detail;
        break;
    case ableem::ScanStage::FetchingBoxArt:
        stageText_ = _("Fetching box art") + " " + detail;
        break;
    default:
        stageText_ = detail;
        break;
    }
    this->done = done;
    this->total = total;
}

//*******************************
// GuiUpdateRoms::startJob
//*******************************
void GuiUpdateRoms::startJob() {
    stop_.store(false);
    done_.store(false);
    worker_ = thread([this]() {
        UpdateRomsJob::Report result = UpdateRomsJob::run(
            setup, this, [this]() { return stop_.load(); },
            [this](const string &line) {
                lock_guard<mutex> lock(mutex_);
                lines_.push_back(line);
            },
            runner);
        {
            lock_guard<mutex> lock(mutex_);
            report = result;
            stageText_ = stop_.load() ? _("Stopped") : _("Done");
            done = total = 0;
        }
        done_.store(true);
    });
}

//*******************************
// GuiUpdateRoms::stopJob
//*******************************
void GuiUpdateRoms::stopJob() {
    stop_.store(true);
    if (worker_.joinable())
        worker_.join();
}

//*******************************
// GuiUpdateRoms::render
//*******************************
void GuiUpdateRoms::render() {
    string stage;
    int d, t;
    vector<string> lines;
    bool finished = done_.load();
    {
        lock_guard<mutex> lock(mutex_);
        stage = stageText_;
        d = done;
        t = total;
        lines = lines_;
    }

    gui->renderBackground();
    gui->renderTextBar();
    int yoffset = gui->renderLogo(true);
    const ableem::Rect panel = gui->text().getOpscreenRectOfTheme();
    const ableem::Font &font = gui->assets().themeFont;
    const int lineHeight = font.lineHeight();
    const ableem::Color color = TextRenderer::toColor(app.theme().classic().textColor, 255);

    gui->text().renderTextLine("-=" + _("Update ROMs") + "=-", 0, yoffset, XALIGN_CENTER);
    int y = yoffset + lineHeight * 2;

    // what it is on, and the bar under it
    gui->text().renderTextLine(stage.empty() ? _("Starting...") : stage, -y, 0, XALIGN_CENTER);
    y += lineHeight + 4;
    const int barX = panel.x + 40, barW = panel.w - 80, barH = lineHeight / 2;
    renderer.setDrawColor(ableem::Color{color.r, color.g, color.b, 90});
    renderer.fillRect(ableem::Rect{barX, y, barW, barH});
    if (t > 0) {
        renderer.setDrawColor(color);
        renderer.fillRect(ableem::Rect{barX, y, barW * min(d, t) / t, barH});
    } else if (finished) {
        renderer.setDrawColor(color);
        renderer.fillRect(ableem::Rect{barX, y, barW, barH});
    }
    y += barH + lineHeight;

    // the log, as many of the last lines as fit above the status bar
    const int rows = max(1, (panel.y + panel.h - y) / lineHeight);
    const size_t first = lines.size() > static_cast<size_t>(rows) ? lines.size() - rows : 0;
    for (size_t i = first; i < lines.size(); i++) {
        gui->text().renderText_WithColor(font, gui->text().elide(font, lines[i], panel.w - 20), panel.x + 10, y, color);
        y += lineHeight;
    }

    gui->renderStatus(finished ? "|@X| " + _("Close") : "|@O| " + _("Stop"));
    renderer.present();
}

//*******************************
// GuiUpdateRoms::loop
//*******************************
void GuiUpdateRoms::loop() {
    menuVisible = true;
    gui->input().setPowerKeyAsKey(true); // Escape is "stop" or "close" here, never the power switch
    startJob();
    while (menuVisible) {
        render();
        Event e;
        while (gui->input().poll(e)) {
            const bool close =
                e.type == Event::Type::Quit ||
                (e.type == Event::Type::ButtonDown && (e.button == Button::Cross || e.button == Button::Circle)) ||
                (e.type == Event::Type::KeyDown &&
                 (e.key == Key::Escape || e.key == Key::Sleep || e.key == Key::Return));
            if (!close)
                continue;
            if (done_.load()) {
                menuVisible = false;
            } else if (!stop_.load()) {
                // a stop is asked for once; the job returns at its next checkpoint (a download in flight
                // finishes or times out first) and the screen then shows Close
                stop_.store(true);
                lock_guard<mutex> lock(mutex_);
                stageText_ = _("Stopping...");
            }
        }
        gui->platform().delay(30);
    }
    stopJob();
}
