#include "evoui_processors.h"
#include "core/services/environment.h"
#include "core/services/scan_service.h"
#include "gui/gui.h"

#include <algorithm>

using namespace std;

namespace {
const int PanelWidth = 860;
const int PanelMargin = PanelStyle::Margin;
const int HeaderHeight = PanelStyle::HeaderHeight;
const int FooterHeight = PanelStyle::FooterHeight;
const int RowHeight = PanelStyle::RowHeight;
const int RowInset = PanelStyle::RowInset;
const int NumberWidth = 40; // the row's position in the sequence
const int EmptyHeight = 110;
} // namespace

//*******************************
// GuiProcessors::init
//*******************************
void GuiProcessors::init() {
    style = gui->panelStyle();
    ProcessorCatalog catalog(ScanService::processorsDir(), Env::appPlatformKeys());
    installed = catalog.scan();
    sequences = ProcessorSequences(ScanService::processorSequencesFile());
    if (sequences.load(installed) && !installed.empty())
        sequences.save(); // what the scan would write anyway: the new ones placed
    state = ProcessorState(ScanService::processorStateFilePath());
    state.load();
    selected = firstVisible = 0;
    moving = changed_ = stateChanged = false;
    // start on a sequence that has something in it
    sequence = sequences.entries(ProcessorSequence::Ps1).empty() && !sequences.entries(ProcessorSequence::Roms).empty()
                   ? ProcessorSequence::Roms
                   : ProcessorSequence::Ps1;
}

//*******************************
// GuiProcessors::info
//*******************************
const ProcessorInfo *GuiProcessors::info(const string &name) const {
    for (const ProcessorInfo &p : installed) {
        if (p.name == name)
            return &p;
    }
    return nullptr;
}

//*******************************
// GuiProcessors::visibleRows
//*******************************
int GuiProcessors::visibleRows() const {
    int roomForRows = SCREEN_HEIGHT - 2 * PanelMargin - HeaderHeight - FooterHeight;
    return max(1, min(count(), roomForRows / RowHeight));
}

//*******************************
// GuiProcessors::render
//*******************************
void GuiProcessors::render() {
    if (background.valid())
        renderer.copy(background, nullptr, nullptr);
    else
        gui->renderBackground();
    style.dim(renderer);

    const bool empty = count() == 0;
    const int rows = empty ? 0 : visibleRows();
    const int body = empty ? EmptyHeight : rows * RowHeight;
    const int panelHeight = HeaderHeight + body + FooterHeight;
    ableem::Rect panel{(SCREEN_WIDTH - PanelWidth) / 2, (SCREEN_HEIGHT - panelHeight) / 2, PanelWidth, panelHeight};
    style.sheet(renderer, panel);

    const TextRenderer::Shadow classicShadow = gui->text().shadow();
    TextRenderer::Shadow shadow;
    shadow.enabled = style.textShadow;
    gui->text().setShadow(shadow);

    Fonts &fonts = gui->assets().themeFonts;
    int rowY = style.header(*gui, panel, _("Scanner processors"));
    // the two tabs, at the header's right: the one showing in the text colour
    {
        const string roms = _("ROMs"), ps1 = _("PlayStation");
        const int y = panel.y + RowInset + 8;
        int x = panel.x + panel.w - RowInset - 8 - fonts[FONT_20_BOLD].width(roms);
        gui->text().renderText_WithColor(fonts[FONT_20_BOLD], roms, x, y,
                                         sequence == ProcessorSequence::Roms ? style.text : style.secondary,
                                         XALIGN_LEFT);
        x -= fonts[FONT_20_BOLD].width(ps1) + 28;
        gui->text().renderText_WithColor(fonts[FONT_20_BOLD], ps1, x, y,
                                         sequence == ProcessorSequence::Ps1 ? style.text : style.secondary,
                                         XALIGN_LEFT);
    }

    if (empty) {
        gui->text().renderText_WithColor(fonts[FONT_22_MED], _("No processors for these games"), panel.x + RowInset + 8,
                                         rowY + 20, style.text, XALIGN_LEFT);
        gui->text().renderText_WithColor(fonts[FONT_15_BOLD], _("Unpack a processor into System/Processors"),
                                         panel.x + RowInset + 8, rowY + 56, style.secondary, XALIGN_LEFT);
    }
    const auto &entries = sequences.entries(sequence);
    for (int i = firstVisible; i < firstVisible + rows && i < count(); i++) {
        const ProcessorSequences::Entry &e = entries[i];
        const ProcessorInfo *p = info(e.name);
        const ableem::Rect row(panel.x + 1, rowY, panel.w - 2, RowHeight);
        if (i == selected)
            style.selection(renderer, row);
        const int textX = panel.x + RowInset + 8 + NumberWidth;
        gui->text().renderText_WithColor(fonts[FONT_22_MED], to_string(i + 1), panel.x + RowInset + 8, rowY + 8,
                                         style.hint, XALIGN_LEFT);
        string title = p ? p->title : e.name;
        if (p && !p->version.empty())
            title += "  " + p->version;
        if (i == selected && moving)
            title = "\xC2\xBB " + title + " \xC2\xAB"; // the row being carried
        string line2;
        bool greyed = false;
        if (!p || !p->builtForThisSystem()) {
            line2 = _("Not available for this system");
            greyed = true;
        } else if (!e.enabled) {
            line2 = _("Switched off");
            greyed = true;
        } else if (rerun.count(e.name)) {
            line2 = _("Runs on everything again at the next scan");
        } else {
            line2 = p->description;
        }
        gui->text().renderText_WithColor(fonts[FONT_22_MED], title, textX, rowY + 8,
                                         i == selected ? style.text : style.secondary, XALIGN_LEFT);
        gui->text().renderText_WithColor(fonts[FONT_15_BOLD], line2, textX, rowY + 36, style.secondary, XALIGN_LEFT);
        if (greyed)
            style.disabled(renderer, row);
        rowY += RowHeight;
    }

    const int markerX = panel.x + panel.w - RowInset;
    if (firstVisible > 0)
        style.scrollMarker(renderer, markerX, panel.y + HeaderHeight - 4, -1);
    if (!empty && firstVisible + rows < count())
        style.scrollMarker(renderer, markerX, panel.y + HeaderHeight + rows * RowHeight + 2, 1);

    vector<PanelStyle::HintItem> hints;
    if (moving) {
        hints.push_back({{"S"}, _("Put down")});
    } else if (!empty) {
        hints.push_back({{"X"}, entries[selected].enabled ? _("Switch off") : _("Switch on")});
        hints.push_back({{"O"}, _("Back")});
        hints.push_back({{"T"}, _("Run again")});
        hints.push_back({{"S"}, _("Move")});
        hints.push_back({{"L1", "R1"}, _("Tab")});
    } else {
        hints.push_back({{"O"}, _("Back")});
        hints.push_back({{"L1", "R1"}, _("Tab")});
    }
    style.footer(*gui, ableem::Rect(panel.x, panel.y + panel.h - FooterHeight, panel.w, FooterHeight), hints, "",
                 false);

    gui->text().setShadow(classicShadow);
    renderer.present();
}

