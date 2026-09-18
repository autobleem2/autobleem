//
// Created by screemer on 04.05.2020.
//

#pragma once

#include "../gui_screen.h"

class bluetooth_info : public GuiScreen {
public:
    void render() override;

    void loop() override;

    using GuiScreen::GuiScreen;

};

