//
// AbFlashKit: the program.
//
#include "abflashkit_app.h"
#include "screens/gui_flashkit_main.h"
#include "gui/screens/gui_confirm.h"

using namespace std;

//*******************************
// AbFlashKit::AbFlashKit
//*******************************
AbFlashKit::AbFlashKit(unique_ptr<Flasher> flasher, unique_ptr<Led> led, string scratchDir)
    : AppBase("ABFlashKit"), flasher_(std::move(flasher)), led_(std::move(led)), scratchDir_(std::move(scratchDir)) {
    lang_.loadMore(Env::getPathToAppLangDir());
}

//*******************************
// the paths
//*******************************
string AbFlashKit::backupPath() const {
    return Env::getPathToUSBRoot() + sep + "LBOOT.EPB";
}

string AbFlashKit::validMarker() const {
    return Env::getPathToUSBRoot() + sep + "validlboot";
}

string AbFlashKit::kernelDir() const {
    return Env::getAppDir() + sep + "kernel";
}

//*******************************
// AbFlashKit::run
//*******************************
int AbFlashKit::run() {
    gui_->loadAssets();
    gui_->hideMouseCursor();
    // the warning first: this tool writes to the console's flash
    GuiConfirm warning(*gui_);
    warning.title = "ABFlashKit";
    warning.label = _("This tool writes to the console's flash memory. A flash that is interrupted - the power "
                      "cut, the stick pulled - can leave the console unable to start, and installing a custom "
                      "kernel voids its warranty. Keep the console powered and the stick in until it reboots by "
                      "itself. Continue at your own risk.");
    warning.confirmLabel = _("I understand");
    warning.cancelLabel = _("Quit");
    warning.show();
    if (!warning.result) {
        gui_->finish();
        return 0;
    }
    GuiFlashKitMain mainScreen(*gui_);
    mainScreen.show();
    gui_->finish();
    return 0;
}
