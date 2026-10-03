//
// Created by screemer on 2/16/19.
//

#include "evoui_menu.h"
#include "core/model/timing.h"
#include "gui/gui.h"
using namespace std;

//*******************************
// PsMenu::PsMenu
//*******************************
PsMenu::PsMenu(string name1, const LauncherTheme::MenuIcons &icons) : PsObj(name1, "") {
    settings = ThemeAssets::loadImage(renderer, icons.settings);
    guide = ThemeAssets::loadImage(renderer, icons.guide);
    memcard = ThemeAssets::loadImage(renderer, icons.memcard);
    savestate = ThemeAssets::loadImage(renderer, icons.resume);
    if (icons.resumePicture.set)
        resumePicture =
            ableem::Rect(icons.resumePicture.x, icons.resumePicture.y, icons.resumePicture.w, icons.resumePicture.h);
    x = 640 - 118 / 2;
    y = 520;
    oy = y;
    ox = x;
}

//*******************************
// PsMenu::freeAssets
//*******************************
void PsMenu::freeAssets() {
    resume = ableem::Texture();
}

#define ICON_GAP evomotion::IconGap

//*******************************
// PsMenu::settle
//*******************************
void PsMenu::settle(bool open, int restY) {
    owner_.cancel(); // a transition cut short leaves nothing half-way
    moving_ = false;
    y = oy = targety = restY;
    active = open;
    for (int i = 0; i < 4; i++) {
        optionscales[i] = 1.0f;
        xoff[i] = 0;
        yoff[i] = 0;
    }
    if (open) {
        optionscales[selOption] = maxZoom;
        xoff[selOption] = zoomOffset(maxZoom);
        yoff[selOption] = zoomOffset(maxZoom);
    }
}

namespace {
// the selected icon's scale, and the offsets that keep it centred on its place
void setScale(PsMenu &menu, int option, float scale) {
    menu.optionscales[option] = scale;
    menu.xoff[option] = PsMenu::zoomOffset(scale);
    menu.yoff[option] = PsMenu::zoomOffset(scale);
}
} // namespace

//*******************************
// PsMenu::startTransition
//*******************************
// a new transition replaces the one running (the tween of the old one stops where it is), as restarting did
void PsMenu::startTransition() {
    owner_.cancel();
    progress_ = 0;
    moving_ = true;
    gui->uiContext().stack().tweens().start(
        abgui::Tween(progress_, 0.0f, 1.0f, static_cast<unsigned int>(duration)).onEnd([this]() {
            completeTransition();
        }),
        owner_);
}

//*******************************
// PsMenu::applyProgress
//*******************************
// the positions of the running transition at its eased progress: the old update()'s numbers
void PsMenu::applyProgress() {
    if (!moving_)
        return;
    const float progress = progress_;
    if (transition == TR_MENUON) {
        y = evomotion::rowY(oy, targety, progress);
        setScale(*this, selOption,
                 active ? evomotion::openingScale(progress, maxZoom) : evomotion::closingScale(progress, maxZoom));
    } else {
        x = evomotion::optionX(direction == 0 ? 0 : 1, ox, progress);
        // the icon left shrinks while the one entered grows (UIREV-44), both over the same eased progress
        setScale(*this, selOption, evomotion::closingScale(progress, maxZoom));
        const int entered = selOption + (direction == 0 ? -1 : 1);
        if (entered >= 0 && entered < 4)
            setScale(*this, entered, evomotion::openingScale(progress, maxZoom));
    }
}

//*******************************
// PsMenu::completeTransition
//*******************************
// the end of the tween: everything at its exact resting value, the selection moved for an option move
void PsMenu::completeTransition() {
    moving_ = false;
    if (transition == TR_MENUON) {
        y = evomotion::rowY(oy, targety, 1.0f);
        oy = y;
        setScale(*this, selOption, active ? 1 + (maxZoom - 1) : 1.0f);
    } else if (direction == 0) {
        setScale(*this, selOption, 1.0f);
        xoff[selOption] = 0;
        yoff[selOption] = 0;
        selOption--;
        setScale(*this, selOption, maxZoom);
        x = ox + ICON_GAP;
        ox = x;
    } else {
        setScale(*this, selOption, 1.0f);
        xoff[selOption] = 0;
        yoff[selOption] = 0;
        selOption++;
        setScale(*this, selOption, maxZoom);
        x = ox - ICON_GAP;
        ox = x;
    }
}

//*******************************
// PsMenu::render
//*******************************
void PsMenu::render() {
    applyProgress();
    static const float slots[4] = {0, ICON_GAP, ICON_GAP * 2, ICON_GAP * 3};
    const ableem::Texture *icons[4] = {&settings, &guide, &memcard, &savestate};
    const ableem::Rect input(0, 0, 118, 118);
    // the Resume icon greyed when the selected game has no resume points - still drawn, still selectable
    // (how faint is the theme's `inactive.resume` when it sets one, else 120)
    const unsigned char resumeAlpha =
        resumeAvailable ? 255 : abgui::InactiveAlphas::orToday(gui->uiContext().inactiveAlphas().resume, 120);
    savestate.setAlphaMod(resumeAlpha);
    resume.setAlphaMod(resumeAlpha);
    // the theme's `tile` / `tileSelected` frames (G5i), asked at draw time: one behind each icon's (zoomed) box, the
    // selected one's while the row is open; no such frame = the icons alone, as before
    abgui::Context &ctx = gui->uiContext();
    const bool tileFrame = ctx.frame("tile").valid();
    const bool selectedFrame = ctx.frame("tileSelected").valid();
    const abgui::Style &style = ctx.style();
    for (int i = 0; i < 4; i++) {
        if (i > 0 && !enabled[i])
            continue;
        const float left = x + slots[i] + xoff[i];
        const float top = y + yoff[i];
        const float size = 118 * optionscales[i];
        if (tileFrame || selectedFrame) {
            const bool selected = active && i == selOption;
            const ableem::Rect box(static_cast<int>(left + 0.5f), static_cast<int>(top + 0.5f),
                                   static_cast<int>(size + 0.5f), static_cast<int>(size + 0.5f));
            const unsigned char alpha = i == 3 ? resumeAlpha : 255;
            if (!(selected && selectedFrame && style.drawFrame(ctx, "tileSelected", box, alpha)))
                style.drawFrame(ctx, "tile", box, alpha);
        }
        renderer.copy(*icons[i], &input, ableem::FRect(left, top, size, size));

        if (i == 3 && resume.valid()) {
            const ableem::Size s = resume.size();
            const ableem::Rect whole(0, 0, s.w, s.h);
            renderer.copy(resume, &whole,
                          ableem::FRect(left + resumePicture.x * optionscales[3],
                                        top + resumePicture.y * optionscales[3], resumePicture.w * optionscales[3],
                                        resumePicture.h * optionscales[3]));
        }
    }
}

//*******************************
// PsMenu::setResumePic
//*******************************
void PsMenu::setResumePic(string picturePath) {
    setResumeTex(ableem::Texture::loadFile(renderer, picturePath));
}

//*******************************
// PsMenu::setResumeTex
//*******************************
// the mask of the theme's resume icon is multiplied in here, once per picture (G5s) - not in render()
void PsMenu::setResumeTex(const ableem::Texture &picture) {
    resume = Gui::getInstance()->maskedResumePicture(picture);
}
