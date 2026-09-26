//
// Created by screemer on 2019-03-02.
//

#include "evoui_btn_guide.h"
#include <string>
#include "gui/gui.h"

#include <algorithm>

using namespace std;

//*******************************
// GuiBtnGuide::render
//*******************************
void GuiBtnGuide::render() {
    shared_ptr<Gui> gui(Gui::getInstance());
    gui->renderBackground();

    gui->renderTextBar();
    gui->renderHeader(_("Button Guide"));

    // three sections as heading bands; each row the buttons in a column at the left and what they do,
    // wrapped to the panel, beside it - a long translation takes two lines instead of running off
    PanelStyle style = gui->panelStyle();
    const ableem::Rect content = gui->classicContent();
    const ableem::Font &buttonFont = gui->assets().themeFonts[FONT_20_BOLD];
    const ableem::Font &textFont = gui->assets().themeFont;
    const int xButtons = content.x + PanelStyle::RowInset + 8;
    const int xText = xButtons + 250;
    const int textWidth = content.x + content.w - PanelStyle::RowInset - 8 - xText;
    const int rowHeight = max(30, textFont.lineHeight() + 2);
    int y = content.y;
    auto section = [&](const string &title) {
        style.label(renderer, ableem::Rect(content.x + 1, y, content.w - 2, rowHeight));
        gui->text().renderText_WithColor(buttonFont, title, xButtons, y + (rowHeight - buttonFont.lineHeight()) / 2,
                                         style.text, XALIGN_LEFT);
        y += rowHeight;
    };
    auto row = [&](const string &buttons, const string &what) {
        const int textHeight = max(rowHeight, gui->text().wrappedHeight(textFont, what, textWidth));
        style.buttons(*gui, buttons, xButtons, y + (rowHeight - 30) / 2);
        gui->text().renderWrappedText(textFont, what, xText, y + (rowHeight - textFont.lineHeight()) / 2, textWidth,
                                      style.text);
        y += textHeight;
    };

    section(_("Launcher"));
    row("|@X| / |@O|", _("Select or cancel highlighted option"));
    row("|@S|", _("Run using RetroArch"));
    row("|@R1| / |@L1|", _("Quick scroll to next letter"));
    row("|@Start|", _("Random Game"));
    row("|@Select|", _("Choose the games shown: PlayStation, RetroArch or Apps and the group"));
    row("|@L2| + |@R2|", _("System Menu (Re-Scan, RetroArch, Memory Cards, Power Off, ...)"));
    row("|@Up|", _("Quick menu (Re-Scan, Store, Network & Controllers, System menu)"));
    section(_("In Game"));
    row("|@Select| + |@Start|", _("Emulator config MENU"));
    row(_("RESET"), _("Quit emulation - back to AutoBleem"));
    section(_("In Retroarch Game"));
    row("|@Select| + |@Start|", _("Open Retroarch Menu"));
    row(_("POWER"), _("Exit to EvoUI"));

    gui->renderStatus("|@O| " + _("Back") + "|");
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
