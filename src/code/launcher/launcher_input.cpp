//
// GuiLauncher, the input half: the event loop and what each stick move and button does in each state.
// The handlers that open another screen or start a game are in launcher_actions.cpp.
//
#include "gui_launcher.h"
#include "../gui/gui.h"
#include "../gui/gui_confirm.h"
#include "gui_btn_guide.h"

#include <algorithm>
#include <iostream>

using namespace std;

//*******************************
// GuiLauncher::loop
// event loop
//*******************************
void GuiLauncher::loop() {
    cout << "Main Loop" << endl;
    powerOffShift = false;  // L2 shift used for power off and selecting game sub dir or RA playlist

    menuVisible = true;
    motionStart = 0;
    timespeed = 0;
    motionDir = 0;

    while (menuVisible) {
        // get the current translated string values
        headers = {_("SETTINGS"), _("GAME"), _("MEMORY CARD"), _("RESUME")};
        texts = {_("Customize AutoBleem settings"), _("Edit game parameters"),
                 _("Edit Memory Card information"), _("Resume game from saved state point")};

        time = gui->platform().ticks();
        for (auto &obj : staticElements) {
            obj->update(time);
        }

        menu->update(time);
        carousel.updatePositions();
        render();

        if (motionStart != 0) {
            long timePressed = time - motionStart;
            if (timePressed > 300) {
                if (time - timespeed > 100) {
                    if (motionDir == 0) {
                        if (!carousel.scrolling) {
                            nextCarouselGame(60);
                        }
                    } else {
                        if (!carousel.scrolling) {
                            prevCarouselGame(60);
                        }
                    }
                    timespeed = time;
                }
                if (timespeed == 0) {
                    timespeed = time;
                }
            } else {
                timespeed = 0;
            }
        }

        while (gui->input().poll(e)) {
            // this is for pc Only
            if (e.type == Event::Type::Quit) {
                menuVisible = false;
            }
            switch (e.type) {
                case Event::Type::KeyDown:
                    // the power button / Esc case is already handled inside Input::poll (see
                    // Platform::setPowerOffHandler, wired up once in main.cpp)
                    break;
                case Event::Type::DpadDown:  /* Handle Joystick Motion */
                case Event::Type::DpadUp:
                    if (powerOffShift)
                        continue;
                    if (gui->input().dpadCentered()) {
                        if (state == LauncherScreenState::Games) {
                            if (carousel.games.empty()) {
                                continue;
                            }
                        }
                        motionStart = 0;
                    }
                    if (gui->input().dpadLeft()) {
                        loop_joyMoveLeft();
                    }
                    else if (gui->input().dpadRight()) {
                        loop_joyMoveRight();
                    }
                    else if (gui->input().dpadUp()) {
                        loop_joyMoveUp();
                    }
                    else if (gui->input().dpadDown()) {
                        loop_joyMoveDown();
                    }
                    else {
                        ;
                    }
                    break;

                case Event::Type::ButtonDown:
                    loop_joyButton_Pressed();    // button pressed
                    break;
                case Event::Type::ButtonUp:
                    loop_joyButtonReleased();   // button released
                    break;
                default:
                    break;

            }   // switch (e.type)
        // end while (gui->input().poll(e))
        }

        { // no event.  see if we're holding down L1 or R1 for fast forward first letter
            if (L1_isPressedForFastForward) {
                unsigned int timePressed = (gui->platform().ticks() - L1_fastForwardTimeStart);
                if (timePressed > prevNextFastForwardTimeLimit) {
                    loop_prevGameFirstLetter();
                }
            }

            if (R1_isPressedForFastForward) {
                unsigned int timePressed = (gui->platform().ticks() - R1_fastForwardTimeStart);
                if (timePressed > prevNextFastForwardTimeLimit) {
                    loop_nextGameFirstLetter();
                }
            }
        }
    }   // while (menuVisible)

    freeAssets();
}

//*******************************
// GuiLauncher::loop_joyMoveLeft
//*******************************
void GuiLauncher::loop_joyMoveLeft() {
    if (state == LauncherScreenState::Games) {
        if (carousel.games.empty()) {
            return;
        }
        if (!carousel.scrolling) {
            motionStart = time;
            motionDir = 1;
            carousel.scrolling = true;
            prevCarouselGame(110);
        }
    } else if (state == LauncherScreenState::Set) {

        if (!menu->foreign) {
            if (!selOptionIs(menu->selOption, LauncherMenuOption::AbSettings)) {
                if (menu->animationStarted == 0) {
                    app.audio().cursor.play();
                    menu->transition = TR_OPTION;
                    menu->direction = 0;
                    menu->duration = 100;
                    menuHead->setText(headers[menu->selOption - 1], fgColor);
                    menuText->setText(texts[menu->selOption - 1], fgColor);
                    menu->animationStarted = time;
                }
            }
        }

    } else if (state == LauncherScreenState::Resume) {
        if (sselector->selSlot != 0) {
            app.audio().cursor.play();
            sselector->selSlot--;
        }
    }
}

