//
// Created by screemer on 2019-12-01.
//

#include "launch_interceptor.h"
#include "../util.h"
#include "../util_time.h"
#include "../gui/gui.h"
#include "../app.h"
#include <iostream>
#include <unistd.h>
#include "../environment.h"

using namespace std;

bool LaunchInterceptor::execute(PsGamePtr &game, int resumepoint) {
    cout << "calling LaunchInterceptor::execute()" <<endl;

    shared_ptr<Gui> gui(Gui::getInstance());
    cout << "Starting External App" << endl;

    App::get().library().updateDatePlayed(*game, UtilTime::getCurrentTime());

    if (game->foreign) {
        cout << "FOREIGN MODE" << endl;
    }

    App::get().writeSelectionScript();

    string link = game->base + sep+ game->startup;

#ifdef AB_DEBUG_HOST
    Gui::splash("I'm sorry Dave.  I'm afraid I can't do that.");
#else
    Util::runAndWait(link, {});
    usleep(3 * 1000);
#endif

    return true;
}

