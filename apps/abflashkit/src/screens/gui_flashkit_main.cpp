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

namespace {
enum Action { Flash = 0, FullBackup, Restore };
}

//*******************************
// GuiFlashKitMain::init
//*******************************
void GuiFlashKitMain::init() {
    menu.title = "ABFlashKit";
    menu.subtitle = Version::VERSION;
    menu.circleLabel = _("Quit");
    menu.items = {
        {_("Flash Kernel"), _("Back the console up to the stick, install the AutoBleem kernel and reboot")},
        {_("Full backup"), _("All four partitions to LBOOT.EPB on the stick, for a restore later")},
        {_("Restore Mode"), _("Reboot into Sony's recovery, which restores the console from LBOOT.EPB")},
    };
}

//*******************************
// GuiFlashKitMain::render
//*******************************
void GuiFlashKitMain::render() {
    menu.render();
}

//*******************************
// the FlashUi: every step as the spinner's message over the menu, a confirm dialog, a pause
//*******************************
void GuiFlashKitMain::status(const string &text) {
    PLOG_INFO << text;
    lastStatus = text;
    if (busy)
        gui->endBusy();
    gui->beginBusy(text, [this]() { render(); });
    busy = true;
}

bool GuiFlashKitMain::confirm(const string &question) {
    if (busy) {
        gui->endBusy();
        busy = false;
    }
    GuiConfirm dialog(*gui);
    dialog.label = question;
    dialog.show();
    if (!lastStatus.empty())
        status(lastStatus); // the spinner comes back with the step in progress
    return dialog.result;
}

void GuiFlashKitMain::wait(int ms) {
    const unsigned int until = gui->platform().ticks() + ms;
    while (gui->platform().ticks() < until) {
        if (busy)
            gui->busyTick();
        gui->platform().delay(20);
    }
}

//*******************************
// GuiFlashKitMain::run
//*******************************
void GuiFlashKitMain::run(FlashKitActions::Outcome (FlashKitActions::*action)()) {
    AbFlashKit &tool = AbFlashKit::get();
    FlashKitActions actions(tool.flasher(), tool.led(), *this, tool.backupPath(), tool.kernelDir(), tool.scratchDir(),
                            tool.validMarker());
    lastStatus.clear();
    FlashKitActions::Outcome outcome = (actions.*action)();
    PLOG_INFO << "Action " << FlashKitActions::name(outcome);
    // the last status stays readable for a moment before the menu is back
    wait(1500);
    if (busy) {
        gui->endBusy();
        busy = false;
    }
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
        menu.show();
        switch (menu.result) {
        case Flash:
            run(&FlashKitActions::flash);
            break;
        case FullBackup:
            run(&FlashKitActions::fullBackup);
            break;
        case Restore:
            run(&FlashKitActions::restore);
            break;
        default:
            menuVisible = false; // Circle, or the window closed
            break;
        }
    }
    AbFlashKit::get().led().setMode(LedMode::Off);
}
