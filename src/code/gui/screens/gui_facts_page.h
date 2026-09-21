//
// GuiFactsPage: a read-only page of facts in the classic panel - sections with a heading band each and
// label/value rows under it, the values in a column a third of the way across (a long one elided), as many
// rows as the panel holds, scrolling a row at a time with edge markers, re-read every refreshInterval while
// the page is up. Up/Down a row, Left/Right and L2/R2 a page, Circle back; a subclass adds its own buttons
// through onButton() and their hints through extraHints(). Hardware Information (the launcher) and
// PSC-Bios's opening screen are the two.
//
#pragma once

#include "../gui_screen.h"
#include "../../core/services/system_info.h"

#include <ableem/ui/font.h>

#include <string>
#include <vector>

//********************
// GuiFactsPage
//********************
class GuiFactsPage : public GuiScreen {
public:
    using GuiScreen::GuiScreen;

    void init() override;
    void render() override;
    void loop() override;

    unsigned int refreshInterval = 1000; // ms between re-reads of the sections

protected:
    virtual std::string title() = 0;
    virtual std::vector<InfoSection> collect() = 0; // the sections, fresh
    // "|@Select| WiFi settings   |@S| ..." - the page's own hints, before Back and the page counter
    virtual std::string extraHints() { return ""; }
    // a button the page's own: true when taken (Circle is the page's, when not taken here)
    virtual bool onButton(ableem::Button /*button*/) { return false; }

    void refresh(); // rebuild the rows from collect() - also after a sub-screen that may have changed them

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

    void scrollBy(int rows);     // clamped to the list
    int maxFirstVisible() const; // the last top row that still fills the panel
};
