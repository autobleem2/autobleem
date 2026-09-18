//
// Created by screemer on 21.03.2020.
//

#include "gui_timezoneSelect.h"
#include <zconf.h>
#include <algorithm>
#include <sstream>
void gui_timezoneSelect::init()
{
    GuiMenuBase::init();    // call the base init
    Gui::splash(_("Loading timezones..."));
    fill();
};

void gui_timezoneSelect::fill()
{
    vector<string> timezones;
#if defined(__x86_64__) || defined(_M_X64)
    for (int i=0;i<200;i++) {
        timezones.emplace_back("Timezone Name");
    }
#else
    string allZones = Util::execUnixCommand("timedatectl list-timezones",true);
    std::istringstream f(allZones);
    std::string line;
    while (std::getline(f, line)) {
        line = Util::trim(line);
        if (!line.empty())
        {
            line.erase(remove(line.begin(), line.end(), '\n'), line.end());
            timezones.push_back(line);
        }
    }
    sort( timezones.begin(), timezones.end() );
    timezones.erase( unique( timezones.begin(), timezones.end() ), timezones.end() );
#endif



    lines.clear();
    for (string timezone:timezones)
    {
        lines.emplace_back(timezone);
    }



};



void gui_timezoneSelect::doCircle_Pressed()
{
    Mix_PlayChannel(-1, gui->cancel, 0);
    menuVisible = false;
    cancelled = true;
};

void gui_timezoneSelect::doCross_Pressed()
{
    if (!lines.empty()) {
        Mix_PlayChannel(-1, gui->cursor, 0);
        newTimeZone = lines[selected];
        cancelled = false;
        menuVisible = false;
    }
};