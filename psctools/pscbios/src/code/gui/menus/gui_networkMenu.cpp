#include "gui_networkMenu.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include "../gui.h"
#include "../gui_confirm.h"
#include "../gui_keyboard.h"
#include "../../lang.h"
#include "gui_ssidScanMenu.h"
#include "gui_timezoneSelect.h"
#include <cassert> // assert
#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <regex>
#include <fstream>

#ifndef __APPLE__
#include <wait.h>
#endif

using namespace std;



enum {
    Text = 0, SSID, PW, DriverMode, Blank1, WriteFile, Blank2, InitNetwork, IpAddress, TimeZone
};

void GuiNetworkMenu::render()
{
    if (frame > 75) {
        updateData();
        frame = 0;
    }
    fill();
    GuiStringMenu::render();
}
//*******************************
// GuiNetworkMenu::init
//*******************************
void GuiNetworkMenu::init() {
    GuiMenuBase::init();    // call the base init
    frame=0;
    // read from ssid.cfg if it exists
    driverMode = "wext";
    readCfgFile();
    updateData();
    fill();
}

//*******************************
// GuiNetworkMenu::readCfgFile
//*******************************
void GuiNetworkMenu::readCfgFile() {
    // read from ssid.cfg if it exists
    string cfgPath = getCfgPath();
    if (DirEntry::exists(cfgPath)) {
        ifstream ssidFile;
        ssidFile.open(cfgPath.c_str(), ios::binary);

        getline(ssidFile, ssid);
        Util::removeCRLFFromString(ssid);

        getline(ssidFile, password);
        Util::removeCRLFFromString(password);

        getline(ssidFile, driverMode);
        Util::removeCRLFFromString(driverMode);

        if (driverMode.empty()) driverMode = "wext";

        ssidFile.close();
    } else {
        ssid = "";
        password = "";
    }
}

//*******************************
// GuiNetworkMenu::writeCfgFile
//*******************************
void GuiNetworkMenu::writeCfgFile() {
    if (ssid.empty()) return;
    if (password.empty()) return;
    string cfgPath = getCfgPath();
    ofstream ssidFile(cfgPath, ios_base::trunc);
    ssidFile << ssid << endl;
    ssidFile << password << endl;
    ssidFile << driverMode << endl;
    ssidFile.flush();
    ssidFile.close();

    auto *networkController = new NetworkController();
    networkController->configure(ssid, password, driverMode);
    delete networkController;
}

//*******************************
// GuiNetworkMenu::initializeWifi
//*******************************
std::string GuiNetworkMenu::initializeWifi() {

    Gui::splash(_("Reinitializing Network"));
    usleep(2 * 1000000);
    auto *networkController = new NetworkController();
    networkController->restart();


    Gui::splash(_("Restarted Wi-Fi  With SSID:") + " " + ssid );
    usleep(2 * 1000000);


    delete networkController;

    return "";
}

//*******************************
// GuiNetworkMenu::getIPAddress
//*******************************
string GuiNetworkMenu::getIPAddress(string iface) {
    auto *networkController = new NetworkController();
    string ipAddress = networkController->getIPInfo(iface);
    delete networkController;

    if (ipAddress.empty())
    {
        ipAddress = _("Waiting for IP Address...");
    }
    return ipAddress;
}

//*******************************
// GuiNetworkMenu::getSSID
//*******************************
string GuiNetworkMenu::getSSID() {
    string ret = getSSIDfrom_ssidcfg();
    if (ret == "")
        ret = getSSIDfrom_wpa_supplicant();

    return ret;
}

//*******************************
// GuiNetworkMenu::getSSIDfrom_ssidcfg
//*******************************
string GuiNetworkMenu::getSSIDfrom_ssidcfg() {
    string ret;
    // read from ssid.cfg if it exists
    string cfgPath = getCfgPath();
    if (DirEntry::exists(cfgPath)) {
        ifstream ssidFile;
        ssidFile.open(cfgPath.c_str(), ios::binary);

        getline(ssidFile, ret);
        Util::removeCRLFFromString(ret);

        ssidFile.close();
    }

    return ret;
}

//*******************************
// GuiNetworkMenu::getSSIDfrom_wpa_supplicant
//*******************************
string GuiNetworkMenu::getSSIDfrom_wpa_supplicant() {
    string ret;
    string wpaPath = getWpaSupplicantPath();
    if (DirEntry::exists(wpaPath)) {
        ifstream wpaFile;
        wpaFile.open(wpaPath.c_str(), ios::binary);
        string line;
        string searchFor = "ssid=";
        while (getline(wpaFile, line)) {
            trim(line);
            if (!line.empty()) {
                auto pos = line.find(searchFor);
                if (pos != string::npos) {  // if found on this line
                    // get the rest of the line
                    ret = line.substr(pos + string(searchFor).size(), string::npos);
                    // remove the "" around the ssid
                    Util::removeCharsFromString(ret, "\"");
                    if (ret == "1")
                        ret = "";   // bleemsync will create an ssid of 1 if there is no ssid.cfg
                    break;
                }
            }
        }
        wpaFile.close();
    }

    return ret;
}

void GuiNetworkMenu::updateData()
{
    tz = gui->guiGetTZ();
    ipaddr = getIPAddress("wlan0");
}

