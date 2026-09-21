//
// GuiTextPage: static text in the classic layout.
//
#include "gui_text_page.h"
#include "../gui.h"

#include <algorithm>

using namespace std;

//*******************************
// GuiTextPage::render
//*******************************
void GuiTextPage::render() {
    gui->renderBackground();
    gui->renderTextBar();
    const ableem::Rect panel = gui->text().getOpscreenRectOfTheme();
    const ableem::Font &font = gui->assets().themeFont;
    const int lineHeight = font.lineHeight();
    int yoffset = gui->renderHeader(title);
    // each line wrapped to the panel; a line that wraps takes the rows it needs
    const int x = panel.x + 10;
    const int width = panel.w - 20;
    const ableem::Color color = TextRenderer::toColor(app.theme().classic().textColor, 255);
    // the lines from `firstLine` down, each wrapped, until the content rect is full
    const ableem::Rect content = gui->classicContent();
    const int bottom = content.y + content.h - 4;
    rowsThatFit = max(1, (bottom - yoffset) / lineHeight);
    firstLine = max(0, min(firstLine, static_cast<int>(lines.size()) - 1));
    int y = yoffset;
    size_t i = firstLine;
    for (; i < lines.size(); i++) {
        const string &line = lines[i];
        const int height = (line.empty() || centred) ? lineHeight : max(lineHeight, font.columnHeight(line, width));
        if (y + height > bottom)
            break;
        if (line.empty() || centred) {
            gui->text().renderTextLine(line, -y, 0, centred ? XALIGN_CENTER : XALIGN_LEFT);
        } else {
            gui->text().renderWrappedText(font, line, x, y, width, color);
        }
        y += height;
    }
    lastLineShown = static_cast<int>(i); // one past the last drawn
    gui->renderScrollMarkers(firstLine > 0, lastLineShown < static_cast<int>(lines.size()));
    string status = "|@O| " + _("Go back");
    if (firstLine > 0 || lastLineShown < static_cast<int>(lines.size()))
        status = "|@L1|/|@R1| " + _("Page") + "   " + status;
    gui->renderStatus(status);
    renderer.present();
}

//*******************************
// GuiTextPage::loop
//*******************************
void GuiTextPage::loop() {
    menuVisible = true;
    while (menuVisible) {
        render();
        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit)
                menuVisible = false;
            if ((e.type == Event::Type::ButtonDown && e.button == Button::Circle) ||
                (e.type == Event::Type::KeyDown && e.key == Key::Escape)) {
                app.audio().cancel.play();
                menuVisible = false;
            }
            // scrolling: a line with the d-pad, a page with L1/R1 (or the keyboard's Page Up/Down)
            const int last = static_cast<int>(lines.size());
            const bool more = lastLineShown < last;
            int move = 0;
            if (e.type == Event::Type::DpadDown) {
                if (gui->input().dpadDown())
                    move = 1;
                else if (gui->input().dpadUp())
                    move = -1;
            } else if (e.type == Event::Type::ButtonDown) {
                if (e.button == Button::R1)
                    move = rowsThatFit;
                else if (e.button == Button::L1)
                    move = -rowsThatFit;
            } else if (e.type == Event::Type::KeyDown) {
                if (e.key == Key::Down)
                    move = 1;
                else if (e.key == Key::Up)
                    move = -1;
                else if (e.key == Key::PageDown)
                    move = rowsThatFit;
                else if (e.key == Key::PageUp)
                    move = -rowsThatFit;
            }
            if (move > 0 && more) {
                app.audio().cursor.play();
                firstLine = min(firstLine + move, last - 1);
            } else if (move < 0 && firstLine > 0) {
                app.audio().cursor.play();
                firstLine = max(0, firstLine + move);
            }
        }
    }
}