//*******************************
// GuiLauncher::loop_joyMoveRight
//*******************************
void GuiLauncher::loop_joyMoveRight() {
    if (state == LauncherScreenState::Games) {
        if (carousel.games.empty()) {
            return;
        }
        if (!carousel.scrolling) {
            motionStart = time;
            motionDir = 0;
            carousel.scrolling = true;
            nextCarouselGame(110);
        }
    } else if (state == LauncherScreenState::Set) {

        if (!menu->foreign) {
            if (!selOptionIs(menu->selOption, LauncherMenuOption::ResumeFromSavestate)) {
                if (menu->animationStarted == 0) {
                    app.audio().cursor.play();
                    menu->transition = TR_OPTION;
                    menu->direction = 1;
                    menu->duration = 100;
                    menuHead->setText(headers[menu->selOption + 1], fgColor);
                    menuText->setText(texts[menu->selOption + 1], fgColor);
                    menu->animationStarted = time;
                }
            }
        }

    } else if (state == LauncherScreenState::Resume) {
        if (sselector->selSlot != 3) {
            app.audio().cursor.play();
            sselector->selSlot++;
        }
    }
}

//*******************************
// GuiLauncher::loop_joyMoveUp
//*******************************
void GuiLauncher::loop_joyMoveUp() {
    if (carousel.scrolling) {
        return;
    }
    if (state == LauncherScreenState::Set) {
        if (menu->animationStarted == 0) {
            menu->transition = TR_MENUON;
            switchState(LauncherScreenState::Games, time);
            motionStart = 0;
        }
    }
}

//*******************************
// GuiLauncher::loop_joyMoveDown
//*******************************
void GuiLauncher::loop_joyMoveDown() {
    if (carousel.scrolling) {
        return;
    }
    if (state == LauncherScreenState::Games) {
        if (menu->animationStarted == 0) {
            menu->transition = TR_MENUON;
            switchState(LauncherScreenState::Set, time);
            motionStart = 0;
        }
    }
}

//*******************************
// GuiLauncher::loop_joyButtonPressed
// button pressed
//*******************************
void GuiLauncher::loop_joyButton_Pressed() {
    if (e.button == Button::L2) {
        app.audio().cursor.play();
        powerOffShift = true;
    }

    if (powerOffShift) {
        if (e.button == Button::R2) {
            app.audio().cursor.play();
            gui->drawText(_("POWERING OFF... PLEASE WAIT"));
            Util::powerOff();
            return;
        }
    }


    if (e.button == Button::Select) {
        loop_selectButton_Pressed();
    };

    if (e.button == Button::Start) {
        loop_startButton_Pressed();
    };

    if (powerOffShift)
        return; // none of the following buttons should work if L2 is pressed

    if (e.button == Button::L1) {
        L1_isPressedForFastForward = true;
        loop_prevGameFirstLetter();

    } else if (e.button == Button::R1) {
        R1_isPressedForFastForward = true;
        loop_nextGameFirstLetter();

    } else if (e.button == Button::Circle) {
        loop_circleButton_Pressed();

    } else if (e.button == Button::Triangle) {
        loop_triangleButton_Pressed();

    } else if (e.button == Button::Square) {
        loop_squareButton_Pressed();

    } else if (e.button == Button::Cross) {
        loop_crossButton_Pressed();
    };
}

