//
// Created by screemer on 19.03.2020.
//

#include <iostream>
#include <algorithm>
#include <sstream>
#include "network_controller.h"

using namespace std;

bool NetworkController::interfaceFound(string interface)
{
    string interfaces=Util::execUnixCommand("/bin/abnet list_ifaces");
    return interfaces.find(interface)!=string::npos;
}

string NetworkController::configure(string SSID, string pass, string driverMode)
{
    string command = "/bin/abnet configure \""+SSID+"\" \""+pass+"\"";
    Util::execUnixCommand(command.c_str());
    command = "/bin/abnet driver_mode "+driverMode;
    Util::execUnixCommand(command.c_str());
    return "";
}
string NetworkController::getIPInfo(string interface)
{
    string command = "/bin/abnet show_ip "+interface;
    if(interfaceFound(interface))
    {
        return Util::execUnixCommand(command.c_str());
    }
    return "-";
}
string NetworkController::restart()
{
    string command = "/bin/abnet restart";
    Util::execUnixCommand(command.c_str());
    return "";
}
vector<string> NetworkController::scan()
{
    vector<string> result;
    if (interfaceFound("wlan0") && isUp("wlan0"))
    {
        string ssids = Util::execUnixCommand("/bin/abnet scan",true);
        std::istringstream f(ssids);
        std::string line;
        while (std::getline(f, line)) {
            line = Util::trim(line);
            if (!line.empty())
            {
                line.erase(remove(line.begin(), line.end(), '\n'), line.end());
                result.push_back(line);
            }
        }
        sort( result.begin(), result.end() );
        result.erase( unique( result.begin(), result.end() ), result.end() );
        return result;
    } else
        return result;
}
bool NetworkController::isWlanOn()
{
    string ison = Util::execUnixCommand("/bin/abnet wlan_on");
    return ison=="yes";
}

bool NetworkController::isUp(string interface)
{
    string command = "/bin/abnet is_up "+interface;
    string ison = Util::execUnixCommand(command.c_str());
    return ison=="yes";
}

bool NetworkController::isBluetoothOn()
{
    string command = "/bin/abnet bt_up ";
    string ison = Util::execUnixCommand(command.c_str());
    return ison=="yes";
}

string NetworkController::getBluetoothInfo()
{
    string command = "/bin/abnet bt_name ";
    return Util::execUnixCommand(command.c_str());
}