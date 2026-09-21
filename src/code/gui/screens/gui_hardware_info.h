//
// GuiHardwareInfo: the Hardware Information screen for a machine without the PSC-Bios app - a Raspberry Pi
// or a PC. The system menu shows it instead of running the app when there is no app to run.
//
#pragma once

#include "gui_facts_page.h"
#include "../../core/services/system_info.h"

//********************
// GuiHardwareInfo
//********************
// A GuiFactsPage of SystemInfoService's sections - the OS, the hardware, the volumes and their free space,
// the network - followed by what only the running program can tell (the renderer and display, SDL, the
// audio driver, the connected pads). The values that move (uptime, temperature, free memory, free space)
// are re-read every second while the screen is up.
class GuiHardwareInfo : public GuiFactsPage {
public:
    using GuiFactsPage::GuiFactsPage;

protected:
    std::string title() override { return _("Hardware Information"); }
    std::vector<InfoSection> collect() override;

private:
    SystemInfoService systemInfo;
    InfoSection displayAndInput(); // the section only the screen can fill in
};
