//
// PscBios: the program.
//
#include "pscbios_app.h"
#include "screens/gui_pscbios_main.h"

#include <ableem/engine/log.h>

using namespace std;

//*******************************
// PscBios::PscBios
//*******************************
PscBios::PscBios(unique_ptr<ConsoleBackend> console) : AppBase("PSC Bios"), console_(std::move(console)) {
    // the tool's own translations, in the language the main GUI's config.ini names, over the main GUI's
    // (AppBase loaded those: the shared classic screens' strings live there)
    lang_.loadMore(Env::getPathToAppLangDir());
}

//*******************************
// PscBios::run
//*******************************
int PscBios::run() {
    gui_->loadAssets();
    gui_->hideMouseCursor();
    GuiPscBiosMain mainScreen(*gui_);
    mainScreen.show();
    gui_->finish();
    return 0;
}
