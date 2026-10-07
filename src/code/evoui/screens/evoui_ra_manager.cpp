#include "evoui_ra_manager.h"
#include "../../app.h"
#include "evoui_panel_common.h"
#include "gui/gui.h"
#include "gui/screens/gui_confirm.h"

#include <ableem/ui/debug_driver.h>

#include <algorithm>
#include <typeinfo>

using namespace std;
using namespace evoui_panel;

namespace {
const int RowHeight = PanelStyle::RowHeight;
const int RowInset = PanelStyle::RowInset;
const int LogLineHeight = 22;
const int LogLines = 3;
} // namespace

//*******************************
// GuiRaManager::init
//*******************************
void GuiRaManager::init() {
    style = gui->panelStyle();
    changed = false;
    selected_ = 0;
    state_ = RaJobService::Inspection();
    rows_.clear();
    App::get().raJob().startInspect(); // the size walk and the catalog fetch: a worker, the panel says "Checking..."
}

//*******************************
// GuiRaManager::rebuild
//*******************************
void GuiRaManager::rebuild() {
    rows_ = RaManager::panelRows(state_);
    selected_ = rows_.empty() ? 0 : min(selected_, static_cast<int>(rows_.size()) - 1);
}

//*******************************
// GuiRaManager::publish
//*******************************
// the DebugDriver's rows: the actions' titles, and the cursor among them
void GuiRaManager::publish() const {
    if (!ableem::DebugDriver::active())
        return;
    vector<string> titles;
    for (const RaManager::Row &row : rows_)
        titles.push_back(row.title);
    ableem::DebugDriver::publish(typeid(*this).name(), titles, rows_.empty() ? -1 : selected_);
}

//*******************************
// GuiRaManager::draw
//*******************************
void GuiRaManager::draw() {
    publish();
    const vector<string> lines = RaManager::stateLines(state_);
    // the header holds the title and the state lines under it; the rule sits under those
    const int headerHeight = PanelStyle::HeaderHeight + static_cast<int>(lines.size()) * LineHeight + 12;
    const int footerHeight = PanelStyle::FooterHeight;
    const int panelHeight = headerHeight + static_cast<int>(rows_.size()) * RowHeight + footerHeight;
    Fonts &fonts = gui->assets().themeFonts;
    vector<pair<const ableem::Font *, string>> texts;
    for (const string &line : lines)
        texts.emplace_back(&fonts[FONT_22_MED], line);
    for (const RaManager::Row &row : rows_) {
        texts.emplace_back(&fonts[FONT_22_MED], row.title);
        texts.emplace_back(&fonts[FONT_15_BOLD], row.description);
    }
    ableem::Rect panel = drawPanel(*gui, style, panelWidthFor(*gui, texts), panelHeight);
    const int textWidth = panel.w - 2 * TextInset;

    const TextRenderer::Shadow classicShadow = gui->text().shadow();
    TextRenderer::Shadow shadow;
    shadow.enabled = style.textShadow;
    gui->text().setShadow(shadow);

    gui->text().renderText_WithColor(fonts[FONT_28_BOLD], _("RetroArch"), panel.x + RowInset, panel.y + 18, style.text,
                                     XALIGN_LEFT);
    int y = panel.y + 66;
    for (const string &line : lines) {
        gui->text().renderText_WithColor(fonts[FONT_22_MED], gui->text().elide(fonts[FONT_22_MED], line, textWidth),
                                         panel.x + TextInset, y, style.description, XALIGN_LEFT);
        y += LineHeight;
    }
    style.rule(gui->uiContext(), panel, panel.y + headerHeight - 8);

    int rowY = panel.y + headerHeight;
    for (int i = 0; i < static_cast<int>(rows_.size()); i++) {
        const RaManager::Row &row = rows_[i];
        const bool sel = i == selected_;
        if (sel)
            style.selection(gui->uiContext(), ableem::Rect(panel.x + 1, rowY, panel.w - 2, RowHeight));
        // a greyed row is under the theme's `disabled` role, as in the System menu
        gui->text().renderText_WithColor(fonts[FONT_22_MED], row.title, panel.x + RowInset + 8, rowY + 7,
                                         row.enabled ? style.rowColor(sel)
                                                     : style.disabledColor(gui->uiContext(), style.rowColor(sel)),
                                         XALIGN_LEFT);
        gui->text().renderText_WithColor(fonts[FONT_15_BOLD],
                                         gui->text().elide(fonts[FONT_15_BOLD], row.description, textWidth),
                                         panel.x + RowInset + 8, rowY + 35, style.description, XALIGN_LEFT);
        if (!row.enabled)
            style.disabled(gui->uiContext(), ableem::Rect(panel.x + 1, rowY, panel.w - 2, RowHeight));
        rowY += RowHeight;
    }

    style.footer(*gui, ableem::Rect(panel.x, panel.y + panel.h - footerHeight, panel.w, footerHeight),
                 rows_.empty() ? vector<PanelStyle::HintItem>{{{"O"}, _("Back")}}
                               : vector<PanelStyle::HintItem>{{{"X"}, _("Select")}, {{"O"}, _("Back")}},
                 "", false);

    gui->text().setShadow(classicShadow);
}

