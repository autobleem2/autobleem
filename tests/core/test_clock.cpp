//
// Clock: the "last played" time as text, and when there is none to show.
//
#include "doctest/doctest.h"

#include "../support/env_fixture.h"
#include "../support/temp_dir.h"

#include "core/services/clock.h"

#include <ctime>
#include <memory>
#include <string>

using std::string;

namespace {

struct ClockWith {
    explicit ClockWith(const string &configIni) : tmp("clock") {
        env.setWorkingPath(tmp.path());
        tmp.writeFile("config.ini", configIni);
        config.reset(new Config);
        clock.reset(new Clock(*config));
    }
    EnvFixture env;
    TempDir tmp;
    std::unique_ptr<Config> config;
    std::unique_ptr<Clock> clock;
};

// a local time in the middle of the day, so that no timezone moves the date
time_t localNoon(int year, int month, int day) {
    tm t = {};
    t.tm_year = year - 1900;
    t.tm_mon = month - 1;
    t.tm_mday = day;
    t.tm_hour = 12;
    t.tm_isdst = -1;
    return mktime(&t);
}

} // namespace

TEST_CASE("a time is shown in config.ini's datetimeformat") {
    ClockWith c("Datetimeformat=%d/%m/%Y\n");
    CHECK(c.clock->displayTime(localNoon(2024, 3, 9)) == "09/03/2024");
}

TEST_CASE("without a configured format the default one is used, and a passed format wins over both") {
    ClockWith c("");
    CHECK(c.clock->displayTime(localNoon(2024, 3, 9)) == "2024-03-09 12:00:00 PM");
    CHECK(c.clock->displayTime(localNoon(2024, 3, 9), "%Y") == "2024");
}

TEST_CASE("a time the console could not have known is shown as nothing") {
    ClockWith c("");
    CHECK(c.clock->displayTime(0) == "");                       // never played
    CHECK(c.clock->displayTime(localNoon(1970, 1, 2)) == "");   // the clock was never set: still 1970
    CHECK(c.clock->displayTime(localNoon(2019, 12, 31)) == ""); // before the cut-off
    CHECK(c.clock->displayTime(localNoon(2020, 1, 1), "%Y") == "2020");
}
