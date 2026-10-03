//
// Created by screemer on 2019-03-02.
//

#include "evoui_btn_guide.h"
#include <string>
#include "gui/gui.h"
#include "core/services/environment.h"
#include <ableem/ui/keyboard_map.h>

#include <algorithm>

using namespace std;

//*******************************
// GuiBtnGuide::draw
//*******************************
void GuiBtnGuide::draw() {
    shared_ptr<Gui> gui(Gui::getInstance());
    gui->renderBackground();

    gui->renderTextBar();
    gui->renderHeader(_("Button guide"));

    // three sections as heading bands; each row the buttons in a column at the left and what they do,
    // wrapped to the panel, beside it - a long translation takes two lines instead of running off. With a
    // keyboard connected (or typed on) a second column has its keys (the PC-style map, keyboard_map.h)
    const bool keyboard = gui->input().keyboardPresent();
    const bool devHost = gui->platform().isDevHost();
    using ableem::KeyboardMap::keyFor;
    auto key = [](const string &name) { return name.empty() ? string() : "|@" + name + "|"; };
    PanelStyle style = gui->panelStyle();
    const ableem::Rect content = gui->classicContent();
    const ableem::Font &buttonFont = gui->assets().themeFonts[FONT_20_BOLD];
    const ableem::Font &textFont = gui->assets().themeFont;
    const int xButtons = content.x + PanelStyle::RowInset + 8;
    const int xKeys = xButtons + 250;
    const int xText = keyboard ? xKeys + 230 : xButtons + 250;
    const int textWidth = content.x + content.w - PanelStyle::RowInset - 8 - xText;
    const int rowHeight = max(30, textFont.lineHeight() + 2);
    int y = content.y;
    auto section = [&](const string &title, const string &keysTitle = "") {
        style.label(gui->uiContext(), ableem::Rect(content.x + 1, y, content.w - 2, rowHeight));
        gui->text().renderText_WithColor(buttonFont, title, xButtons, y + (rowHeight - buttonFont.lineHeight()) / 2,
                                         style.text, XALIGN_LEFT);
        if (keyboard && !keysTitle.empty())
            gui->text().renderText_WithColor(buttonFont, keysTitle, xKeys,
                                             y + (rowHeight - buttonFont.lineHeight()) / 2, style.secondary,
                                             XALIGN_LEFT);
        y += rowHeight;
    };
    auto row = [&](const string &buttons, const string &what, const string &keys = "") {
        const int textHeight = max(rowHeight, gui->text().wrappedHeight(textFont, what, textWidth));
        style.buttons(*gui, buttons, xButtons, y + (rowHeight - 30) / 2);
        if (keyboard && !keys.empty())
            style.buttons(*gui, keys, xKeys, y + (rowHeight - 30) / 2);
        gui->text().renderWrappedText(textFont, what, xText, y + (rowHeight - textFont.lineHeight()) / 2, textWidth,
                                      style.text);
        y += textHeight;
    };

    // the RetroArch lines only where the RetroArch program is installed (Env::retroArchInstalled)
    const bool retroArch = Env::retroArchInstalled();

    section(_("Launcher"), _("Keyboard"));
    row("|@X| / |@O|", _("Select or cancel highlighted option"),
        key(keyFor(Button::Cross, devHost)) + " / " + key(keyFor(Button::Circle, devHost)));
    if (retroArch)
        row("|@S|", _("Run using RetroArch"), key(keyFor(Button::Square, devHost)));
    row("|@R1/L1|", _("Quick scroll to next letter"),
        key(keyFor(Button::R1, devHost)) + " / " + key(keyFor(Button::L1, devHost)));
    row("|@Start|", _("Random game"), key(keyFor(Button::Start, devHost)));
    row("|@Select|",
        retroArch ? _("Choose the games shown: PlayStation, RetroArch or Apps and the group")
                  : _("Choose the games shown: PlayStation or Apps and the group"),
        key(keyFor(Button::Select, devHost)));
    row("|@L2+R2|",
        retroArch ? _("System menu (Re-scan, RetroArch, Memory Cards, Power off, ...)")
                  : _("System menu (Re-scan, Memory Cards, Power off, ...)"),
        key(ableem::KeyboardMap::systemMenuKey()));
    row("|@Up|", _("Quick menu (Re-scan, Store, Network & Controllers, System menu)"), key(_("Arrow up")));
    section(_("In game"));
    row("|@Select+Start|", _("Emulator config menu"));
    row(_("RESET"), _("Quit emulation - back to AutoBleem"));
    if (retroArch) {
        section(_("In RetroArch game"));
        row("|@Select+Start|", _("Open RetroArch menu"));
        row(_("POWER"), _("Exit to EvoUI"));
    }

    gui->renderStatus("|@O| " + _("Back") + "|");
}

//*******************************
// GuiBtnGuide::loop
//*******************************
void GuiBtnGuide::loop() {
    shared_ptr<Gui> gui(Gui::getInstance());
    menuVisible = true;
    while (menuVisible) {
        // nothing animates here: sleep until a press, and redraw 4 times a second meanwhile (the performance
        // overlay, the DebugDriver's shots)
        if (!gui->input().waitForEvent(250))
            render();
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
