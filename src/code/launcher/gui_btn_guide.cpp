//
// Created by screemer on 2019-03-02.
//

#include "gui_btn_guide.h"
#include <string>
#include "../gui/gui.h"
#include "../core/lang.h"
#include "../engine/scanner.h"

using namespace std;

//*******************************
// GuiBtnGuide::render
//*******************************
void GuiBtnGuide::render() {
    shared_ptr<Gui> gui(Gui::getInstance());
    gui->renderBackground();

    gui->renderTextBar();
    // -50 is because the "button guide" line is too low and the lines go below the bottom of the rectangle
    int yoffset = gui->renderLogo(true) - 50;

    int xLeft = 300;
    int xRight = 520;
    int line = 0;
    auto font = gui->assets().sonyFonts[FONT_20_BOLD];

    auto renderTextLineToColumns = [&] (const string &textLeft, const string &textRight) {
        gui->text().renderTextLineToColumns(textLeft, textRight, xLeft, xRight, line++, yoffset, font);
    };

    renderTextLineToColumns("",                          "-=" + _("Button Guide") + "=-");
    line++;
    renderTextLineToColumns("|@X| / |@O|",               _("Select or cancel highlighted option"));
    renderTextLineToColumns("|@S|",                      _("Run using RetroBoot"));
    renderTextLineToColumns("|@R1| / |@L1|",             _("Quick scroll to next letter"));
    renderTextLineToColumns("|@Start|",                  _("Random Game"));
    renderTextLineToColumns("|@Select|",                 _("Next Game Platform"));
    renderTextLineToColumns("|@L2| + |@Select|",         _("Change USB Games Sub-Directory"));
    renderTextLineToColumns("|@L2| + |@Select|",         _("Change RetroBoot System"));
    line++;
    renderTextLineToColumns("",                          "-=" + _("In Game") + "=-");
    renderTextLineToColumns("|@Select| + |@Start|",          _("Emulator config MENU"));
    renderTextLineToColumns(_("RESET"),                  _("Quit emulation - back to AutoBleem"));
    line++;
    renderTextLineToColumns("",                          "-=" + _("In Retroarch Game") + "=-");
    renderTextLineToColumns("|@Select| + |@Start|",      _("Open Retroarch Menu"));
    renderTextLineToColumns(_("POWER"),                  _("Exit to EvoUI"));
    line++;
    renderTextLineToColumns("|@L2| + |@R2|",             _("In Boot Menu: Safe Power Off The Console"));

    gui->renderStatus("|@O| " + _("Go back") + "|");
    renderer.present();
}

//*******************************
// GuiBtnGuide::loop
//*******************************
void GuiBtnGuide::loop() {
    shared_ptr<Gui> gui(Gui::getInstance());
    menuVisible = true;
    while (menuVisible) {
        Event e;
        while (gui->input().poll(e)) {
            // this is for pc Only
            if (e.type == Event::Type::Quit) {
                menuVisible = false;
            }
            switch (e.type) {
                case Event::Type::ButtonUp:
                    if (e.button == Button::Circle) {
                        app.audio().cancel.play();
                        menuVisible = false;
                    };
                    break;
                default:
                    break;
            }
        }
    }
}
