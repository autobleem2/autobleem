//
// GuiBtPairing: pair a DualShock 4 or other standard Bluetooth gamepad. A compact list - a "Scan for
// controllers" action on top, then the discovered/known devices; Cross pairs (or a paired one is left as
// is), Triangle removes a pairing, Circle goes back. DualShock 3 is NOT here: it pairs over USB through
// the sixaxis plugin (see the DualShock 3 page). Needs a Bluetooth adapter (btUp()); without one the
// screen just says so.
//
#pragma once

#include "core/console_backend.h"
#include "gui/menus/gui_string_menu.h"

#include <vector>

//********************
// GuiBtPairing
//********************
class GuiBtPairing : public GuiStringMenu {
public:
    explicit GuiBtPairing(ableem::GuiBase &_gui) : GuiStringMenu(_gui) {}

    void init() override;
    std::string getTitle() override { return _("Bluetooth controller pairing"); }
    std::string getStatusLine() override;

    void doCross_Pressed() override;
    void doTriangle_Pressed() override;
    void doCircle_Pressed() override;

private:
    ConsoleBackend &backend();
    void rebuild();       // lines from hasAdapter_ + devices_
    void scan();          // busy scan, then merge the paired flags
    void refreshPaired(); // re-read which of devices_ are paired/connected

    bool hasAdapter_ = false;
    std::vector<BtDevice> devices_; // rows 1..N (row 0 is the Scan action)
};
