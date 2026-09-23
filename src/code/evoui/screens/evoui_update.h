//
// The online update's screens (AB_ONLINE_UPDATE builds - a Pi, a dev host): GuiUpdatePrompt asks what to
// do about an update UpdateService found, GuiUpdateProgress shows a check or a download running. Both are
// panels over the launcher's dimmed background, in the launcher's fonts and colours, like GuiSystemMenu.
//
#pragma once

#include "gui/gui_screen.h"
#include "gui/panel_style.h"
#include "core/services/update_service.h"

#include <string>
#include <vector>

//******************
// UpdateChoice
//******************
enum class UpdateChoice { Later, Now, Skip };

//******************
// GuiUpdatePrompt
//******************
// "An update is available": what it is (the versions), three rows - Update now, Remind me tomorrow, Skip
// this version. Circle is "later" too.
class GuiUpdatePrompt : public GuiScreen {
public:
    void init() override;
    void render() override;
    void loop() override;

    UpdateInfo info;
    ableem::Texture background; // the launcher's background, drawn dimmed under the panel
    UpdateChoice result = UpdateChoice::Later;

    using GuiScreen::GuiScreen;

private:
    struct Item {
        UpdateChoice choice;
        std::string title;
        std::string description;
    };
    std::vector<Item> items;
    std::vector<std::string> lines; // what the update is
    int selected = 0;
    PanelStyle style;
};

//******************
// GuiUpdateProgress
//******************
// Polls the service every frame and draws its phase: a check ("Checking for updates..."), a download
// (a bar over the bytes, the file's name), and the outcome - then returns. Nothing to press: the
// download is a few tens of MB and the Pi goes on to the installer right after.
class GuiUpdateProgress : public GuiScreen {
public:
    void init() override;
    void render() override;
    void loop() override;

    ableem::Texture background;
    UpdateService::Status finalStatus; // what the service ended in

    using GuiScreen::GuiScreen;

private:
    UpdateService::Status status;
    unsigned long shownSince = 0; // an outcome stays readable for a moment before the screen closes
    PanelStyle style;
};
