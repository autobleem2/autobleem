//
// UpdateRomsApp: the program.
//
#include "updateroms_app.h"
#include "screens/gui_update_roms.h"

#include <ableem/engine/log.h>

using namespace std;

//*******************************
// UpdateRomsApp::UpdateRomsApp
//*******************************
UpdateRomsApp::UpdateRomsApp(const UpdateRomsJob::Setup &setup) : AppBase("AutoBleem - Update ROMs"), setup_(setup) {
    lang_.loadMore(Env::getPathToAppLangDir());
}

//*******************************
// UpdateRomsApp::run
//*******************************
int UpdateRomsApp::run() {
    gui_->loadAssets(false); // the theme's look, not its music: this is a utility window on a PC
    GuiUpdateRoms screen(*gui_);
    screen.setup = setup_;
    screen.show();
    gui_->finish();
    return 0;
}
