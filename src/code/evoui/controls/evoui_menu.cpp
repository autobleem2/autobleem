//
// Created by screemer on 2/16/19.
//

#include "evoui_menu.h"
#include "core/model/timing.h"
using namespace std;

//*******************************
// PsMenu::PsMenu
//*******************************
PsMenu::PsMenu(string name1, const LauncherTheme::MenuIcons &icons) : PsObj(name1, "") {
    settings = ableem::Texture::loadFile(renderer, icons.settings);
    guide = ableem::Texture::loadFile(renderer, icons.guide);
    memcard = ableem::Texture::loadFile(renderer, icons.memcard);
    savestate = ableem::Texture::loadFile(renderer, icons.resume);
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
        const int grown = static_cast<int>(118 * maxZoom) - 118;
        xoff[selOption] = -(grown / 2);
        yoff[selOption] = -(grown / 2);
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
                int neww = 118 * optionscales[selOption];
                int newh = 118 * optionscales[selOption];

                xoff[selOption] = -((neww - 118) / 2);
                yoff[selOption] = -((newh - 118) / 2);

            } else {

                optionscales[selOption] = 1 + (1 - progress) * (maxZoom - 1);
                int neww = 118 * optionscales[selOption];
                int newh = 118 * optionscales[selOption];

                xoff[selOption] = -((neww - 118) / 2);
                yoff[selOption] = -((newh - 118) / 2);
            }

            if (progress == 1) {
                oy = y;
                animationStarted = 0;
                if (active) {

                    optionscales[selOption] = 1 + (maxZoom - 1);
                    int neww = 118 * optionscales[selOption];
                    int newh = 118 * optionscales[selOption];

                    xoff[selOption] = -((neww - 118) / 2);
                    yoff[selOption] = -((newh - 118) / 2);

                } else {

                    optionscales[selOption] = 1;
                    int neww = 118 * optionscales[selOption];
                    int newh = 118 * optionscales[selOption];

                    xoff[selOption] = -((neww - 118) / 2);
                    yoff[selOption] = -((newh - 118) / 2);
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
                int neww = 118 * optionscales[selOption];
                int newh = 118 * optionscales[selOption];

                xoff[selOption] = -((neww - 118) / 2);
                yoff[selOption] = -((newh - 118) / 2);

                if (progress >= 1.0f) {
                    optionscales[selOption] = 1.0;
                    xoff[selOption] = 0;
                    yoff[selOption] = 0;

                    selOption--;
                    optionscales[selOption] = maxZoom;
                    int neww = 118 * optionscales[selOption];
                    int newh = 118 * optionscales[selOption];

                    xoff[selOption] = -((neww - 118) / 2);
                    yoff[selOption] = -((newh - 118) / 2);

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
                int neww = 118 * optionscales[selOption];
                int newh = 118 * optionscales[selOption];

                xoff[selOption] = -((neww - 118) / 2);
                yoff[selOption] = -((newh - 118) / 2);

                if (progress >= 1.0f) {
                    optionscales[selOption] = 1.0;
                    xoff[selOption] = 0;
                    yoff[selOption] = 0;
                    selOption++;
                    optionscales[selOption] = maxZoom;
                    int neww = 118 * optionscales[selOption];
                    int newh = 118 * optionscales[selOption];

                    xoff[selOption] = -((neww - 118) / 2);
                    yoff[selOption] = -((newh - 118) / 2);
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
    int w = 118 * optionscales[0];
    int h = 118 * optionscales[0];
    ableem::Rect input, output;
    input.x = 0, input.y = 0;
    input.h = 118, input.w = 118;
    output.x = x + xoff[0];
    output.y = y + yoff[0];
    output.w = w;
    output.h = h;

    renderer.copy(settings, &input, &output);

    if (enabled[1]) {
        w = 118 * optionscales[1];
        h = 118 * optionscales[1];

        input.x = 0, input.y = 0;
        input.h = 118, input.w = 118;
        output.x = x + 130 + xoff[1];
        output.y = y + yoff[1];
        output.w = w;
        output.h = h;

        renderer.copy(guide, &input, &output);
    }

    if (enabled[2]) {
        w = 118 * optionscales[2];
        h = 118 * optionscales[2];
        input.x = 0, input.y = 0;
        input.h = 118, input.w = 118;
        output.x = x + 260 + xoff[2];
        output.y = y + yoff[2];
        output.w = w;
        output.h = h;

        renderer.copy(memcard, &input, &output);
    }

    if (enabled[3]) {
        w = 118 * optionscales[3];
        h = 118 * optionscales[3];
        input.x = 0, input.y = 0;
        input.h = 118, input.w = 118;
        output.x = x + 130 * 3 + xoff[3];
        output.y = y + yoff[3];
        output.w = w;
        output.h = h;

        renderer.copy(savestate, &input, &output);

        if (resume.valid()) {
            ableem::Size s = resume.size();
            input.h = s.h;
            input.w = s.w;
            output.x = x + 130 * 3 + resumePicture.x * optionscales[3] + xoff[3];
            output.y = y + yoff[3] + resumePicture.y * optionscales[3];
            output.w = resumePicture.w * optionscales[3];
            output.h = resumePicture.h * optionscales[3];
            renderer.copy(resume, &input, &output);
        }
    }
}

//*******************************
// PsMenu::setResumePic
//*******************************
void PsMenu::setResumePic(string picturePath) {
    resume = ableem::Texture::loadFile(renderer, picturePath);
}