//*******************************
// GuiProcessors::moveSelection
//*******************************
// with a row picked up, the row moves with the cursor
void GuiProcessors::moveSelection(int step) {
    if (count() == 0)
        return;
    int target;
    if (moving)
        target = max(0, min(count() - 1, selected + step)); // carrying one: no wrap
    else if (step == 1 || step == -1)
        target = (selected + step + count()) % count();
    else
        target = max(0, min(count() - 1, selected + step));
    if (moving && sequences.move(sequence, selected, target))
        changed_ = true;
    selected = target;
    const int rows = visibleRows();
    if (selected < firstVisible)
        firstVisible = selected;
    else if (selected >= firstVisible + rows)
        firstVisible = selected - rows + 1;
}

//*******************************
// GuiProcessors::switchTab
//*******************************
void GuiProcessors::switchTab(ProcessorSequence to) {
    if (to == sequence)
        return;
    sequence = to;
    moving = false;
    selected = firstVisible = 0;
}

//*******************************
// GuiProcessors::save
//*******************************
void GuiProcessors::save() {
    if (changed_)
        sequences.save();
    if (stateChanged)
        state.save();
}

//*******************************
// GuiProcessors::loop
//*******************************
void GuiProcessors::loop() {
    menuVisible = true;
    while (menuVisible) {
        render();
        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit) {
                save();
                menuVisible = false;
            }
            switch (e.type) {
            case Event::Type::DpadDown:
            case Event::Type::DpadUp:
                if (gui->input().dpadUp()) {
                    app.audio().cursor.play();
                    moveSelection(-1);
                } else if (gui->input().dpadDown()) {
                    app.audio().cursor.play();
                    moveSelection(1);
                }
                break;
            case Event::Type::ButtonDown:
                if (e.button == Button::Square && count() > 0) {
                    app.audio().cursor.play();
                    moving = !moving;
                } else if (e.button == Button::Cross && moving) {
                    app.audio().cursor.play();
                    moving = false;
                } else if (e.button == Button::Cross && count() > 0) {
                    app.audio().cursor.play();
                    const auto &entry = sequences.entries(sequence)[selected];
                    sequences.setEnabled(sequence, selected, !entry.enabled);
                    changed_ = true;
                } else if (e.button == Button::Triangle && count() > 0 && !moving) {
                    // forgotten: it is offered everything again at the next scan
                    app.audio().cursor.play();
                    state.forget(sequences.entries(sequence)[selected].name);
                    stateChanged = true;
                    changed_ = true;
                    rerun.insert(sequences.entries(sequence)[selected].name);
                } else if (e.button == Button::L1 && !moving) {
                    app.audio().cursor.play();
                    switchTab(ProcessorSequence::Ps1);
                } else if (e.button == Button::R1 && !moving) {
                    app.audio().cursor.play();
                    switchTab(ProcessorSequence::Roms);
                } else if (e.button == Button::L2) {
                    app.audio().cursor.play();
                    moveSelection(-visibleRows());
                } else if (e.button == Button::R2) {
                    app.audio().cursor.play();
                    moveSelection(visibleRows());
                } else if (e.button == Button::Circle) {
                    app.audio().cancel.play();
                    if (moving) {
                        moving = false;
                    } else {
                        save();
                        menuVisible = false;
                    }
                }
                break;
            default:
                break;
            }
        }
    }
}
