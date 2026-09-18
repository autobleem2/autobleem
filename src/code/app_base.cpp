#include "app_base.h"
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
