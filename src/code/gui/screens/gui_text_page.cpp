//
// GuiTextPage: static text in the classic layout.
//
#include "gui_text_page.h"
#include "../gui.h"

using namespace std;

//*******************************
// GuiTextPage::render
//*******************************
void GuiTextPage::render() {
    gui->renderBackground();
    gui->renderTextBar();
    int yoffset = gui->renderLogo(true);
    gui->text().renderTextLine("-=" + title + "=-", 0, yoffset, XALIGN_CENTER);
    int row = 2;
    for (const string &line : lines)
        gui->text().renderTextLine(line, row++, yoffset, centred ? XALIGN_CENTER : XALIGN_LEFT);
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
