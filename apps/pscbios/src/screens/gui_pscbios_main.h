//
// GuiPscBiosMain: the hardware information screen PSC-Bios opens on - the time and timezone, the WiFi,
// ethernet and Bluetooth dongles with their addresses, the connected pads and whether each has a mapping.
// Select opens the WiFi settings, Square the gamepad menu, Triangle About, Circle closes the tool.
//
#pragma once

#include "gui/gui_screen.h"
#include "core/network_status.h"

//********************
// GuiPscBiosMain
//********************
class GuiPscBiosMain : public GuiScreen {
public:
    using GuiScreen::GuiScreen;

    void init() override;
    void render() override;
    void loop() override;

    static const unsigned int RefreshInterval = 2000; // ms between rounds of abnet questions

private:
    NetworkStatus status;
    unsigned int lastRefresh = 0;
    bool kernel = false; // the AutoBleem kernel: the network rows, and the WiFi settings, need it

    void refresh();
    void openNetworkMenu();
    void openGamepadMenu();
    void openAbout();
    static std::string clockText(); // the local time as asctime() prints it, without its newline
};