//*******************************
// GuiRaManager::run
//*******************************
// the question (Cross yes, Circle no), then the job with its progress panel; afterwards the panel looks again
void GuiRaManager::run(const RaManager::Row &row) {
    {
        GuiConfirm confirm(*gui);
        confirm.label = RaManager::confirmQuestion(row.kind);
        confirm.show();
        if (!confirm.result)
            return;
    }
    RaJobService &jobs = App::get().raJob();
    if (!jobs.start(RaManager::actionFor(row.kind)))
        return;
    {
        GuiRaJobProgress progress(*gui);
        progress.show();
    }
    changed = true;
    state_ = RaJobService::Inspection();
    rows_.clear();
    jobs.startInspect();
}

//*******************************
// GuiRaManager::loop
//*******************************
void GuiRaManager::loop() {
    App &app = App::get();
    menuVisible = true;
    while (menuVisible) {
        if (!state_.ready) {
            state_ = app.raJob().pollInspect();
            if (state_.ready)
                rebuild();
        }
        if (!state_.ready) { // the check is running: a frame at a time
            render();
            gui->platform().delay(16);
        } else {
            gui->input().setFrameNeed(ableem::Input::FrameNeed::Idle); // nothing moves between presses
            if (gui->input().frameDue())
                render();
        }
        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit)
                menuVisible = false;
            switch (e.type) {
            case Event::Type::DpadDown:
            case Event::Type::DpadUp:
                if (rows_.empty())
                    break;
                if (gui->input().dpadUp()) {
                    app.audio().cursor.play();
                    selected_ = (selected_ + static_cast<int>(rows_.size()) - 1) % static_cast<int>(rows_.size());
                } else if (gui->input().dpadDown()) {
                    app.audio().cursor.play();
                    selected_ = (selected_ + 1) % static_cast<int>(rows_.size());
                }
                publish();
                break;
            case Event::Type::ButtonDown:
                if (e.button == Button::Cross && !rows_.empty() && rows_[selected_].enabled) {
                    app.audio().cursor.play();
                    const RaManager::Row row = rows_[selected_];
                    run(row);
                    gui->input().setFrameNeed(ableem::Input::FrameNeed::Active);
                } else if (e.button == Button::Circle) {
                    app.audio().cancel.play();
                    menuVisible = false;
                }
                break;
            default:
                break;
            }
        }
    }
}

//*******************************
// GuiRaJobProgress::init
//*******************************
void GuiRaJobProgress::init() {
    style = gui->panelStyle();
    status_ = App::get().raJob().status();
}

