//
// Created by screemer on 2019-02-22.
//

#include "evoui_stateselector.h"
#include "../../gui/gui.h"
#include "../screens/evoui_launcher.h"
#include "../../app.h"

using namespace std;

//*******************************
// PsStateSelector::cleanSaveStateImages
//*******************************
void PsStateSelector::cleanSaveStateImages() {
    for (auto &i : slotImg)
        i = ableem::Texture();
}

//*******************************
// PsStateSelector::loadSaveStateImages
//*******************************
void PsStateSelector::loadSaveStateImages(PsGamePtr &game, bool saving) {
    if (saving) {
        operation = OP_SAVE;
    } else {
        operation = OP_LOAD;
    }
    for (int i = 0; i < 4; i++) {
        slotImg[i] = ableem::Texture();
        slotActive[i] = false;
        if (!saving) {
            if (App::get().resumePoints().slotIsActive(*game, i)) {
                slotImg[i] = ableem::Texture::loadFile(renderer, App::get().resumePoints().pictureForSlot(*game, i));
                slotActive[i] = true;
            }
        } else {
            if (App::get().resumePoints().slotIsActive(*game, i)) {
                slotImg[i] = ableem::Texture::loadFile(renderer, App::get().resumePoints().pictureForSlot(*game, i));
            }
            slotActive[i] = true;
        }
    }
}

//*******************************
// PsStateSelector::render
//*******************************
void PsStateSelector::render() {
    if (visible) {
        float scale = 2.7f;
        x = 10;
        y = 220;
        renderer.setDrawColor(ableem::Color(0, 0, 0, 200));
        ableem::Rect rect;
        rect.x = 0;
        rect.y = 100;
        rect.w = SCREEN_WIDTH;
        rect.h = SCREEN_HEIGHT - 200;
        renderer.fillRect(rect);

        int w = 118 * scale;
        int h = 118 * scale;
        ableem::Rect input, output;
        input.x = 0, input.y = 0;
        input.h = 118, input.w = 118;
        output.x = x;
        output.y = y;
        output.w = w;
        output.h = h;

        string text = _("SELECT RESUME SLOT TO LOAD");

        if (operation == OP_SAVE) {
            text = _("SELECT SLOT TO SAVE STATE");
        }

        shared_ptr<Gui> gui(Gui::getInstance());

        // where the picture goes on the resume icon, in the icon's pixels - the theme's say (ab2 centres a
        // screen on its tile), else the original frame's window; the same rect the menu row uses
        ableem::Rect window{25, 33, 68, 52};
        const auto &icons = App::get().theme().launcher().menuIcons;
        if (icons.resumePicture.set) {
            window = ableem::Rect(icons.resumePicture.x, icons.resumePicture.y, icons.resumePicture.w,
                                  icons.resumePicture.h);
        }

        gui->text().renderText_WithColor(font30, text, 0, 110, brightWhite, XALIGN_CENTER); // translated above

        if (operation == OP_LOAD) {
            gui->text().renderText(
                font24, "|@T| " + _("Delete") + "     |@X| " + _("Select") + "     |@O| " + _("Cancel") + "|", 0, 150,
                XALIGN_CENTER);
        } else {
            gui->text().renderText(font24, "|@X| " + _("Select") + "     |@O| " + _("Cancel") + "|", 0, 150,
                                   XALIGN_CENTER);
        }

        // the selected slot: with the theme's selection colour, the other tiles go dim and the selected
        // one keeps its colours behind a halo in that colour - the tile's own shape, a little larger,
        // added twice behind it, so it follows whatever the theme's tile looks like; a theme without the
        // colour gets the original red tint of the selected tile (which only shows on a white tile)
        const ThemeColor &selection = App::get().theme().launcher().colors.selection;
        const ableem::Color white(255, 255, 255);

        for (int i = 0; i < 4; i++) {
            output.x = x + (118 * scale) * i;
            input = ableem::Rect(0, 0, 118, 118);

            if (selSlot == i && selection.set) {
                frame.setBlendMode(ableem::BlendMode::Add);
                frame.setColorMod(TextRenderer::toColor(selection, 255));
                for (int ring = 2; ring >= 1; ring--) {
                    int grow = static_cast<int>(ring * 3 * scale + 0.5f);
                    ableem::Rect halo(output.x - grow, output.y - grow, output.w + 2 * grow, output.h + 2 * grow);
                    frame.setAlphaMod(ring == 2 ? 70 : 120);
                    renderer.copy(frame, &input, &halo);
                }
                frame.setAlphaMod(255);
                frame.setBlendMode(ableem::BlendMode::Blend);
                frame.setColorMod(white);
            } else if (selSlot == i) {
                frame.setColorMod(ableem::Color(255, 128, 128));
            } else if (selection.set) {
                frame.setColorMod(ableem::Color(120, 120, 120));
            } else {
                frame.setColorMod(white);
            }
            renderer.copy(frame, &input, &output);
            frame.setColorMod(white);

            if (slotImg[i].valid()) {
                ableem::Size s = slotImg[i].size();

                input.x = 0;
                input.y = 0;
                input.w = s.w;
                input.h = s.h;
                ableem::Rect imgOut;
                imgOut.x = output.x + window.x * scale;
                imgOut.y = y + window.y * scale;
                imgOut.w = window.w * scale;
                imgOut.h = window.h * scale;

                renderer.copy(slotImg[i], &input, &imgOut);
            }

            // the slot's name where the theme puts it on the icon; the original spot otherwise - which is
            // exactly where it always was (x + 60, 270 on the 2.7x tile)
            ableem::Rect label{22, 18, 0, 0};
            if (icons.resumeSlotLabel.set)
                label = ableem::Rect(icons.resumeSlotLabel.x, icons.resumeSlotLabel.y, 0, 0);
            gui->text().renderText_WithColor(font24, _("Slot") + " " + to_string(i + 1),
                                             output.x + static_cast<int>(label.x * scale + 0.5f),
                                             y + static_cast<int>(label.y * scale + 0.5f), brightWhite);
        }
    }
}
