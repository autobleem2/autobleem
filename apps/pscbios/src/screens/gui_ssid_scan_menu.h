//
// GuiSsidScanMenu: the networks in range, one to pick. `newSsid` is the pick when not `cancelled`.
//
#pragma once

#include "gui/menus/gui_string_menu.h"

#include <string>

//********************
// GuiSsidScanMenu
//********************
class GuiSsidScanMenu : public GuiStringMenu {
public:
    explicit GuiSsidScanMenu(ableem::GuiBase &_gui) : GuiStringMenu(_gui) {}

    void init() override;
    std::string getTitle() override { return _("Select WiFi Network to connect"); }
    std::string getStatusLine() override;
    void doCircle_Pressed() override;
    void doCross_Pressed() override;

    std::string newSsid;
};

//********************
// GuiTimezoneSelect
//********************
// the system's timezone names, one to pick. `newTimezone` is the pick when not `cancelled`.
class GuiTimezoneSelect : public GuiStringMenu {
public:
    explicit GuiTimezoneSelect(ableem::GuiBase &_gui) : GuiStringMenu(_gui) {}

    void init() override;
    std::string getTitle() override { return _("Select Time Zone for Your Location"); }
    void doCircle_Pressed() override;
    void doCross_Pressed() override;

    std::string newTimezone;
};
