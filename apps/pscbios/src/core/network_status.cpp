//
// NetworkStatus: one round of questions to the console.
//
#include "network_status.h"
#include "console_backend.h"
#include "core/main.h"

using namespace std;

//*******************************
// NetworkStatus::refresh
//*******************************
void NetworkStatus::refresh(ConsoleBackend &backend) {
    wirelessFound = backend.wlanOn();
    wirelessActive = backend.isUp("wlan0");
    wirelessAddr = backend.ipOf("wlan0");
    ethFound = backend.interfaceFound("eth0");
    ethActive = backend.isUp("eth0");
    ethAddr = backend.ipOf("eth0");
    btActive = backend.btUp();
    btName = backend.btName();
    timezone = backend.timezone();
}

//*******************************
// NetworkStatus::dongleStatus / addressText
//*******************************
string NetworkStatus::dongleStatus(bool found, bool active) {
    return (found ? _("Found") : _("Not found")) + "/" + (active ? _("Active") : _("Not active"));
}

string NetworkStatus::addressText(bool found, const string &address) {
    if (!found)
        return "-";
    return address.empty() ? _("Waiting for IP Address...") : address;
}
