//
// UtilTime: the clock, and how it is formatted for the "last played" column.
//
#pragma once

#include <string>
#include <ctime>

//******************
// UtilTime
//******************
// The PSC has no battery-backed clock, so unless the AutoBleem kernel managed to set the time over WiFi the
// "last played" times are meaningless - that is what usingWiFiUpdatedTime() is for: callers hide the column
// when it returns false.
class UtilTime {
public:
    static time_t getCurrentTime();

    // true if we are on the AutoBleem kernel and it set the clock from the network
    static bool usingWiFiUpdatedTime();

    // formats t with `format`, or with config.ini's "datetimeformat" when format is empty
    static std::string timeToDisplayTimeString(time_t t, const std::string &format = "");
};
