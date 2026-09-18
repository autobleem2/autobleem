//
// GuiHardwareInfo: the Hardware Information screen for a machine without the PSC-Bios app - a Raspberry Pi
// or a PC. The system menu shows it instead of running the app when there is no app to run.
//
#pragma once

#include "../gui_screen.h"
#include "../../core/services/system_info.h"

#include <ableem/ui/font.h>

#include <string>
#include <vector>

//********************
// GuiHardwareInfo
//********************
// A read-only list in the classic layout (panel, small logo, title, status bar): SystemInfoService's sections
// - the OS, the hardware, the volumes and their free space, the network - followed by what only the running
// program can tell (the renderer and display, SDL, the audio driver, the connected pads). The values that
// move (uptime, temperature, free memory, free space) are re-read every RefreshInterval while the screen is
// up. Up/Down scroll a row, L1/R1 (or Left/Right) a page, Circle goes back.
class GuiHardwareInfo : public GuiScreen {
public:
    using GuiScreen::GuiScreen;

    void init() override;
    void render() override;
    void loop() override;

    static const unsigned int RefreshInterval = 1000; // ms between re-reads of the sections

private:
    // one drawn line: a section heading, or a label and its value
    struct Line {
        bool heading = false;
        std::string label;
        std::string value;
    };
    std::vector<Line> lines;
    int firstVisible = 0; // index into lines of the top row on the screen
    int rowsThatFit = 1;  // rows the panel holds at the font's height, from the last render()
    unsigned int lastRefresh = 0;
    ableem::Font font;
    SystemInfoService systemInfo;

    void refresh();                // rebuild lines from the service and the display/input facts
    InfoSection displayAndInput(); // the section only the screen can fill in
    void scrollBy(int rows);       // clamped to the list
    int maxFirstVisible() const;   // the last top row that still fills the panel
};
