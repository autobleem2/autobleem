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
    int y = yoffset;
    for (const string &line : lines) {
        if (line.empty() || centred) {
            gui->text().renderTextLine(line, -y, 0, centred ? XALIGN_CENTER : XALIGN_LEFT);
            y += lineHeight;
        } else {
            y += max(lineHeight, gui->text().renderWrappedText(font, line, x, y, width, color));
        }
    }
    gui->renderStatus("|@O| " + _("Go back"));
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
        }
    }
}
