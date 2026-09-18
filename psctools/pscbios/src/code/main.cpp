/* 
 * File:   main.cpp
 * Author: screemer
 *
 * Created on 11 Dec 2018, 20:37
 */

#include <iostream>

#include "gui/gui.h"
#include "gui/menus/gui_networkMenu.h"
#include "main.h"
#include "util.h"
#include <unistd.h>
#include "lang.h"
#include "environment.h"
#include "engine/gcdbprocessor.h"


using namespace std;


// these are defined in environment.h and are meant to not be modified once they are initialized here.
extern bool private_singleArgPassed;
extern string private_pathToUSBDrive;

//*******************************
// copyGameFilesInGamesDirToSubDirs
//*******************************
// Search for games with supported extension and move to sub-dir
// returns true is any files moved into sub-dirs



//*******************************
// main
//*******************************
int main(int argc, char *argv[]) {
    SDL_Init(SDL_INIT_VIDEO);
    SDL_InitSubSystem(SDL_INIT_AUDIO);
    SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
    Env::autobleemKernel = DirEntry::exists("/autobleem");
    cout << "AutoBleem kernel:" << Env::autobleemKernel << endl;
    shared_ptr<Lang> lang(Lang::getInstance());

#if defined(__x86_64__) || defined(_M_X64)
    if (argc >= 1+1) {
        private_pathToUSBDrive = argv[1];
    }
#else
        private_pathToUSBDrive = "/media";
#endif


    // now that Environment is setup, routines that need the paths can be called
    shared_ptr<Gui> gui(Gui::getInstance());
    gui->mapper.init();
    gui->mapper.probePads();
    lang->load(gui->cfg.inifile.values["language"]);
    gui->display();

    while (true) {

        gui->menuSelection();

    }

}