//*******************************
// GuiLauncher::loop_prevNextGameFirstLetter
//*******************************
void GuiLauncher::loop_prevNextGameFirstLetter(bool next) {  // false is prev, true is next
    app.audio().cursor.play();

    if (state == LauncherScreenState::Games) {
        if (carousel.games.empty()) {
            return;
        }

        if (carousel.games[carousel.selected]->title == "") {
            return;
        }

        // find the index of all the first letters
        map<char, int> firstLetterToIndex;
        for (int index = 0; index < carousel.games.size() ; ++index) {
            if (carousel.games[index]->title != "") {
                char firstLetter = toupper(carousel.games[index]->title[0]);
                if (firstLetterToIndex.find(firstLetter) == firstLetterToIndex.end())   // if letter not in map
                    firstLetterToIndex[firstLetter] = index;    // add the first letter to the map
            }
        }

        if (firstLetterToIndex.size() == 0)
            return; // nothing with a title

        if (carousel.selectedIsValid()) {
            char currentLetter = toupper(carousel.games[carousel.selected]->title[0]);
            int nextGame = carousel.selected;
            if (firstLetterToIndex.size() == 1) {
                nextGame = firstLetterToIndex[currentLetter];   // there is only one first letter in the games
            } else {
                auto iter = firstLetterToIndex.find(currentLetter);
                if (next) {
                    if (firstLetterToIndex.upper_bound(currentLetter) == firstLetterToIndex.end()) // if this is the last letter
                        nextGame = firstLetterToIndex.begin()->second;  // wrap around to first letter
                    else
                        nextGame = (++iter)->second;                    // next letter
                } else {
                    if (iter == firstLetterToIndex.begin())             // if this is the first letter
                        nextGame = firstLetterToIndex.rbegin()->second;  // wrap around to last letter
                    else
                        nextGame = (--iter)->second;                    // prev letter
                }
            }

            if (nextGame != carousel.selected) {
                // we have prev/next game first letter;
                carousel.selected = nextGame;
                app.audio().cursor.play();
                notificationLines[1].setText(toUpperCopy(carousel.games[carousel.selected]->title.substr(0,1)),
                                             DefaultShowingTimeout, brightWhite, FONT_22_MED);
                carousel.setInitialPositions(carousel.selected);
                updateMeta();
                menu->setResumePic(app.resumePoints().lastPicture(*carousel.games[carousel.selected]));
            } else {
                // no change
                app.audio().cancel.play();
                notificationLines[1].setText(toUpperCopy(carousel.games[carousel.selected]->title.substr(0,1)),
                                             DefaultShowingTimeout, brightWhite, FONT_22_MED);
            }
        }
    }
}

//*******************************
// GuiLauncher::loop_prevGameFirstLetter
//*******************************
void GuiLauncher::loop_prevGameFirstLetter() {
    loop_prevNextGameFirstLetter(false);
    L1_fastForwardTimeStart = gui->platform().ticks();
};

//*******************************
// GuiLauncher::loop_nextGameFirstLetter
//*******************************
void GuiLauncher::loop_nextGameFirstLetter()
{
    loop_prevNextGameFirstLetter(true);
    R1_fastForwardTimeStart = gui->platform().ticks();
};

//*******************************
// GuiLauncher::loop_selectButtonPressed
//*******************************
void GuiLauncher::loop_selectButton_Pressed() {
    if (state == LauncherScreenState::Games) {
        if (powerOffShift) {
            if (selection.set == GameSet::PS1)
                loop_chooseGameDir();
            else if (selection.set == GameSet::RetroArch)
                loop_chooseRAPlaylist();
            else
                return; // if L2 is pressed then Select should only work if current_set is PS1 or RetroArch
        }
        else {
            // switch to next Select Mode
            app.audio().cursor.play();

            GameSet previousSet = selection.set;
            selection.set = nextGameSet(selection.set);
            if (previousSet == GameSet::Apps) {
                showAllOptions();
                menuHead->setText(headers[0], fgColor);
                menuText->setText(texts[0], fgColor);
            }

            switchSet(selection.set,false);
            showSetName();
            if (carousel.selected != -1 && carousel.selectedIsValid()) {
                updateMeta();
                menu->setResumePic(app.resumePoints().lastPicture(*carousel.games[carousel.selected]));
            } else {
                updateMeta();
            }
        }
    }
}

//*******************************
// GuiLauncher::loop_startButton_Pressed
// pick a random game
//*******************************
void GuiLauncher::loop_startButton_Pressed() {
    app.audio().cursor.play();

    if (state == LauncherScreenState::Games) {
        if (carousel.games.empty()) {
            return;
        }

        carousel.selected = Util::getRandomIndex(carousel.games.size());
        if (carousel.selectedIsValid()) {
            app.audio().cursor.play();
            carousel.setInitialPositions(carousel.selected);
            updateMeta();
            menu->setResumePic(app.resumePoints().lastPicture(*carousel.games[carousel.selected]));
        }
    }
}

