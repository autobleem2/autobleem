#pragma once

#include "gui/screens/gui_confirm.h"

//********************
// GuiKeepDisplay
//********************
// Options -> Display's safety net: "Keep this display mode?" over the new mode, counting down. Cross keeps it
// (result = true); Circle, or nothing for Seconds - a TV that shows nothing in the new mode - goes back.
class GuiKeepDisplay : public GuiConfirm {
public:
    static constexpr int Seconds = 15;

    void init() override;
    void loop() override;

    std::string modeLabel; // what the question names, "1080p"

    using GuiConfirm::GuiConfirm;

private:
    void updateLabel(int secondsLeft);
};
