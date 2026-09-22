//
// GuiBtPairing: the Bluetooth controller pairing screen.
//
#include "gui_bt_pairing.h"
#include "gui/gui.h"
#include "gui/screens/gui_confirm.h"
#include "pscbios_app.h"

using namespace std;

//*******************************
// helpers
//*******************************
namespace {
// mark which of `list` are paired/connected from `paired`, and append paired devices the scan did not see
void mergePaired(vector<BtDevice> &list, const vector<BtDevice> &paired) {
    for (BtDevice &d : list) {
        d.paired = false;
        d.connected = false;
        for (const BtDevice &p : paired)
            if (p.mac == d.mac) {
                d.paired = true;
                d.connected = p.connected;
            }
    }
    for (const BtDevice &p : paired) {
        bool seen = false;
        for (const BtDevice &d : list)
            if (d.mac == p.mac)
                seen = true;
        if (!seen)
            list.push_back(p);
    }
}
} // namespace

ConsoleBackend &GuiBtPairing::backend() {
    return PscBios::get().console();
}

//*******************************
// GuiBtPairing::init
//*******************************
void GuiBtPairing::init() {
    GuiMenuBase::init();
    hasAdapter_ = backend().btUp();
    devices_ = hasAdapter_ ? backend().btPairedDevices() : vector<BtDevice>{};
    rebuild();
}

//*******************************
// GuiBtPairing::rebuild
//*******************************
void GuiBtPairing::rebuild() {
    lines.clear();
    if (!hasAdapter_) {
        lines.push_back(_("No Bluetooth adapter found."));
        lines.push_back(_("Plug a USB Bluetooth dongle into the console, then open this screen again."));
    } else {
        lines.push_back(_("Scan for controllers"));
        for (const BtDevice &d : devices_) {
            string status = d.connected ? _("connected") : d.paired ? _("paired") : _("new");
            lines.push_back(d.name + "   [" + status + "]   " + d.mac);
        }
    }
    if (selected >= getVerticalSize())
        selected = 0;
    computePagePosition();
}

//*******************************
// GuiBtPairing::refreshPaired
//*******************************
void GuiBtPairing::refreshPaired() {
    mergePaired(devices_, backend().btPairedDevices());
    rebuild();
}

//*******************************
// GuiBtPairing::scan
//*******************************
void GuiBtPairing::scan() {
    app.audio().cursor.play();
    gui->drawText(_("Scanning for controllers..."), _("Put your controller in pairing mode"));
    devices_ = backend().btScan();
    mergePaired(devices_, backend().btPairedDevices());
    rebuild();
}

//*******************************
// GuiBtPairing::doCross_Pressed
//*******************************
void GuiBtPairing::doCross_Pressed() {
    if (!hasAdapter_)
        return; // the two info rows do nothing
    if (selected == 0) {
        scan();
        return;
    }
    int idx = selected - 1;
    if (idx < 0 || idx >= static_cast<int>(devices_.size()))
        return;
    BtDevice d = devices_[idx];
    if (d.paired) {
        gui->drawText(_("Already paired") + ": " + d.name); // Triangle removes it
        return;
    }
    app.audio().cursor.play();
    gui->drawText(_("Pairing") + " " + d.name + "...", _("Keep the controller in pairing mode"));
    backend().btPair(d.mac);
    refreshPaired(); // the row now shows [paired]/[connected], or stays [new] on failure
}

//*******************************
// GuiBtPairing::doTriangle_Pressed
//*******************************
void GuiBtPairing::doTriangle_Pressed() {
    if (!hasAdapter_ || selected == 0)
        return;
    int idx = selected - 1;
    if (idx < 0 || idx >= static_cast<int>(devices_.size()))
        return;
    BtDevice d = devices_[idx];
    if (!d.paired)
        return;
    GuiConfirm confirm(*gui);
    confirm.label = _("Remove pairing for") + " " + d.name + "?";
    confirm.show();
    if (!confirm.result)
        return;
    gui->drawText(_("Removing") + " " + d.name + "...");
    backend().btRemove(d.mac);
    refreshPaired();
}

//*******************************
// GuiBtPairing::doCircle_Pressed
//*******************************
void GuiBtPairing::doCircle_Pressed() {
    app.audio().cancel.play();
    menuVisible = false;
}

//*******************************
// GuiBtPairing::getStatusLine
//*******************************
string GuiBtPairing::getStatusLine() {
    if (!hasAdapter_)
        return "|@O| " + _("Back");
    string act = selected == 0 ? _("Scan") : _("Pair");
    return "|@X| " + act + "   |@T| " + _("Remove") + "   |@O| " + _("Back");
}