//*******************************
// GuiRaJobProgress::draw
//*******************************
void GuiRaJobProgress::draw() {
    Fonts &fonts = gui->assets().themeFonts;
    const RaManager::ProgressView view = RaManager::progressView(status_);
    // sized to its content: the title's band with its rule, the detail line, the bar, the log lines, the footer's band
    const int lineCount = static_cast<int>(min(view.log.size(), static_cast<size_t>(LogLines)));
    const int bodyHeight =
        20 + (view.detail.empty() ? 0 : LineHeight) + (view.fraction >= 0 ? 34 : 0) + lineCount * LogLineHeight + 12;
    const int panelHeight = PanelStyle::HeaderHeight + bodyHeight + PanelStyle::FooterHeight;
    ableem::Rect panel = drawPanel(
        *gui, style, panelWidthFor(*gui, {{&fonts[FONT_28_BOLD], view.title}, {&fonts[FONT_22_MED], view.detail}}),
        panelHeight);

    const TextRenderer::Shadow classicShadow = gui->text().shadow();
    TextRenderer::Shadow shadow;
    shadow.enabled = style.textShadow;
    gui->text().setShadow(shadow);

    gui->text().renderText_WithColor(fonts[FONT_28_BOLD], view.title, panel.x + TextInset, panel.y + 18, style.text,
                                     XALIGN_LEFT);
    style.rule(gui->uiContext(), panel, panel.y + PanelStyle::HeaderHeight - 8);
    int y = panel.y + PanelStyle::HeaderHeight + 20;
    if (!view.detail.empty()) {
        gui->text().renderText_WithColor(fonts[FONT_22_MED],
                                         gui->text().elide(fonts[FONT_22_MED], view.detail, panel.w - 2 * TextInset),
                                         panel.x + TextInset, y, style.description, XALIGN_LEFT);
        y += LineHeight;
    }
    if (view.fraction >= 0) {
        ableem::Rect bar(panel.x + TextInset, y + 4, panel.w - 2 * TextInset, 22);
        // the outline and the fill (or the theme's progressTrack/progressFill frames) - ab_gui G5g
        style.progressBox(gui->uiContext(), bar, view.fraction);
        y += 34;
    }
    for (int i = static_cast<int>(view.log.size()) - lineCount; i < static_cast<int>(view.log.size()); i++) {
        gui->text().renderText_WithColor(fonts[FONT_15_BOLD],
                                         gui->text().elide(fonts[FONT_15_BOLD], view.log[i], panel.w - 2 * TextInset),
                                         panel.x + TextInset, y, style.description, XALIGN_LEFT);
        y += LogLineHeight;
    }

    // a running job can be stopped (Circle asks); an outcome is dismissed with a button
    vector<PanelStyle::HintItem> hints;
    if (view.over)
        hints = {{{"X"}, _("OK")}};
    else if (view.canStop)
        hints = {{{"O"}, _("Stop")}};
    style.footer(*gui,
                 ableem::Rect(panel.x, panel.y + panel.h - PanelStyle::FooterHeight, panel.w, PanelStyle::FooterHeight),
                 hints, "", true);

    gui->text().setShadow(classicShadow);
}

//*******************************
// GuiRaJobProgress::loop
//*******************************
void GuiRaJobProgress::loop() {
    RaJobService &jobs = App::get().raJob();
    menuVisible = true;
    bool leaving = false;
    while (menuVisible) {
        status_ = jobs.poll();
        render();
        if (leaving && status_.over())
            menuVisible = false;
        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit) {
                // the launcher is leaving: the runner stops as it does for Circle (its state stays resumable) and
                // the panel closes when it has ended
                jobs.stop();
                leaving = true;
                break;
            }
            if (e.type != Event::Type::ButtonDown)
                continue;
            if (status_.over()) {
                menuVisible = false;
            } else if (e.button == Button::Circle && !status_.stopping) {
                GuiConfirm confirm(*gui);
                confirm.label = _("Stop? The job can be carried on later.");
                confirm.confirmLabel = _("Stop");
                confirm.cancelLabel = _("Go on");
                confirm.show();
                if (confirm.result)
                    jobs.stop(); // the runner gets SIGTERM; the panel says "Stopping..." until it ends
            }
        }
        gui->platform().delay(16);
    }
    finalStatus = status_;
}
