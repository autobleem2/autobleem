//
// Created by screemer on 2/16/19.
//

#pragma once

#include <ableem/ui/texture.h>
#include <ab_gui/tween.h>
#include "evoui_motion.h"
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
    int targety = 0;
    int duration = 0;
    bool active = false;
    // the row at rest at `restY` with no animation running: every icon at its size, the selected one
    // zoomed when the row is open (`open`) - what a rebuild of the row (GuiLauncher::showOptions) settles
    // it to, so cutting an animation short leaves nothing half-way
    void settle(bool open, int restY);

    // A transition is a non-ambient tween (ab_gui G5o3) on the program's Tweens: the caller sets `transition`
    // (TR_MENUON: the row slides to `targety` and the selected icon zooms with `active`; TR_OPTION: the selection
    // moves one icon, `direction` 0 left / 1 right), `duration` and, for the slide, `targety`, then calls
    // startTransition(). While one runs animating() is true and the caller does not start another (a press
    // during it is ignored, as before); the DebugDriver is busy meanwhile. The positions are the eased progress
    // put through evoui_motion.h's formulas in render(); the end settles everything exactly.
    void startTransition();
    bool animating() const { return moving_; }

    // which of the four positions are shown, left to right - always a prefix: settings alone for an App,
    // settings + game editor (+ resume, in the memory card's place) for a RetroArch game, all four for a PS1 game
    // (GuiLauncher::showOptions)
    bool enabled[4] = {true, true, true, true};
    // the Resume icon (index 3) is still selectable with no resume points, just greyed - so the cursor can
    // pass it and Cross can tell the player why nothing happens (GuiLauncher::showOptions)
    bool resumeAvailable = true;
    // A RetroArch game has no memory card here: its Resume icon takes the memory card's place (position 2), so the
    // row is a plain run of three icons with no gap. Positions and the icons they show: optionAt(). enabled[3] is
    // then false (position 3 does not exist).
    bool resumeAtMemcard = false;
    // the icon kind at a position of the row: 0 settings, 1 game editor, 2 memory card, 3 resume
    int optionAt(int position) const { return resumeAtMemcard && position == 2 ? 3 : position; }
    int lastEnabled() const {
        int last = 0;
        for (int i = 0; i < 4; i++)
            if (enabled[i])
                last = i;
        return last;
    }
    int direction = 0;

    // the offset that keeps an icon drawn at `scale` centred on its unzoomed place
    static float zoomOffset(float scale) { return evomotion::zoomOffset(scale); }

    void freeAssets();
    void render() override;

    void setResumePic(std::string picturePath);
    // the same picture decoded elsewhere (the launcher's background loader)
    // (through the theme's resume picture mask, when it has one - Gui::maskedResumePicture)
    void setResumeTex(const ableem::Texture &picture);

    int transition = 0;

    // the four icons come from the theme's launcher.menuIcons, already resolved to files
    PsMenu(std::string name1, const LauncherTheme::MenuIcons &icons);

private:
    // the running transition's eased progress (the tween writes it), and whether one is running - true from
    // startTransition() until its end callback has settled the row
    void applyProgress();
    void completeTransition();
    float progress_ = 0;
    bool moving_ = false;
    // the tween is started for this owner, so it never writes after the row is gone
    abgui::TweenOwner owner_;
};
