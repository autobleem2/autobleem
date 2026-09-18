//
// Clock: the "last played" time as text.
//
#include "clock.h"

using namespace std;

const char *const Clock::DefaultFormat = "%F %I:%M:%S %p"; // YYYY-MM-DD HH:MM:SS AM/PM

//*******************************
// Clock::displayTime
//*******************************
string Clock::displayTime(time_t t, const string &_format) const {
    string format = _format; // if you pass a format it uses that

    if (format == "") {
        // see if the user has a preferred format in config.ini
        string datetimeFormat = config_.inifile.values["datetimeformat"];
        format = datetimeFormat != "" ? datetimeFormat : DefaultFormat;
    }

    string datetime;
    if (t != 0) {
        tm *local = localtime(&t);
        if ((local != nullptr) && (local->tm_year + 1900 >= 2020)) { // a time the console could have known
            char buf[200];
            if (std::strftime(buf, sizeof(buf), format.c_str(), local))
                datetime = buf;
        }
    }

    return datetime;
}
