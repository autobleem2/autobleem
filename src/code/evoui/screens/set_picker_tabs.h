//
// SetPickerTabs: which tabs the set picker has and where each sits - PlayStation, RetroArch (only when the
// RetroArch program is installed, Env::retroArchInstalled) and Apps. Pure, so the tab list can be tested.
//
#pragma once

#include "core/model/game_set.h"

struct SetPickerTabs {
    bool retroArch = true;

    int count() const { return retroArch ? 3 : 2; }
    int playStation() const { return 0; }
    int retroArchTab() const { return retroArch ? 1 : -1; } // -1: no such tab
    int apps() const { return retroArch ? 2 : 1; }
    // the tab a selection's set lives on (Lightgun is a PlayStation tab row; a RetroArch set with no RetroArch tab
    // falls back to the first tab)
    int tabFor(GameSet set) const {
        if (set == GameSet::Apps)
            return apps();
        if (set == GameSet::RetroArch && retroArch)
            return retroArchTab();
        return playStation();
    }
};
