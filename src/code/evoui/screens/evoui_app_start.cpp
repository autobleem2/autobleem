//
// GuiAppStart: the screen before an App runs - its README on the left, the App's icon and facts in the
// detail pane on the right, Cross starts it. In the shared panel look since 2026-09-21 (it used to be
// hand-drawn lines over the background).
//
#include "evoui_app_start.h"
#include "../../gui/gui.h"
#include "../../core/services/environment.h"

#include <algorithm>
#include <fstream>
#include <sstream>

using namespace std;

//*******************************
// GuiAppStart::init
//*******************************
void GuiAppStart::init() {
    font = gui->assets().themeFont;
    appName = game->title;
    lines.clear();
    readmeLoaded = false;
    firstLine = 0;
    if (DirEntry::exists(game->readme_path)) {
        ifstream t(game->readme_path);
        string line;
        while (getline(t, line)) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            lines.push_back(line);
        }
        readmeLoaded = true;
    }
    pane.cover = DirEntry::exists(game->image_path)
                     ? ableem::Texture::loadFile(renderer, game->image_path)
                     : ableem::Texture::loadFile(renderer, Env::getWorkingPath() + sep + "evoimg/app-cover.png");
    pane.facts.clear();
    if (!game->publisher.empty())
        pane.facts.emplace_back(_("Published by:"), game->publisher);
    const string folder = DirEntry::getFileNameFromPath(DirEntry::removeSeparatorFromEndOfPath(game->folder));
    if (!folder.empty())
        pane.facts.emplace_back(_("Folder:"), folder);
    if (!game->startup.empty())
        pane.facts.emplace_back(_("Startup"), DirEntry::getFileNameFromPath(game->startup));
}

//*******************************
// GuiAppStart::render
//*******************************
void GuiAppStart::render() {
    gui->renderBackground();
    gui->renderTextBar();
    int yoffset = gui->renderHeader(gui->text().elide(gui->assets().themeFonts[FONT_28_BOLD], appName,
                                                      gui->classicPanel().w - 2 * PanelStyle::RowInset));
    pane.render(*gui);

    // the README on the left, each line wrapped to the rows' width; Up/Down a line, L2/R2 a page
    const ableem::Rect content = gui->classicContent();
    const int x = content.x + PanelStyle::RowInset + 8;
    const int width = GameDetailPane::rowsRight(*gui) - x;
    const int bottom = content.y + content.h - 4;
    const int lineHeight = font.lineHeight();
    rowsThatFit = max(1, (bottom - yoffset) / lineHeight);
    const ableem::Color color = gui->panelStyle().text;
    if (!readmeLoaded) {
        gui->text().renderText_WithColor(font, _("ReadMe file not found"), x, yoffset, gui->panelStyle().secondary,
                                         XALIGN_LEFT);
    } else {
        firstLine = max(0, min(firstLine, max(0, static_cast<int>(lines.size()) - 1)));
        int y = yoffset;
        size_t i = firstLine;
        for (; i < lines.size(); i++) {
            const string &line = lines[i];
            const int height =
                line.empty() ? lineHeight : max(lineHeight, gui->text().wrappedHeight(font, line, width));
            if (y + height > bottom)
                break;
            if (!line.empty())
                gui->text().renderWrappedText(font, line, x, y, width, color);
            y += height;
        }
        lastLineShown = static_cast<int>(i);
        gui->renderScrollMarkers(firstLine > 0, lastLineShown < static_cast<int>(lines.size()));
    }

    string status = "|@X| " + _("OK") + "  |@O| " + _("Cancel") + "|";
    if (readmeLoaded && (firstLine > 0 || lastLineShown < static_cast<int>(lines.size())))
        status += "  |@L2|/|@R2| " + _("Page");
    gui->renderStatus(status);
    renderer.present();
}

//*******************************
// GuiAppStart::loop
//*******************************
void GuiAppStart::loop() {
    bool visible = true;
    while (visible) {
        Event e;
        render();
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit) {
                result = false;
                visible = false;
            }
            const int last = static_cast<int>(lines.size());
            int move = 0;
            switch (e.type) {
            case Event::Type::ButtonDown:
                if (e.button == Button::Cross) {
                    app.audio().cursor.play();
                    result = true;
                    visible = false;
                } else if (e.button == Button::Circle) {
                    app.audio().cancel.play();
                    result = false;
                    visible = false;
                } else if (e.button == Button::R2) {
                    move = rowsThatFit;
                } else if (e.button == Button::L2) {
                    move = -rowsThatFit;
                }
                break;
            case Event::Type::DpadDown:
                if (gui->input().dpadDown())
                    move = 1;
                else if (gui->input().dpadUp())
                    move = -1;
                break;
            default:
                break;
            }
            if (move > 0 && lastLineShown < last) {
                app.audio().cursor.play();
                firstLine = min(firstLine + move, last - 1);
            } else if (move < 0 && firstLine > 0) {
                app.audio().cursor.play();
                firstLine = max(0, firstLine + move);
            }
        }
    }
}
