//
// Clock: the console's idea of the time, and how it is shown in the "last played" column.
//
#pragma once

#include "config.h"

#include <ctime>
#include <string>

//******************
// Clock
//******************
// The PSC has no battery-backed clock. Unless the AutoBleem kernel set the time over WiFi, the clock starts
// at the epoch and every timestamp is from 1970 - so a time before 2020 is treated as "no clock" and shown
// as nothing. Was UtilTime.
//
// Owned by App (App::clock()).
class Clock {
public:
    explicit Clock(Config &config) : config_(config) {}

    // t formatted with `format`, or with config.ini's "datetimeformat" when format is empty, or with
    // "%F %I:%M:%S %p" when that is too. "" for a time the console could not have known (0, or before 2020).
    std::string displayTime(time_t t, const std::string &format = "") const;

    static const char *const DefaultFormat;

private:
    Config &config_;
};
