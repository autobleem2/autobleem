//
// Created by screemer on 2019-01-24.
//
#pragma once

// GuiScreen and its input types now live in lib_ableem, portable across every target. This thin app-side
// wrapper keeps the members every existing screen file already expects: a `gui` shared_ptr<Gui> (the app's
// theme/database-aware singleton, not just the abstract ableem::GuiBase) and a bare `renderer` reference, so
// none of those files need to touch how they reach the Gui/renderer - only their SDL-specific calls change.
#include <ableem/gui_screen.h>
#include "gui.h"

using ableem::Event;
using ableem::Button;
using ableem::Key;

//********************
// GuiScreen
//********************
class GuiScreen : public ableem::GuiScreen {
public:
    explicit GuiScreen(ableem::GuiBase &_gui)
        : ableem::GuiScreen(_gui), gui(Gui::getInstance()), renderer(_gui.renderer()) {}

    std::shared_ptr<Gui> gui;
    ableem::Renderer &renderer;
};
