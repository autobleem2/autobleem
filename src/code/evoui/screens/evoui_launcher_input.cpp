//
// GuiLauncher, the input half: the event loop and what each stick move and button does in each state.
// The handlers that open another screen or start a game are in launcher_actions.cpp.
//
#include "evoui_launcher.h"
#include "gui/gui.h"
#include "gui/screens/gui_confirm.h"
#include "evoui_btn_guide.h"
#include "core/model/timing.h"

#include <algorithm>
#include <iostream>
#include <ableem/engine/log.h>

using namespace std;

//*******************************
// GuiLauncher::loop
// event loop
//*******************************
void GuiLauncher::loop() {
    PLOG_DEBUG << "Main Loop";
    powerOffShift = false; // L2 shift used for power off and selecting game sub dir or RA playlist

    menuVisible = true;
    motionStart = 0;
    motionDir = 0;
    queuedScroll = 0;

    while (menuVisible) {
        // the menu's headers and blurbs in the current language - Options may have changed it on the way
        // back into this loop, which is the one time they need translating again
        if (headersLanguage != app.lang().currentLanguage()) {
            headersLanguage = app.lang().currentLanguage();
            headers = {_("SETTINGS"), _("GAME"), _("MEMORY CARD"), _("RESUME")};
            texts = {_("Customize AutoBleem settings"), _("Edit game parameters"), _("Edit Memory Card information"),
                     _("Resume game from saved state point")};
        }

        time = gui->platform().ticks();
        for (auto &obj : staticElements) {
            obj->update(time);
        }

        menu->update(time);
        carousel.updatePositions();
        if (!carousel.scrolling) {
            // the frame the carousel rests in does the loads a scroll put off; the idle frames after it
            // get the covers just past the ends of the row decoded, one a frame
            if (settleLoadsPending)
                finishSettleLoads();
            else
                carousel.loadOneMissingTexture();
        }
        applyScanUpdate(app.scans().poll());
        app.extensions().poll();
        applyExtensionRequests();
#ifdef AB_ONLINE_UPDATE
        pollUpdates();
#endif
        render();

        if (!carousel.scrolling && state == LauncherScreenState::Games) {
            if (queuedScroll != 0) {
                // the tap that came in during the last scroll
                if (queuedScroll > 0)
                    nextCarouselGame(CarouselScrollDuration);
                else
                    prevCarouselGame(CarouselScrollDuration);
                queuedScroll = 0;
            } else if (motionStart != 0 && time - motionStart > CarouselHoldDelay) {
                // the stick is held: the next step starts the moment the last one ends, at one speed, so
                // the row runs instead of stopping between games
                if (motionDir == 0)
                    nextCarouselGame(CarouselHeldScrollDuration, false);
                else
                    prevCarouselGame(CarouselHeldScrollDuration, false);
            }
        }

        while (gui->input().poll(e)) {
            // this is for pc Only - the window's own close button. Closing this screen alone is not enough:
            // AutoBleem::run() would just show a fresh GuiLauncher again (session().menuOption is nothing
            // this loop's exit condition checks for), reconstructing the whole screen every frame the
            // now-gone window still delivers an event for. quitRequested is what tells run() to actually
            // stop instead.
            if (e.type == Event::Type::Quit) {
                menuVisible = false;
                quitRequested = true;
            }
            switch (e.type) {
            case Event::Type::KeyDown:
                // the power button / Esc case is already handled inside Input::poll (see
                // Platform::setPowerOffHandler, wired up once in main.cpp)
                break;
            case Event::Type::DpadDown: /* Handle Joystick Motion */
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
                } else if (gui->input().dpadRight()) {
                    loop_joyMoveRight();
                } else if (gui->input().dpadUp()) {
                    loop_joyMoveUp();
                } else if (gui->input().dpadDown()) {
                    loop_joyMoveDown();
                } else {
                    ;
                }
                break;

            case Event::Type::ButtonDown:
                loop_joyButton_Pressed(); // button pressed
                break;
            case Event::Type::ButtonUp:
                loop_joyButtonReleased(); // button released
                break;
            default:
                break;

            } // switch (e.type)
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
    } // while (menuVisible)

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
            prevCarouselGame(CarouselScrollDuration);
        } else {
            queuedScroll = -1;
        }
    } else if (state == LauncherScreenState::Set) {

        if (menu->lastEnabled() > 0) {
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
            nextCarouselGame(CarouselScrollDuration);
        } else {
            queuedScroll = 1;
        }
    } else if (state == LauncherScreenState::Set) {

        if (menu->lastEnabled() > 0) {
            if (menu->selOption < menu->lastEnabled()) {
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
        if (carousel.games.empty()) {
            app.audio().cancel.play(); // nothing up there to go to (settleEmptyRoster)
            return;
        }
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
    if (e.button == Button::R2) {
        r2Held = true;
    }

    if (powerOffShift && r2Held && (e.button == Button::L2 || e.button == Button::R2)) {
        // L2+R2, in either order, opens the system menu (was a direct power off; Power Off is one of the
        // menu's items now, so one accidental R2 while reaching for L2 no longer shuts the console down)
        loop_openSystemMenu();
        return;
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
void GuiLauncher::loop_prevNextGameFirstLetter(bool next) { // false is prev, true is next
    // one sound per press, played below where the jump is decided: a cursor.play() here as well put the
    // same sound on two mixer channels at once, twice as loud as a d-pad step
    if (state == LauncherScreenState::Games) {
        if (carousel.games.empty()) {
            return;
        }

        if (carousel.games[carousel.selected]->title == "") {
            return;
        }

        // find the index of all the first letters
        map<char, int> firstLetterToIndex;
        for (int index = 0; index < carousel.games.size(); ++index) {
            if (carousel.games[index]->title != "") {
                char firstLetter = toupper(carousel.games[index]->title[0]);
                if (firstLetterToIndex.find(firstLetter) == firstLetterToIndex.end()) // if letter not in map
                    firstLetterToIndex[firstLetter] = index;                          // add the first letter to the map
            }
        }

        if (firstLetterToIndex.size() == 0)
            return; // nothing with a title

        if (carousel.selectedIsValid()) {
            char currentLetter = toupper(carousel.games[carousel.selected]->title[0]);
            int nextGame = carousel.selected;
            if (firstLetterToIndex.size() == 1) {
                nextGame = firstLetterToIndex[currentLetter]; // there is only one first letter in the games
            } else {
                auto iter = firstLetterToIndex.find(currentLetter);
                if (next) {
                    if (firstLetterToIndex.upper_bound(currentLetter) ==
                        firstLetterToIndex.end())                      // if this is the last letter
                        nextGame = firstLetterToIndex.begin()->second; // wrap around to first letter
                    else
                        nextGame = (++iter)->second; // next letter
                } else {
                    if (iter == firstLetterToIndex.begin())             // if this is the first letter
                        nextGame = firstLetterToIndex.rbegin()->second; // wrap around to last letter
                    else
                        nextGame = (--iter)->second; // prev letter
                }
            }

            if (nextGame != carousel.selected) {
                // we have prev/next game first letter;
                carousel.selected = nextGame;
                app.audio().cursor.play();
                notificationLines[1].setText(toUpperCopy(carousel.games[carousel.selected]->title.substr(0, 1)),
                                             DefaultShowingTimeout);
                carousel.setInitialPositions(carousel.selected);
                updateMeta();
                menu->setResumePic(app.resumePoints().lastPicture(*carousel.games[carousel.selected]));
            } else {
                // no change
                app.audio().cancel.play();
                notificationLines[1].setText(toUpperCopy(carousel.games[carousel.selected]->title.substr(0, 1)),
                                             DefaultShowingTimeout);
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
void GuiLauncher::loop_nextGameFirstLetter() {
    loop_prevNextGameFirstLetter(true);
    R1_fastForwardTimeStart = gui->platform().ticks();
};

//*******************************
// GuiLauncher::loop_selectButtonPressed
//*******************************
void GuiLauncher::loop_selectButton_Pressed() {
    // with the game menu's icon row open, Select (and L2+Select) still change the set: the row closes
    // first, the way Up closes it, and the switch goes on from the Games state
    if (state == LauncherScreenState::Set) {
        if (menu->animationStarted != 0)
            return;
        menu->transition = TR_MENUON;
        switchState(LauncherScreenState::Games, time);
        motionStart = 0;
    }
    if (state == LauncherScreenState::Games) {
        app.audio().cursor.play();
        loop_chooseSet(); // with or without L2: the one picker for sets, folders and playlists
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

        carousel.selected = System::getRandomIndex(carousel.games.size());
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
        if (carousel.games.empty()) {
            app.audio().cancel.play(); // the row stays open on an empty set (settleEmptyRoster)
            return;
        }
        if (menu->animationStarted == 0) {
            menu->transition = TR_MENUON;
            switchState(LauncherScreenState::Games, time);
            motionStart = 0;
        }
    } else if (state == LauncherScreenState::Games) {
        // there is no classic menu to fall back to any more (the background-scan work's step 3) - Circle here
        // used to close the launcher; R2 (the system menu) is where Power Off and everything else now lives
        app.audio().cancel.play();
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

    if (Env::retroArchInstalled()) {

        if (state == LauncherScreenState::Games) {
            if (carousel.games.empty()) {
                return;
            }
            if (carousel.selectedIsValid() && carousel.games[carousel.selected]->foreign) {
                return;
            }
            if (refuseLicenceProtected()) {
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
    } else if (selOptionIs(menu->selOption, LauncherMenuOption::EditMemcardInfo)) {
        loop_crossButtonPressed_STATE_SET__OPT_EDIT_MEMCARD();
    } else if (selOptionIs(menu->selOption, LauncherMenuOption::EditGameSettings)) {
        loop_crossButtonPressed_STATE_SET__OPT_EDIT_GAME_SETTINGS();
    } else if (selOptionIs(menu->selOption, LauncherMenuOption::AbSettings)) {
        loop_crossButtonPressed_STATE_SET__OPT_AB_SETTINGS();
    }
}

//*******************************
// GuiLauncher::forgetHeldModifiers
//*******************************
void GuiLauncher::forgetHeldModifiers() {
    powerOffShift = false;
    r2Held = false;
    L1_isPressedForFastForward = false;
    R1_isPressedForFastForward = false;
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
    if (e.button == Button::R2) {
        r2Held = false;
    }

    if (L1_isPressedForFastForward && (e.button == Button::L1)) {
        L1_isPressedForFastForward = false;
    } else if (R1_isPressedForFastForward && (e.button == Button::R1)) {
        R1_isPressedForFastForward = false;
    }
}
