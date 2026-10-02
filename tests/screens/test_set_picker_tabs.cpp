//
// SetPickerTabs (evoui/screens/set_picker_tabs.h, pure): the set picker's tab list with and without the RetroArch
// tab - the program not installed (Env::retroArchInstalled) leaves two tabs, and every set still lands on a tab
// that exists.
//
#include "doctest/doctest.h"

#include "set_picker_tabs.h"

TEST_CASE("set picker tabs: with RetroArch there are three, PlayStation / RetroArch / Apps") {
    SetPickerTabs t;
    t.retroArch = true;
    CHECK(t.count() == 3);
    CHECK(t.playStation() == 0);
    CHECK(t.retroArchTab() == 1);
    CHECK(t.apps() == 2);
    CHECK(t.tabFor(GameSet::PS1) == 0);
    CHECK(t.tabFor(GameSet::Lightgun) == 0);
    CHECK(t.tabFor(GameSet::RetroArch) == 1);
    CHECK(t.tabFor(GameSet::Apps) == 2);
}

TEST_CASE("set picker tabs: without RetroArch there are two and no RetroArch tab") {
    SetPickerTabs t;
    t.retroArch = false;
    CHECK(t.count() == 2);
    CHECK(t.playStation() == 0);
    CHECK(t.retroArchTab() == -1);
    CHECK(t.apps() == 1);
    CHECK(t.tabFor(GameSet::PS1) == 0);
    CHECK(t.tabFor(GameSet::Lightgun) == 0);
    CHECK(t.tabFor(GameSet::Apps) == 1);
    // a remembered RetroArch set falls back to the first tab, never past the end
    CHECK(t.tabFor(GameSet::RetroArch) == 0);
}

TEST_CASE("set picker tabs: L1/R1 cycling stays inside the tabs") {
    for (bool ra : {true, false}) {
        SetPickerTabs t;
        t.retroArch = ra;
        int tab = 0;
        for (int i = 0; i < t.count(); i++)
            tab = (tab + 1) % t.count();
        CHECK(tab == 0);
        tab = (tab + t.count() - 1) % t.count();
        CHECK(tab == t.count() - 1);
        CHECK(tab == t.apps());
    }
}
