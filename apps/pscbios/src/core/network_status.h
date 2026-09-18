//
// NetworkStatus: what the main screen shows about the network - the WiFi and ethernet dongles, their
// addresses, the Bluetooth dongle, the timezone - refreshed from the ConsoleBackend every so often (each
// field is one abnet call on the console, so not every frame).
//
#pragma once

#include <string>

class ConsoleBackend;

//******************
// NetworkStatus
//******************
struct NetworkStatus {
    bool wirelessFound = false;
    bool wirelessActive = false;
    std::string wirelessAddr;
    bool ethFound = false;
    bool ethActive = false;
    std::string ethAddr;
    bool btActive = false;
    std::string btName;
    std::string timezone;

    void refresh(ConsoleBackend &backend);

    // "<Found|Not found>/<Active|Not active>", translated - the dongle line of the main screen
    static std::string dongleStatus(bool found, bool active);
    // the address, or the translated "Waiting for IP Address..." when there is none yet, "-" for no dongle
    static std::string addressText(bool found, const std::string &address);
};
