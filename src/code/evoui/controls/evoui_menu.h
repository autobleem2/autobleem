//
// Created by screemer on 2/16/19.
//

#pragma once

#include <ableem/ui/texture.h>
#include "evoui_obj.h"
#include "core/main.h"
#include <string>

#define TR_MENUON 0
#define TR_OPTION 1

//******************
// PsMenu
//******************
class PsMenu : public PsObj {
public:
    ableem::Texture settings;
    ableem::Texture guide;
    ableem::Texture memcard;
    ableem::Texture savestate;
    ableem::Texture resume;
    // where `resume` is pasted on the resume icon, in the icon's pixels (theme launcher.menuIcons.resumePicture)
    ableem::Rect resumePicture{25, 33, 68, 52};

    // fractional pixels: the zoomed icon stays centred and the row slides without 1-pixel steps
    float x = 0, y = 0, oy = 0, ox = 0;
    float xoff[4] = {0, 0, 0, 0};
    float yoff[4] = {0, 0, 0, 0};

    float optionscales[4] = {1.0f, 1.0f, 1.0f, 1.0f};

    float maxZoom = 1.5;

    int selOption = 0;
    int animationStarted = 0;
    int targety = 0;
    int duration = 0;
    bool active = false;
    // the row at rest at `restY` with no animation running: every icon at its size, the selected one
    // zoomed when the row is open (`open`) - what a rebuild of the row (GuiLauncher::showOptions) settles
    // it to, so cutting an animation short leaves nothing half-way
    void settle(bool open, int restY);

    // which of the four icons are shown, left to right - always a prefix: settings alone for an App,
    // settings + game editor for a RetroArch game, all four for a PS1 game (GuiLauncher::showOptions)
    bool enabled[4] = {true, true, true, true};
    int lastEnabled() const {
        int last = 0;
        for (int i = 0; i < 4; i++)
            if (enabled[i])
                last = i;
        return last;
    }
    int direction = 0;

    // the offset that keeps an icon drawn at `scale` centred on its unzoomed place
    static float zoomOffset(float scale) { return -(118.0f * scale - 118.0f) / 2.0f; }

    void freeAssets();
    void update(long time) override;
    void render() override;

    void setResumePic(std::string picturePath);
    // the same picture decoded elsewhere (the launcher's background loader)
    void setResumeTex(const ableem::Texture &picture) { resume = picture; }

    int transition = 0;

    // the four icons come from the theme's launcher.menuIcons, already resolved to files
    PsMenu(std::string name1, const LauncherTheme::MenuIcons &icons);
};
