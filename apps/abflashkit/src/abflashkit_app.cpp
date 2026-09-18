//
// AbFlashKit: the program.
//
#include "abflashkit_app.h"
#include "screens/gui_flashkit_main.h"

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
    gui_->drawText(_("Welcome to ABFlashKit"));
    gui_->platform().delay(2000);
    GuiFlashKitMain mainScreen(*gui_);
    mainScreen.show();
    gui_->finish();
    return 0;
}
