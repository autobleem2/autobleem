#pragma once

#include "gui_stringMenu.h"
#include <vector>
#include <string>
#include <stdio.h>
#include <iostream>
#include "../../lang.h"


//********************
// GuiNetworkMenu
//********************
class GuiNetworkMenu : public GuiStringMenu {
public:
    GuiNetworkMenu(SDL_Shared<SDL_Renderer> _renderer) : GuiStringMenu(_renderer) {}
    void updateData();
    string ipaddr, tz;
    void init() override;
    void fill();
    void render() override ;

    void readCfgFile();
    void writeCfgFile();
    std::string initializeWifi();   // we currently can't tell if it was successful or not
    static std::string getIPAddress(string iface);

    virtual std::string getTitle() override { return "-=" + _("-=" + _("Edit Network WPA WiFi Credentials") + "=-") + "=-"; }
    virtual std::string getStatusLine() override;   // returns the status line at the bottom

    virtual bool skipSelectingThisLineWhenMovingByOne(int index) { return lines[index] == ""; }
    std::string ssid, password, driverMode;
    bool displayAsterisksInsteadOfPassword = false;   // this is a password.  display *****

    static std::string getCfgPath() { return "/etc/autobleem/ssid.cfg"; }
    static std::string getWpaSupplicantPath()
        { return "/etc/wpa_supplicant.conf"; }

    static std::string getSSID();
    static std::string getSSIDfrom_ssidcfg();
    static std::string getSSIDfrom_wpa_supplicant();

    void doCircle_Pressed() override;
    void doCross_Pressed() override;
    void doTriangle_Pressed() override;

    void doEnter() { doCross_Pressed(); }
    void doEscape() { doCircle_Pressed(); }
};

