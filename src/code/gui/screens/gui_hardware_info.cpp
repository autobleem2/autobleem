//
// GuiHardwareInfo: the built-in Hardware Information screen.
//
#include "gui_hardware_info.h"
#include "../gui.h"

#include <algorithm>
#include <cstdio>

using namespace std;

//*******************************
// GuiHardwareInfo::init
//*******************************
void GuiHardwareInfo::init() {
    font = gui->assets().themeFont;
    firstVisible = 0;
    refresh();
}

//*******************************
// GuiHardwareInfo::refresh
//*******************************
void GuiHardwareInfo::refresh() {
    lastRefresh = gui->platform().ticks();
    lines.clear();
    vector<InfoSection> sections = systemInfo.collect();
    sections.push_back(displayAndInput());
    for (const InfoSection &section : sections) {
        Line heading;
        heading.heading = true;
        heading.label = section.title;
        lines.push_back(heading);
        for (const InfoRow &row : section.rows) {
            Line line;
            line.label = row.label;
            line.value = row.value;
            lines.push_back(line);
        }
    }
    firstVisible = min(firstVisible, maxFirstVisible());
}

//*******************************
// GuiHardwareInfo::displayAndInput
//*******************************
// what SystemInfoService cannot know: the window it is drawn in and the pads it is driven with
InfoSection GuiHardwareInfo::displayAndInput() {
    InfoSection section{_("Display and input"), {}};
    auto add = [&](const string &label, const string &value) {
        if (!value.empty())
            section.rows.push_back({label, value});
    };
    ableem::Platform &platform = gui->platform();
    string rendererName = renderer.driverName();
    if (platform.multisampleSamples() > 0)
        rendererName += ", " + to_string(platform.multisampleSamples()) + "x MSAA";
    add(_("Renderer"), rendererName);
    add(_("Video driver"), platform.videoDriverName());
    add(_("Display mode"), platform.displayModeString());
    // the canvas every screen draws on, and how many output pixels one of its pixels is (see Gui::outputScale)
    string canvas = to_string(renderer.width()) + "x" + to_string(renderer.height());
    if (renderer.outputScale() != 1.0f) {
        char scale[16];
        snprintf(scale, sizeof(scale), " x%.2g", renderer.outputScale());
        canvas += scale;
    }
    add(_("Canvas"), canvas);
    add(_("Audio driver"), gui->audio().driverName());
    add("SDL", platform.linkedVersion());
    vector<ableem::PadInfo> pads = gui->input().pads();
    if (pads.empty()) {
        add(_("Controllers"), platform.isDevHost() ? _("Keyboard") : _("None"));
    } else {
        for (size_t i = 0; i < pads.size(); i++)
            add(_("Controller") + " " + to_string(i + 1), pads[i].name);
    }
    return section;
}

//*******************************
// GuiHardwareInfo::maxFirstVisible / scrollBy
//*******************************
int GuiHardwareInfo::maxFirstVisible() const {
    return max(0, static_cast<int>(lines.size()) - rowsThatFit);
}

void GuiHardwareInfo::scrollBy(int rows) {
    int target = max(0, min(maxFirstVisible(), firstVisible + rows));
    if (target == firstVisible) {
        app.audio().cancel.play();
        return;
    }
    firstVisible = target;
    app.audio().cursor.play();
}

//*******************************
// GuiHardwareInfo::render
//*******************************
void GuiHardwareInfo::render() {
    renderer.clear();
    gui->renderBackground();
    gui->renderTextBar();
    int yoffset = gui->renderHeader(_("Hardware Information"));

    // the rows go from below the header to the bottom of the panel, as in the Options menu
    const ableem::Rect panel = gui->text().getOpscreenRectOfTheme();
    const int fontHeight = font.lineHeight();
    const int firstLineY = yoffset;
    const int lastLineY = panel.y + panel.h - fontHeight - 4;
    rowsThatFit = max(1, (lastLineY - firstLineY) / fontHeight + 1);
    firstVisible = min(firstVisible, maxFirstVisible());

    // the values start a third of the way across; a long one (a path) is cut to what fits
    const int valueX = panel.w * 35 / 100;
    const int valueWidth = panel.w - valueX - 30;
    const int count = static_cast<int>(lines.size());
    for (int i = firstVisible, row = 0; i < count && row < rowsThatFit; i++, row++) {
        const int y = firstLineY + fontHeight * row;
        const Line &line = lines[i];
        if (line.heading) {
            gui->text().renderLabelBox(0, y);
            gui->text().renderTextLine(line.label, -y, 0, XALIGN_CENTER, 0, font);
        } else {
            gui->text().renderTextLineToColumns(line.label, gui->text().elide(font, line.value, valueWidth), 10, valueX,
                                                -y, 0, font);
        }
    }

    string status = "|@O| " + _("Go back");
    if (count > rowsThatFit) {
        const int page = firstVisible / rowsThatFit + 1;
        const int pages = (count + rowsThatFit - 1) / rowsThatFit;
        status += "   |@L1| |@R1| " + _("Page") + " " + to_string(page) + "/" + to_string(pages);
    }
    gui->renderStatus(status);
    renderer.present();
}

//*******************************
// GuiHardwareInfo::loop
//*******************************
void GuiHardwareInfo::loop() {
    menuVisible = true;
    while (menuVisible) {
        if (gui->platform().ticks() - lastRefresh >= RefreshInterval)
            refresh();
        render();

        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit) {
                menuVisible = false;
            }
            switch (e.type) {
            case Event::Type::DpadDown:
            case Event::Type::DpadUp:
                if (gui->input().dpadUp())
                    scrollBy(-1);
                else if (gui->input().dpadDown())
                    scrollBy(1);
                else if (gui->input().dpadLeft())
                    scrollBy(-rowsThatFit);
                else if (gui->input().dpadRight())
                    scrollBy(rowsThatFit);
                break;
            case Event::Type::ButtonDown:
                if (e.button == Button::L1)
                    scrollBy(-rowsThatFit);
                else if (e.button == Button::R1)
                    scrollBy(rowsThatFit);
                else if (e.button == Button::Circle) {
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
