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

#define ICON_GAP 130.0f

//*******************************
// PsMenu::settle
//*******************************
void PsMenu::settle(bool open, int restY) {
    animationStarted = 0;
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

//*******************************
// PsMenu::update
//*******************************
void PsMenu::update(long time) {
    if (animationStarted != 0) {
        float progress = time - animationStarted;
        progress = progress / (duration * 1.0f);
        if (progress > 1)
            progress = 1;
        if (progress < 0)
            progress = 0;
        progress = easeOutCubic(progress);

        if (transition == TR_MENUON) {
            y = oy + (progress * (targety - oy));

            if (active) {

                optionscales[selOption] = 1 + progress * (maxZoom - 1);
                xoff[selOption] = zoomOffset(optionscales[selOption]);
                yoff[selOption] = zoomOffset(optionscales[selOption]);

            } else {

                optionscales[selOption] = 1 + (1 - progress) * (maxZoom - 1);
                xoff[selOption] = zoomOffset(optionscales[selOption]);
                yoff[selOption] = zoomOffset(optionscales[selOption]);
            }

            if (progress == 1) {
                oy = y;
                animationStarted = 0;
                if (active) {

                    optionscales[selOption] = 1 + (maxZoom - 1);
                    xoff[selOption] = zoomOffset(optionscales[selOption]);
                    yoff[selOption] = zoomOffset(optionscales[selOption]);

                } else {

                    optionscales[selOption] = 1;
                    xoff[selOption] = zoomOffset(optionscales[selOption]);
                    yoff[selOption] = zoomOffset(optionscales[selOption]);
                }
            }
        } else {
            // transition between menu options
            if (direction == 0) {
                float progress = time - animationStarted;
                progress = progress / (duration * 1.0f);
                if (progress > 1)
                    progress = 1;
                if (progress < 0)
                    progress = 0;
                progress = easeOutCubic(progress);

                x = ox + progress * ICON_GAP;

                optionscales[selOption] = 1 + (1 - progress) * (maxZoom - 1);
                xoff[selOption] = zoomOffset(optionscales[selOption]);
                yoff[selOption] = zoomOffset(optionscales[selOption]);

                if (progress >= 1.0f) {
                    optionscales[selOption] = 1.0;
                    xoff[selOption] = 0;
                    yoff[selOption] = 0;

                    selOption--;
                    optionscales[selOption] = maxZoom;
                    xoff[selOption] = zoomOffset(optionscales[selOption]);
                    yoff[selOption] = zoomOffset(optionscales[selOption]);

                    x = ox + ICON_GAP;
                    animationStarted = 0;
                    ox = x;
                }
            } else {
                float progress = time - animationStarted;
                progress = progress / (duration * 1.0f);
                if (progress > 1)
                    progress = 1;
                if (progress < 0)
                    progress = 0;
                progress = easeOutCubic(progress);

                x = ox - progress * ICON_GAP;

                optionscales[selOption] = 1 + progress * (maxZoom - 1);
                xoff[selOption] = zoomOffset(optionscales[selOption]);
                yoff[selOption] = zoomOffset(optionscales[selOption]);

                if (progress >= 1.0f) {
                    optionscales[selOption] = 1.0;
                    xoff[selOption] = 0;
                    yoff[selOption] = 0;
                    selOption++;
                    optionscales[selOption] = maxZoom;
                    xoff[selOption] = zoomOffset(optionscales[selOption]);
                    yoff[selOption] = zoomOffset(optionscales[selOption]);
                    x = ox - ICON_GAP;
                    animationStarted = 0;
                    ox = x;
                }
            }
        }
    }
}

//*******************************
// PsMenu::render
//*******************************
void PsMenu::render() {
    static const float slots[4] = {0, ICON_GAP, ICON_GAP * 2, ICON_GAP * 3};
    const ableem::Texture *icons[4] = {&settings, &guide, &memcard, &savestate};
    const ableem::Rect input(0, 0, 118, 118);
    // the Resume icon greyed when the selected game has no resume points - still drawn, still selectable
    const unsigned char resumeAlpha = resumeAvailable ? 255 : 120;
    savestate.setAlphaMod(resumeAlpha);
    resume.setAlphaMod(resumeAlpha);
    for (int i = 0; i < 4; i++) {
        if (i > 0 && !enabled[i])
            continue;
        const float left = x + slots[i] + xoff[i];
        const float top = y + yoff[i];
        const float size = 118 * optionscales[i];
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
