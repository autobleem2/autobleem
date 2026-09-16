#include "app.h"
#include "core/services/system.h"

#include <iostream>
#include <utility>

using namespace std;

App *App::instance = nullptr;

//*******************************
// App::App
//*******************************
App::App(std::unique_ptr<ProcessRunner> runner) : runner_(std::move(runner)) {
    instance = this;
    Lang::setCurrent(&lang_);
    lang_.load(Env::getPathToLangDir(), cfg_.inifile.values["language"]);

    gui_ = Gui::getInstance();
    audio_.reset(new AppAudio(gui_->audio(), cfg_, theme_));

    gui_->platform().setPowerOffHandler([this]() {
        gui_->drawText(_("POWERING OFF... PLEASE WAIT"));
        System::powerOff();
    });

    gameQuery_.setRetroArchGames(&retroArch_);
}

//*******************************
// App::~App
//*******************************
App::~App() {
    Lang::setCurrent(nullptr);
    instance = nullptr;
}

//*******************************
// App::get
//*******************************
App &App::get() {
    return *instance;
}
