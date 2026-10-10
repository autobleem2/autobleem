#pragma once

#include "gui/screens/gui_facts_page.h"

//********************
// GuiPadDaemon
//********************
// System -> Virtual gamepad: the pads abpadd, the virtual gamepad daemon Apps are given, sees - each one's name,
// the SDL driver it came through, whether the mapping is from gamecontrollerdb.txt or guessed, and whether the
// daemon could open it. The daemon only runs while an App does, so the page asks it once, with its one-shot
// `abpadd --list` (apps/abpad/src/core/pad_list.h), when it opens; Cross asks again. Read-only.
class GuiPadDaemon : public GuiFactsPage {
public:
    using GuiFactsPage::GuiFactsPage;

    // where abpadd lives next to the running program ("" when this build has none)
    static std::string daemonPath();

protected:
    std::string title() override { return _("Virtual gamepad"); }
    std::vector<abgui::FactsSection> collect() override;
    std::string extraHints() override { return "|@X| " + _("Look again"); }
    bool onButton(ableem::Button button) override;

private:
    std::vector<std::string> listing; // abpadd --list's lines, read when the page opens or Cross is pressed
    bool daemonFound = false;
    bool listed = false;
    void look();
};
