#include "gui_pad_daemon.h"
#include "gui/gui.h"
#include "core/services/environment.h"
#include "core/services/system.h"
#include "core/pad_list.h" // apps/abpad/src (abpad_core's include directory), next to src/code/core/

#ifndef _WIN32
#include <unistd.h>
#endif

using namespace std;

//*******************************
// GuiPadDaemon::daemonPath
//*******************************
// the package puts it at Autobleem/bin/abpad/abpadd, in the folder next to the launcher's own folder
// Autobleem/bin/autobleem/ (tools/make_psc_package.sh, rc/app_env.sh AB_PAD_DIR)
string GuiPadDaemon::daemonPath() {
#ifdef _WIN32
    return ""; // abpadd is not built for Windows
#else
    const string dir = Env::executableDir();
    if (dir.empty())
        return "";
    const string path = dir + "/../abpad/abpadd";
    return access(path.c_str(), X_OK) == 0 ? path : "";
#endif
}

//*******************************
// GuiPadDaemon::look / onButton
//*******************************
void GuiPadDaemon::look() {
    const string daemon = daemonPath();
    daemonFound = !daemon.empty();
    listing.clear();
    if (daemonFound)
        listing = System::execUnixCommandLines("'" + daemon + "' --list 2>/dev/null");
    listed = true;
}

bool GuiPadDaemon::onButton(ableem::Button button) {
    if (button != ableem::Button::Cross)
        return false;
    look();
    app.audio().cursor.play();
    refresh();
    return true;
}

//*******************************
// GuiPadDaemon::collect
//*******************************
vector<abgui::FactsSection> GuiPadDaemon::collect() {
    if (!listed)
        look();
    vector<abgui::FactsSection> sections;
    abgui::FactsSection pads;
    pads.title = _("Pads the daemon sees");
    if (!daemonFound) {
        pads.rows.push_back({_("Daemon"), _("Not in this build")});
    } else {
        // literal _() calls at each branch: tools/lang_tools.py's extract only sees a literal inside _(...)
        for (const abpad::PadListEntry &pad : abpad::parsePadList(listing)) {
            const string state = pad.connected ? _("connected") : _("not readable");
            string mapping = _("guessed by the daemon");
            if (pad.mapping == "database")
                mapping = _("from the controller database");
            const string driver = pad.driver == "?" ? _("unknown driver") : pad.driver;
            pads.rows.push_back({to_string(pad.index + 1) + ". " + pad.name, state + ", " + driver + ", " + mapping});
        }
        if (pads.rows.empty())
            pads.rows.push_back({_("Pads"), _("None")});
    }
    sections.push_back(pads);
    return sections;
}