//*******************************
// GuiNetworkMenu::fill
//*******************************
void GuiNetworkMenu::fill() {
    std::shared_ptr<Gui> gui(Gui::getInstance());
    lines.clear();
    lines.emplace_back(_("Wi-Fi Connection:"));
    lines.emplace_back(_("SSID:") + "  " + ssid);
    if (displayAsterisksInsteadOfPassword)
        lines.emplace_back(_("Password:") + "  " + string(password.size(), '*'));
    else
        lines.emplace_back(_("Password:") + "  " + password);
    lines.emplace_back(_("Driver mode:") +  "  " + driverMode);
    lines.emplace_back("");
    lines.emplace_back(_("Write Configuration/Restart Network"));
    lines.emplace_back("");
    lines.emplace_back(_("Restart Network"));
    lines.emplace_back(_("IP Address:") + " " + ipaddr);
    lines.emplace_back(_("Timezone:") + " " + tz);
}

//*******************************
// GuiNetworkMenu::getStatusLine
//*******************************
// returns the status line at the bottom
string GuiNetworkMenu::getStatusLine() {
    switch (selected) {
        case Text:
            return "   |@O| " + _("Cancel") + " |";

        case SSID:
            return "   |@X| " + _("Edit SSID") +
                   "   |@T| " + _("Scan SSID") +
                   "   |@O| " + _("Cancel") + " |";

        case PW:
            return "   |@X| " + _("Edit Password") +
                   "   |@O| " + _("Cancel") + " |";

        case WriteFile:
            return "   |@X| " + _("Write Config/Restart Network") +
                   "   |@O| " + _("Cancel") + " |";

        case Blank1:
        case Blank2:
            return "   |@O| " + _("Cancel") + " |";

        case InitNetwork:
            return "   |@X| " + _("Restart Network") +
                   "   |@O| " + _("Cancel") + " |";
        case DriverMode:
        case TimeZone:
            return "   |@X| " + _("Change") +
                   "   |@O| " + _("Cancel") + " |";

        default:
            //  assert(false);
            return "   |@O| " + _("Cancel") + " |";
    }
}

//*******************************
// GuiNetworkMenu::doCirclePressed
//*******************************
void GuiNetworkMenu::doCircle_Pressed() {
    Mix_PlayChannel(-1, gui->cancel, 0);
    menuVisible = false;
}


void GuiNetworkMenu::doTriangle_Pressed() {
    Mix_PlayChannel(-1, gui->cursor, 0);

    switch (selected) {
        case SSID: {
            auto *ssidSelect = new gui_ssidScanMenu(renderer);
            ssidSelect->show();
            bool cancelled = ssidSelect->cancelled;

            if (!cancelled) {
                ssid = ssidSelect->newSSID;
                fill();
            }
            delete (ssidSelect);
        }
            break;
    }
}

//*******************************
// GuiNetworkMenu::doCrossPressed
//*******************************
void GuiNetworkMenu::doCross_Pressed() {
    Mix_PlayChannel(-1, gui->cursor, 0);

    switch (selected) {
        case Text:
            break;

        case SSID: {
            GuiKeyboard *keyboard = new GuiKeyboard(renderer);
            keyboard->label = _("Enter SSID");
            keyboard->result = ssid;
            keyboard->show();
            string result = keyboard->result;
            bool cancelled = keyboard->cancelled;
            delete (keyboard);
            if (!cancelled) {
                ssid = result;
                fill();
            }
        }
            break;

        case PW: {
            GuiKeyboard *keyboard = new GuiKeyboard(renderer);
            keyboard->label = _("Enter Password");
            keyboard->result = password;
            keyboard->displayAsterisksInstead = displayAsterisksInsteadOfPassword;
            keyboard->show();
            string result = keyboard->result;
            bool cancelled = keyboard->cancelled;
            delete (keyboard);
            if (!cancelled) {
                password = result;
                fill();
            }
        }
            break;

        case WriteFile: {
            writeCfgFile();

            auto confirm = new GuiConfirm(renderer);
            confirm->label = _("Restart Networking Now?");
            confirm->show();
            bool result = confirm->result;
            delete confirm;
            if (result) {
#if defined(__x86_64__) || defined(_M_X64)
                // nope
#else
                initializeWifi();
#endif
            }

            //menuVisible = false;
        }
            break;

        case Blank1:
        case Blank2:
            break;

        case DriverMode:
            if (driverMode=="wext") driverMode="nl80211"; else driverMode="wext";
            break;


        case InitNetwork: {
#if defined(__x86_64__) || defined(_M_X64)
            // nope
#else
            initializeWifi();
#endif
        }
            break;

        case IpAddress:
            break;
        case TimeZone:
            auto *timeZoneMenu = new gui_timezoneSelect(renderer);
            timeZoneMenu->show();
            bool cancelled = timeZoneMenu->cancelled;

            if (!cancelled) {
                cout << "settime tzone \""+timeZoneMenu->newTimeZone+"\"" << endl;
                Util::execUnixCommand(("settime tzone \""+timeZoneMenu->newTimeZone+"\"").c_str());
                fill();
            }

            delete (timeZoneMenu);

            break;

            // default:
            //assert(false);
    }
    render();
}

