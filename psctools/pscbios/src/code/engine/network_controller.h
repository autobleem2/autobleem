//
// Created by screemer on 19.03.2020.
//

#ifndef AUTOBLEEM_GUI_NETWORK_CONTROLLER_H
#define AUTOBLEEM_GUI_NETWORK_CONTROLLER_H

#include "../util.h"


using namespace std;

class NetworkController {
public:
    bool interfaceFound(string interface);
    string configure(string SSID, string pass, string driverMode);
    string getIPInfo(string interface);
    string restart();
    vector<string> scan();
    bool isWlanOn();
    bool isUp(string interface);
    bool isBluetoothOn();
    string getBluetoothInfo();

};


#endif //AUTOBLEEM_GUI_NETWORK_CONTROLLER_H