//*******************************
// GuiLauncher::loop_circleButtonPressed
//*******************************
void GuiLauncher::loop_circleButton_Pressed() {
    if (state == LauncherScreenState::Set) {
        if (menu->animationStarted == 0) {
            menu->transition = TR_MENUON;
            switchState(LauncherScreenState::Games, time);
            motionStart = 0;
        }
    } else if (state == LauncherScreenState::Games) {
        app.audio().cancel.play();
        menuVisible = false;
    } else if (state == LauncherScreenState::Resume) {
        app.audio().cursor.play();
        sselector->visible = false;
        arrow->visible = true;
        sselector->cleanSaveStateImages();
        if (carousel.selectedIsValid())
            menu->setResumePic(app.resumePoints().lastPicture(*carousel.games[carousel.selected]));

        if (sselector->operation == OP_LOAD) {
            state = LauncherScreenState::Set;
        } else {
            state = LauncherScreenState::Games;
        }
    }
}

//*******************************
// GuiLauncher::loop_triangleButtonPressed
//*******************************
void GuiLauncher::loop_triangleButton_Pressed() {
    if (state != LauncherScreenState::Resume) {
        app.audio().cursor.play();
        GuiBtnGuide guide(*gui);
        guide.backgroundImg = background->tex;
        guide.show();
    } else {
        if (sselector->operation == OP_LOAD) {
            if (carousel.selectedIsValid()) {
                auto game = carousel.games[carousel.selected];
                int slot = sselector->selSlot;
                if (app.resumePoints().slotIsActive(*game, slot)) {
                    app.audio().cursor.play();

                    GuiConfirm confirm(*gui);
                    confirm.label = _("Are you sure?");
                    confirm.show();

                    if (confirm.result) {
                        app.resumePoints().removeSlot(*game, slot);
                    }
                    sselector->cleanSaveStateImages();
                    sselector->loadSaveStateImages(carousel.games[carousel.selected], false);
                    state = LauncherScreenState::Resume;
                    sselector->selSlot = 0;
                    sselector->operation = OP_LOAD;
                }
            } else {
                app.audio().cancel.play();
            }
        }
    }
}

//*******************************
// GuiLauncher::loop_squareButtonPressed
//*******************************
void GuiLauncher::loop_squareButton_Pressed() {


    if (DirEntry::exists(Env::getPathToRetroarchDir() + sep + "retroarch")) { // retroarch is a file!!

        if (state == LauncherScreenState::Games) {
            if (carousel.games.empty()) {
                return;
            }
            if (carousel.selectedIsValid() && carousel.games[carousel.selected]->foreign) {
                return;
            }
            app.session().startingGame = true;
            if (carousel.selectedIsValid()) {
                app.session().runningGame = carousel.games[carousel.selected];
                app.gameCatalog().recordGamePlayed(app.session().runningGame);
            }
            app.session().resumePoint = -1;
            rememberSelection();
            menuVisible = false;

            app.session().emuMode = EmuMode::RetroArch;
        }
    }
}

//*******************************
// GuiLauncher::loop_crossButtonPressed
//*******************************
void GuiLauncher::loop_crossButton_Pressed() {


    if (state == LauncherScreenState::Games) {
        loop_crossButtonPressed_STATE_GAMES();

    } else if (state == LauncherScreenState::Set) {
        loop_crossButtonPressed_STATE_SET();

    } else if (state == LauncherScreenState::Resume) {
        loop_crossButtonPressed_STATE_RESUME();

    }
}

//*******************************
// GuiLauncher::loop_crossButtonPressed_STATE_SET
//*******************************
void GuiLauncher::loop_crossButtonPressed_STATE_SET() {
    app.session().resumingGui = false;
    if (selOptionIs(menu->selOption, LauncherMenuOption::ResumeFromSavestate)) {
        loop_crossButtonPressed_STATE_SET__OPT_RESUME_FROM_SAVESTATE();
    }
    else if (selOptionIs(menu->selOption, LauncherMenuOption::EditMemcardInfo)) {
        loop_crossButtonPressed_STATE_SET__OPT_EDIT_MEMCARD();
    }
    else if (selOptionIs(menu->selOption, LauncherMenuOption::EditGameSettings)) {
        loop_crossButtonPressed_STATE_SET__OPT_EDIT_GAME_SETTINGS();
    }
    else if (selOptionIs(menu->selOption, LauncherMenuOption::AbSettings)) {
        loop_crossButtonPressed_STATE_SET__OPT_AB_SETTINGS();
    }
}

//*******************************
// GuiLauncher::loop_joyButtonReleased
// button released
//*******************************
void GuiLauncher::loop_joyButtonReleased() {
    if (e.button == Button::L2) {
        app.audio().cursor.play();
        powerOffShift = false;
    }

    if (L1_isPressedForFastForward && (e.button == Button::L1)) {
        L1_isPressedForFastForward = false;
    }
    else if (R1_isPressedForFastForward && (e.button == Button::R1)) {
        R1_isPressedForFastForward = false;
    }
}
