//
// GuiPscBiosMain: the hardware information screen PSC-Bios opens on - a GuiFactsPage of the time and
// timezone, the WiFi, ethernet and Bluetooth dongles with their addresses, the connected pads and whether
// each has a mapping. Select opens the WiFi settings, Square the gamepad menu, Triangle About, Circle
// closes the tool.
//
#pragma once

#include "gui/screens/gui_facts_page.h"
#include "core/network_status.h"

//********************
// GuiPscBiosMain
//********************
class GuiPscBiosMain : public GuiFactsPage {
public:
    explicit GuiPscBiosMain(ableem::GuiBase &_gui) : GuiFactsPage(_gui) { refreshInterval = 2000; }

    void init() override;

protected:
    std::string title() override { return _("Playstation Classic Hardware Information"); }
    std::vector<InfoSection> collect() override;
    std::string extraHints() override;
    bool onButton(ableem::Button button) override;

private:
    NetworkStatus status;
    bool kernel = false; // the AutoBleem kernel: the network rows, and the WiFi settings, need it

    void openNetworkMenu();
    void openGamepadMenu();
    void openAbout();
    static std::string clockText(); // the local time as asctime() prints it, without its newline
};
