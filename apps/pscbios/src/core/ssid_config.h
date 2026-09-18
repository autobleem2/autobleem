//
// SsidConfig: the WiFi credentials file the AutoBleem kernel's abnet reads - /etc/autobleem/ssid.cfg, three
// lines: the SSID, the password, the driver mode ("wext" or "nl80211"). With no file the SSID may still be
// found in /etc/wpa_supplicant.conf (what an earlier setup wrote there).
//
#pragma once

#include <string>

//******************
// SsidConfig
//******************
class SsidConfig {
public:
    std::string ssid;
    std::string password;
    std::string driverMode = "wext";

    // reads `path`; false (and the defaults kept) when it is not there
    bool load(const std::string &path);
    // writes the three lines; refuses (false) an empty SSID or password, as the tool always has
    bool save(const std::string &path) const;

    // the ssid= of a wpa_supplicant.conf; "" when there is none, or when it is "1" (what an old setup
    // wrote when it had no ssid.cfg)
    static std::string ssidFromWpaSupplicant(const std::string &path);

    // where they are on the console: <kernel config dir>/ssid.cfg and /etc/wpa_supplicant.conf; on a
    // machine without the kernel config dir, next to the tool (so a dev host round-trips them)
    static std::string defaultPath();
    static std::string wpaSupplicantPath();
};
