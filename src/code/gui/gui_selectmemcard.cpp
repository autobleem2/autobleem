//
// Created by screemer on 2019-01-25.
//

#include "gui_selectmemcard.h"

#include <string>
#include "gui.h"
#include "../main.h"
#include "gui_confirm.h"
#include "gui_keyboard.h"
#include "../lang.h"
using namespace std;

//*******************************
// GuiSelectMemcard::init
//*******************************
void GuiSelectMemcard::init() {
    cards.clear();

    shared_ptr<Gui> gui(Gui::getInstance());
    MemcardManager memcardOps(gui->pathToGamesDir);
    if (listType==MC_CUSTOM) {
        cards = memcardOps.list();
    } else
    {
        cards.push_back(_("CONFIGURED"));
        // build memcards list
        vector<string> customList = memcardOps.list();
        for (const string& mc:customList)
        {
            cards.push_back("[1] "+mc);
            cards.push_back("[2] "+mc);
        }


    }
    maxVisible = atoi(gui->themeData.values["lines"].c_str());
    firstVisible = 0;
    lastVisible = firstVisible + maxVisible;

    if (!cardSelected.empty()) {
        for (int i = 0; i < cards.size(); i++) {
            if (cards[i] == cardSelected) {
                selected = i + 1;
            }
        }
    }

    if (listType==MC_CUSTOM) {
        vector<string>::iterator it;
        it = cards.begin();
        cards.insert(it, string("(" + _("Internal") + ")"));
    }
}

//*******************************
// GuiSelectMemcard::render
//*******************************
void GuiSelectMemcard::render() {
    shared_ptr<Gui> gui(Gui::getInstance());
    gui->renderBackground();
    gui->renderTextBar();
    int yoffset = gui->renderLogo(true);
    gui->renderTextLine("-=" + _("Select memory card") + "=-", 0, yoffset, XALIGN_CENTER);

    if (selected >= cards.size()) {
        selected = cards.size() - 1;
    }

    if (selected < firstVisible) {
        firstVisible--;
        lastVisible--;
    }
    if (selected >= lastVisible) {
        firstVisible++;
        lastVisible++;
    }

    int pos = 1;
    for (int i = firstVisible; i < lastVisible; i++) {
        if (i >= cards.size()) {
            break;
        }
        gui->renderTextLine(cards[i], pos, yoffset);
        pos++;
    }

    if (!cards.size() == 0) {
        gui->renderSelectionBox(selected - firstVisible + 1, yoffset);
    }

    gui->renderStatus(_("Card") + " " + to_string(selected + 1) + "/" + to_string(cards.size()) +
                      "   |@L1|/|@R1| " + _("Page") + "     |@X| " + _("Select") + "  |@O| " + _("Cancel") + "|");
    renderer.present();
}

//*******************************
// GuiSelectMemcard::loop
//*******************************
void GuiSelectMemcard::loop() {
    shared_ptr<Gui> gui(Gui::getInstance());
    bool menuVisible = true;
    while (menuVisible) {
        Event e;
        while (gui->input().poll(e)) {
            // this is for pc Only
            if (e.type == Event::Type::Quit) {
                menuVisible = false;
            }
            switch (e.type) {
                case Event::Type::DpadDown:
                case Event::Type::DpadUp:
                    if (gui->input().dpadDown()) {

                            gui->cursor.play();
                            selected++;
                            if (selected >= cards.size()) {
                                selected = 0;
                                firstVisible = selected;
                                lastVisible = firstVisible + maxVisible;
                            }
                            render();
                        }
                    if (gui->input().dpadUp()) {

                            gui->cursor.play();
                            selected--;
                            if (selected < 0) {
                                selected = cards.size() - 1;
                                firstVisible = selected;
                                lastVisible = firstVisible + maxVisible;
                            }
                            render();
                        }

                    break;
                case Event::Type::ButtonDown:
                    if (e.button == Button::R1) {

                        gui->home_up.play();
                        selected += maxVisible;
                        if (selected >= cards.size()) {
                            selected = cards.size() - 1;
                        }
                        firstVisible = selected;
                        lastVisible = firstVisible + maxVisible;
                        render();
                    };
                    if (e.button == Button::L1) {

                        gui->home_down.play();
                        selected -= maxVisible;
                        if (selected < 0) {
                            selected = 0;
                        }
                        firstVisible = selected;
                        lastVisible = firstVisible + maxVisible;
                        render();
                    };

                    if (e.button == Button::Circle) {

                        gui->cancel.play();
                        selected = -1;
                        menuVisible = false;

                    };
                    if (e.button == Button::Cross) {
                        cardSelected = cards[selected];
                        gui->cursor.play();
                        menuVisible = false;
                    };
                    break;
                default:
                    break;
            }
        }
    }
}