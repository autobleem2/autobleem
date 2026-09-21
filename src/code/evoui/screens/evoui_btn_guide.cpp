//
// Created by screemer on 2019-03-02.
//

#include "evoui_btn_guide.h"
#include <string>
#include "../../gui/gui.h"

using namespace std;

//*******************************
// GuiBtnGuide::render
//*******************************
void GuiBtnGuide::render() {
    shared_ptr<Gui> gui(Gui::getInstance());
    gui->renderBackground();

    gui->renderTextBar();
    int yoffset = gui->renderHeader(_("Button Guide"));

    int xLeft = 300;
    int xRight = 520;
    int line = 0;
    auto font = gui->assets().themeFonts[FONT_20_BOLD];

    auto renderTextLineToColumns = [&](const string &textLeft, const string &textRight) {
        gui->text().renderTextLineToColumns(textLeft, textRight, xLeft, xRight, line++, yoffset, font);
    };

    renderTextLineToColumns("|@X| / |@O|", _("Select or cancel highlighted option"));
    renderTextLineToColumns("|@S|", _("Run using RetroArch"));
    renderTextLineToColumns("|@R1| / |@L1|", _("Quick scroll to next letter"));
    renderTextLineToColumns("|@Start|", _("Random Game"));
    renderTextLineToColumns("|@Select|", _("Next Game Platform"));
    renderTextLineToColumns("|@L2| + |@Select|", _("Change USB Games Sub-Directory"));
    renderTextLineToColumns("|@L2| + |@Select|", _("Change RetroArch Playlist"));
    line++;
    renderTextLineToColumns("", _("In Game"));
    renderTextLineToColumns("|@Select| + |@Start|", _("Emulator config MENU"));
    renderTextLineToColumns(_("RESET"), _("Quit emulation - back to AutoBleem"));
    line++;
    renderTextLineToColumns("", _("In Retroarch Game"));
    renderTextLineToColumns("|@Select| + |@Start|", _("Open Retroarch Menu"));
    renderTextLineToColumns(_("POWER"), _("Exit to EvoUI"));
    line++;
    renderTextLineToColumns("|@L2| + |@R2|", _("System Menu (Re-Scan, RetroArch, Memory Cards, Power Off, ...)"));

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
