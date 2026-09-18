#include "app.h"
#include "core/services/system.h"

#include <iostream>
#include <memory>
#include <utility>

using namespace std;

//*******************************
// App::App
//*******************************
App::App(std::unique_ptr<ProcessRunner> runner) : AppBase("AutoBleem"), runner_(std::move(runner)) {
    gameQuery_.setRetroArchGames(&retroArch_);
    gameQuery_.setLightguns(&lightguns_);
}

//*******************************
// App::~App
//*******************************
App::~App() = default;
