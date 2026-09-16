#include "app.h"
#include "core/lang.h"
#include "core/util.h"

#include <iostream>
#include <utility>

using namespace std;

App *App::instance = nullptr;

//*******************************
// App::App
//*******************************
App::App(std::unique_ptr<ProcessRunner> runner) : runner_(std::move(runner)) {
    instance = this;

    gui_ = Gui::getInstance();
    audio_.reset(new AppAudio(gui_->audio()));
    scanner_ = Scanner::getInstance();

    gui_->platform().setPowerOffHandler([this]() {
        gui_->drawText(_("POWERING OFF... PLEASE WAIT"));
        Util::powerOff();
    });

    shared_ptr<Lang> lang(Lang::getInstance());
    lang->load(cfg_.inifile.values["language"]);

    gameQuery_.setRetroArchGames(&retroArch_);
}

//*******************************
// App::~App
//*******************************
App::~App() {
    instance = nullptr;
}

//*******************************
// App::get
//*******************************
App &App::get() {
    return *instance;
}
