//
// Created by screemer on 2019-02-22.
//

#include "evoui_stateselector.h"
#include "../../gui/gui.h"
#include "../gui_launcher.h"
#include "../../app.h"

using namespace std;

//*******************************
// PsStateSelector::cleanSaveStateImages
//*******************************
void PsStateSelector::cleanSaveStateImages()
{
    for (int i=0;i<4;i++)
        slotImg[i] = ableem::Texture();
}

//*******************************
// PsStateSelector::loadSaveStateImages
//*******************************
void PsStateSelector::loadSaveStateImages(PsGamePtr & game, bool saving)
{
    if (saving)
    {
        operation = OP_SAVE;
    } else
    {
        operation = OP_LOAD;
    }
    for (int i=0;i<4;i++)
    {
        slotImg[i] = ableem::Texture();
        slotActive[i]=false;
        if (!saving) {
            if (App::get().resumePoints().slotIsActive(*game, i)) {
                slotImg[i] = ableem::Texture::loadFile(renderer, App::get().resumePoints().pictureForSlot(*game, i));
                slotActive[i]=true;
            }
        } else
        {
            if (App::get().resumePoints().slotIsActive(*game, i)) {
                slotImg[i] = ableem::Texture::loadFile(renderer, App::get().resumePoints().pictureForSlot(*game, i));

            }
            slotActive[i]=true;
        }
    }
}

//*******************************
// PsStateSelector::render
//*******************************
void PsStateSelector::render()
{
    if (visible)
    {
        float scale = 2.7f;
        x=10;
        y=220;
        renderer.setDrawColor(ableem::Color(0,0,0,200));
        ableem::Rect rect;
        rect.x=0;
        rect.y=100;
        rect.w=SCREEN_WIDTH;
        rect.h=SCREEN_HEIGHT-200;
        renderer.fillRect(rect);

        int w = 118 * scale;
        int h = 118 * scale;
        ableem::Rect input, output;
        input.x = 0, input.y = 0;
        input.h = 118, input.w = 118;
        output.x = x ;
        output.y = y ;
        output.w = w;
        output.h = h;

        string text = _("SELECT RESUME SLOT TO LOAD");

        if (operation==OP_SAVE)
        {
            text = _("SELECT SLOT TO SAVE STATE");
        }

        shared_ptr<Gui> gui(Gui::getInstance());

        gui->text().renderText_WithColor(font30, _(text), 0, 110, brightWhite, XALIGN_CENTER);

        if (operation==OP_LOAD) {
            gui->text().renderText(font24, "|@T| " + _("Delete") + "     |@X| " + _("Select") + "     |@O| " + _("Cancel") +
                                    "|", 0, 150, XALIGN_CENTER);
        } else
        {
            gui->text().renderText(font24, "|@X| " + _("Select") + "     |@O| " + _("Cancel") +
                                    "|", 0, 150, XALIGN_CENTER);
        }

        for (int i=0;i<4;i++)
        {
            output.x = x+(118*scale)*i;

            if (selSlot==i)
            {
                frame.setColorMod(ableem::Color(255, 128, 128));
            } else
            {
                frame.setColorMod(ableem::Color(255, 255, 255));
            }
            renderer.copy(frame, &input, &output);
            frame.setColorMod(ableem::Color(255, 255, 255));


            if (slotImg[i].valid())
            {
                ableem::Size s = slotImg[i].size();

                input.x=0; input.y=0; input.w=s.w; input.h=s.h;
                ableem::Rect imgOut;
                imgOut.x = x+(118*scale)*i + 67;
                imgOut.y = y+90;
                imgOut.w=184;
                imgOut.h=140;

                renderer.copy(slotImg[i], &input, &imgOut);
            }

            gui->text().renderText_WithColor(font24, _("Slot") + " " + to_string(i+1), output.x + 60, 270, brightWhite);
        }
    }
}
