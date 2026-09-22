#include "app_base.h"
#include <ableem/ui/debug_driver.h>
#include <cstdlib>
#include "core/services/system.h"

using namespace std;

AppBase *AppBase::instance = nullptr;

//*******************************
// AppBase::AppBase
//*******************************
AppBase::AppBase(const string &windowTitle) {
    instance = this;
    Lang::setCurrent(&lang_);
    lang_.load(Env::getPathToLangDir(), cfg_.inifile.values["language"]);

    Gui::setWindowTitle(windowTitle);
    gui_ = Gui::getInstance();
    audio_ = make_unique<AppAudio>(gui_->audio(), cfg_, theme_);

    gui_->platform().setPowerOffHandler([this]() {
        gui_->drawText(_("POWERING OFF... PLEASE WAIT"));
        System::powerOff();
    });

    // AB_DEBUG_PORT=<port>: the DebugDriver takes pad and keyboard input over a socket and hands frames
    // back - tools/ab_drive.py drives the launcher and the console tools through it for automated looks
    // at the UI. In every build (the Windows product is driven the same way, from its own program folder
    // against the installed data tree); nothing listens unless the variable is set.
    if (const char *port = getenv("AB_DEBUG_PORT"))
        ableem::DebugDriver::start(*gui_, atoi(port));
}

//*******************************
// AppBase::~AppBase
//*******************************
AppBase::~AppBase() {
    Lang::setCurrent(nullptr);
    instance = nullptr;
}

//*******************************
// AppBase::get
//*******************************
AppBase &AppBase::get() {
    return *instance;
}
