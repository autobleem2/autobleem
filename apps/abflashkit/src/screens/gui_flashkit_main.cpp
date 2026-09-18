//
// GuiFlashKitMain: the one screen.
//
#include "gui_flashkit_main.h"
#include "../abflashkit_app.h"
#include "core/version.h"
#include "gui/gui.h"
#include "gui/screens/gui_confirm.h"

#include <ableem/engine/log.h>

using namespace std;

//*******************************
// GuiFlashKitMain::render
//*******************************
void GuiFlashKitMain::render() {
    gui->drawText(string("ABFlashKit ") + Version::VERSION + "  |@X| " + _("Flash Kernel") + "  |@S| " +
                  _("Full backup") + "  |@T| " + _("Restore Mode") + "  |@O|  " + _("Quit") + "|");
}

//*******************************
// the FlashUi: a status line the way the 2020 tool showed every step, a confirm dialog, a pause
//*******************************
void GuiFlashKitMain::status(const string &text) {
    PLOG_INFO << text;
    lastStatus = text;
    gui->drawText(text);
}

bool GuiFlashKitMain::confirm(const string &question) {
    GuiConfirm dialog(*gui);
    dialog.label = question;
    dialog.show();
    return dialog.result;
}

void GuiFlashKitMain::wait(int ms) {
    gui->platform().delay(ms);
}

//*******************************
// GuiFlashKitMain::run
//*******************************
void GuiFlashKitMain::run(FlashKitActions::Outcome (FlashKitActions::*action)()) {
    app.audio().cursor.play();
    AbFlashKit &tool = AbFlashKit::get();
    FlashKitActions actions(tool.flasher(), tool.led(), *this, tool.backupPath(), tool.kernelDir(), tool.scratchDir(),
                            tool.validMarker());
    FlashKitActions::Outcome outcome = (actions.*action)();
    PLOG_INFO << "Action " << FlashKitActions::name(outcome);
    // a reboot has been issued on the console; on a dev host there is nothing to reboot, the tool closes
    if (outcome == FlashKitActions::Outcome::Rebooting)
        menuVisible = false;
    gui->input().flushEvents();
}

//*******************************
// GuiFlashKitMain::loop
//*******************************
void GuiFlashKitMain::loop() {
    menuVisible = true;
    AbFlashKit::get().led().setMode(LedMode::Green);
    while (menuVisible) {
        render();
        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit)
                menuVisible = false;
            if (e.type != Event::Type::ButtonDown)
                continue;
            switch (e.button) {
            case Button::Cross:
                run(&FlashKitActions::flash);
                break;
            case Button::Square:
                run(&FlashKitActions::fullBackup);
                break;
            case Button::Triangle:
                run(&FlashKitActions::restore);
                break;
            case Button::Circle:
                app.audio().cancel.play();
                gui->drawText(_("Exiting..."));
                menuVisible = false;
                break;
            default:
                break;
            }
        }
    }
    AbFlashKit::get().led().setMode(LedMode::Off);
}
