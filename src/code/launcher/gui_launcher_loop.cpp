#include "gui_launcher.h"
#include "../gui/gui.h"
#include "../gui/menus/gui_optionsMenu.h"
#include "../gui/gui_confirm.h"
#include "../gui/menus/gui_gameEditorMenu.h"
#include "gui_btn_guide.h"
#include <algorithm>
#include <iostream>
#include "../gui/menus/gui_playlistsMenu.h"
#include "gui_mc_manager.h"
#include "../gui/menus/gui_gameDirMenu.h"
#include "gui_app_start.h"

using namespace std;

// the four icons of the settings overlay, in the order PsMenu lays them out. PsMenu::selOption stays a plain
// index into that row (it also animates the zoom per option), so it is compared through selOptionIs.
enum class LauncherMenuOption : int { AbSettings = 0, EditGameSettings, EditMemcardInfo, ResumeFromSavestate };
static bool selOptionIs(int selOption, LauncherMenuOption option) { return selOption == static_cast<int>(option); }


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
// GuiLauncher::loop_chooseGameDir
//*******************************
void GuiLauncher::loop_chooseGameDir() {
    // pop game dir menu
    powerOffShift = false;
    SubDirRowInfos gameRowInfos;
    app.library().usbGames().loadSubDirRows(&gameRowInfos);
    if (gameRowInfos.size() == 0) {
        return; // no games!
    }
    GuiGameDirMenu guiGameDirMenu(*gui);

    // add All Games and Internal Games only if origames is true in the config.ini
    int offsetToGamesSubDirs {0};
    bool showInternalGames = app.gameQuery().showInternalGames();
    if (showInternalGames) {
        // show internal is enabled.  show usbgames + internal, and show internal only menu items.
        PsGames gamesList = app.gameQuery().ps1GamesInSubDirRow(0);
        int usbOnly = gamesList.size();
        gamesList += app.gameQuery().internalGames();
        guiGameDirMenu.lines.emplace_back(_("All Games") + " ( " + to_string(gamesList.size()) + ")");
        guiGameDirMenu.lines.emplace_back(_("Internal Games") + " ( " + to_string(gamesList.size() - usbOnly) + ")"); // 20 games
        offsetToGamesSubDirs = 2;
    } else {
        // show internal is disabled.  top game row 0 shows all usb games from /Games down.
        offsetToGamesSubDirs = 0;
    }

    // add the /Games dir and all the game sub directories
    bool top = true;
    for (auto &rowInfo : gameRowInfos) {
        if (top) {
            guiGameDirMenu.lines.emplace_back(string(rowInfo.indentLevel * 4, ' ') +
                                               _("USB Games") + // display "USB Games" instead of "Games"
                                               " ( " + to_string(rowInfo.numGames) + ")");
            top = false;
        } else {
            guiGameDirMenu.lines.emplace_back(string(rowInfo.indentLevel * 4, ' ') +
                                               rowInfo.rowName +
                                               " ( " + to_string(rowInfo.numGames) + ")");
        }
    }

    // add Favorite Games at the bottom
    int favoritesIndex = guiGameDirMenu.lines.size();  // favorites is the last line
    PsGames gamesList = app.gameQuery().favorites();
    guiGameDirMenu.lines.emplace_back(_("Favorite Games") + " ( " + to_string(gamesList.size()) + ")");

    // add History Games at the bottom
    int historyIndex = guiGameDirMenu.lines.size();  // history is the last line
    gamesList = app.gameQuery().history();
    guiGameDirMenu.lines.emplace_back(_("Game History") + " ( " + to_string(gamesList.size()) + ")");

    // set initial selected row
    int nextSel = offsetToGamesSubDirs; // set to game dir as default
    if (selection.ps1SelectState == Ps1SelectState::GamesSubdir) {
        nextSel = offsetToGamesSubDirs + selection.usbGameDirIndex;
    }
    else {
        if (selection.ps1SelectState == Ps1SelectState::Favorites)
            nextSel = favoritesIndex; // favorites is the next to the last line
        else if (selection.ps1SelectState == Ps1SelectState::History)
            nextSel = historyIndex; // history is the last line
        else {
            if (showInternalGames)
                nextSel = static_cast<int>(selection.ps1SelectState);   // AllGames is on row 0, InternalOnly is on row 1
            else {
                    cout << "Error: loop_chooseGameDir() called with \"origames\" off and selection.ps1SelectState = " <<
                    static_cast<int>(selection.ps1SelectState) << endl;
            }
        }
    }

    guiGameDirMenu.selected = nextSel;

    // display the menu and return when user made selection or canceled
    guiGameDirMenu.show();
    bool cancelled = guiGameDirMenu.cancelled;

    // set the select state to the user selection
    if (!cancelled) {
        if (showInternalGames && guiGameDirMenu.selected < offsetToGamesSubDirs)
            // rows 0 and 1 of the menu are AllGames and InternalOnly, in that order
            selection.ps1SelectState = static_cast<Ps1SelectState>(guiGameDirMenu.selected);
        else if (guiGameDirMenu.selected == favoritesIndex)
            selection.ps1SelectState = Ps1SelectState::Favorites;
        else if (guiGameDirMenu.selected == historyIndex)
            selection.ps1SelectState = Ps1SelectState::History;
        else {
            selection.ps1SelectState = Ps1SelectState::GamesSubdir;
            selection.usbGameDirIndex = guiGameDirMenu.selected - offsetToGamesSubDirs;
            selection.usbGameDirName = "";
            if (selection.usbGameDirIndex < gameRowInfos.size())
                selection.usbGameDirName = gameRowInfos[selection.usbGameDirIndex].rowName;
        }
    }


    if (cancelled)
        return;
    switchSet(selection.set,true);
    menuHead->setText(headers[0], fgColor);
    menuText->setText(texts[0], fgColor);
    showSetName();
    if (carousel.selected != -1 && carousel.selectedIsValid()) {
        updateMeta();
        menu->setResumePic(app.resumePoints().lastPicture(*carousel.games[carousel.selected]));
    } else {
        updateMeta();
    }
}

//*******************************
// GuiLauncher::loop_chooseRAPlaylist
//*******************************
void GuiLauncher::loop_chooseRAPlaylist() {
    if (!DirEntry::exists(Env::getPathToRetroarchDir())) {
        return;
    }
    if (raPlaylists.empty()) {
        return;
    }
    powerOffShift = false;
    GuiPlaylists playlists(*gui);
    playlists.playlists = raPlaylists;

    // set the selected menu line to be the current playlist
    int nextSel = 0;
    int i = 0;
    for (string plist:playlists.playlists) {
        if (plist == selection.raPlaylistName) {
            nextSel = i;
            break;
        }
        i++;
    }
    playlists.selected = nextSel;

    playlists.show();
    bool cancelled = playlists.cancelled;
    int selected = playlists.selected;

    if (cancelled)
        return;

    selection.raPlaylistIndex = selected;
    selection.raPlaylistName = raPlaylists[selected];
    selection.set = GameSet::RetroArch;
    switchSet(selection.set,false);
    menuHead->setText(headers[0], fgColor);
    menuText->setText(texts[0], fgColor);
    showSetName();
    if (carousel.selected != -1 && carousel.selectedIsValid()) {
        updateMeta();
        menu->setResumePic(app.resumePoints().lastPicture(*carousel.games[carousel.selected]));
    } else {
        updateMeta();
    }
}

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
// GuiLauncher::loop_crossButtonPressed_STATE_GAMES
//*******************************
void GuiLauncher::loop_crossButtonPressed_STATE_GAMES() {
    if (carousel.games.empty()) {
        return;
    }

    app.session().startingGame = true;
    if (carousel.selectedIsValid()) {
        app.session().runningGame = carousel.games[carousel.selected];
    }
    app.session().resumePoint = -1;
    rememberSelection();
    menuVisible = false;

    if (selection.set == GameSet::PS1)
        app.gameCatalog().recordGamePlayed(app.session().runningGame);

    app.session().emuMode = EmuMode::Pcsx;

    // if it's a PS1 game see if the user wants to play it in RetroArch instead
    if (selection.set == GameSet::PS1) {
        if (app.session().runningGame->internal) {
            if (app.session().runningGame->play_using_ra)
                return loop_squareButton_Pressed();     // play internal PSX game in RA
        } else {
            IniFile gameini;
            gameini.load(carousel.games[carousel.selected]->folder + sep + GAME_INI);
            if (gameini.values["play_using_ra"] == "true")
                return loop_squareButton_Pressed();     // play PSX game in RA
        }
        if (app.config().inifile.values["play_all_psx_with_ra"] == "true")
            return loop_squareButton_Pressed();     // play PSX game in RA
    }

    if (app.session().runningGame->foreign)
    {
        if (!app.session().runningGame->app) {
            app.session().emuMode = EmuMode::RetroArch;
        } else {
            GuiAppStart appStartScreen(*gui);
            appStartScreen.setGame(app.session().runningGame);
            appStartScreen.show();
            bool result = appStartScreen.result;
            // Do not run
            if (!result)
            {
                app.session().startingGame = false;
                menuVisible = true;

            }
            app.session().emuMode = EmuMode::Launcher;
            }
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
// GuiLauncher::loop_crossButtonPressed_STATE_SET__OPT_AB_SETTINGS
//*******************************
void GuiLauncher::loop_crossButtonPressed_STATE_SET__OPT_AB_SETTINGS() {
    app.audio().cursor.play();
    GameSet lastSet = selection.set;
    Ps1SelectState lastPS1_SelectState = selection.ps1SelectState;
    int lastUSBGameDirIndex = selection.usbGameDirIndex;
    int lastRAPlaylistIndex = selection.raPlaylistIndex;
    int lastGame = carousel.selected;
    GuiOptions option(*gui);
    option.show();
    bool exitCode = option.exitCode;

    if (exitCode == 0) {
        freeAssets();
        loadAssets();
        app.session().resumingGui = false;
        selection.set = lastSet;
        if (selection.set == GameSet::PS1)
            selection.ps1SelectState = lastPS1_SelectState;
        selection.usbGameDirIndex = lastUSBGameDirIndex;
        selection.raPlaylistIndex = lastRAPlaylistIndex;
        carousel.selected = lastGame;
        bool resetCarouselPosition = false;

        switchSet(selection.set,false);
        showSetName();

        if (resetCarouselPosition) {
            if (carousel.games.empty()) {
                carousel.selected = -1;
                updateMeta();
            } else {
                carousel.selected = 0;
                carousel.setInitialPositions(0);
                updateMeta();
            }
        } else {
            if (carousel.selected != -1) {
                carousel.setInitialPositions(carousel.selected);
                updateMeta();
                if (carousel.selectedIsValid())
                    menu->setResumePic(app.resumePoints().lastPicture(*carousel.games[carousel.selected]));
            }
        }

        if (!carousel.games.empty()) {
            gui->loadAssets();
            for (auto &game : carousel.games) {
                game.freeTex();
            }
            carousel.setInitialPositions(carousel.selected);
        } else {
            gui->loadAssets();
            meta->gameName = "";
            menu->setResumePic("");
        }

        state = LauncherScreenState::Games;
    } else {
        render();
    }
}

//*******************************
// GuiLauncher::loop_crossButtonPressed_STATE_SET__OPT_EDIT_GAME_SETTINGS
//*******************************
void GuiLauncher::loop_crossButtonPressed_STATE_SET__OPT_EDIT_GAME_SETTINGS() {
    if (carousel.games.empty()) {
        return;
    }

    app.audio().cursor.play();
    GuiEditor editor(*gui);
    if (carousel.selectedIsValid()) {
        editor.gameData = carousel.games[carousel.selected];
    }

    editor.show();

    if (carousel.selectedIsValid()) {
        if (!carousel.games[carousel.selected]->internal) {
            if (editor.changes) {
                IniFile gameIni;
                gameIni.load(carousel.games[carousel.selected]->folder + sep + GAME_INI);
                app.library().updateTitle(*carousel.games[carousel.selected], gameIni.values["title"]);
            }
            app.library().reload(*carousel.games[carousel.selected]);
            if (selection.set == GameSet::PS1 && selection.ps1SelectState == Ps1SelectState::Favorites &&
                editor.settings.ini.values["favorite"] == "0") {
                app.session().launcher.set = GameSet::PS1;
                app.session().launcher.ps1SelectState = Ps1SelectState::Favorites;
                loadAssets();   // reload - one less favorite game in display
            }
        } else {
            if (editor.changes) {
                app.library().updateTitle(*carousel.games[carousel.selected], editor.lastName);
            }
            app.library().reload(*carousel.games[carousel.selected]);
            if (selection.set == GameSet::PS1 && selection.ps1SelectState == Ps1SelectState::Favorites &&
                editor.gameData->favorite == false) {
                app.session().launcher.set = GameSet::PS1;
                app.session().launcher.ps1SelectState = Ps1SelectState::Favorites;
                loadAssets();   // reload - one less favorite game in display
            }
        }
    }

    // if the current set is favorites and the user removes the last favorite carousel.selected will be -1
    if (carousel.selected != -1 && carousel.selectedIsValid()) {
        carousel.setInitialPositions(carousel.selected);
        updateMeta();
        menu->setResumePic(app.resumePoints().lastPicture(*carousel.games[carousel.selected]));

        PsScreenpoint point2;
        point2.x = 640 - 113;
        point2.y = 90;
        point2.scale = 1;
        point2.shade = 220;

        carousel.games[carousel.selected].destination = point2;
        carousel.games[carousel.selected].actual = point2;
        carousel.games[carousel.selected].current = point2;
    }
    // fix to put back cover on top position
}

//*******************************
// GuiLauncher::loop_crossButtonPressed_STATE_SET__OPT_EDIT_MEMCARD
//*******************************
void GuiLauncher::loop_crossButtonPressed_STATE_SET__OPT_EDIT_MEMCARD() {
    if (carousel.games.empty()) {
        return;
    }
    if (carousel.selectedIsValid() && carousel.games[carousel.selected]->foreign)
    {
        return;
    }

    string leftCardName;
    string rightCardName;
    string cardPath1;
    string cardPath2;
    string memcard; // Mapped card
    if (carousel.selectedIsValid()) {
        leftCardName = "[1]" + _("INTERNAL");
        rightCardName = "[2]" + _("INTERNAL");
        cardPath1 = carousel.games[carousel.selected]->ssFolder + "memcards/card1.mcd";
        cardPath2 = carousel.games[carousel.selected]->ssFolder + "memcards/card2.mcd";
        // Mapped card
        memcard = "SONY";
        if (!carousel.games[carousel.selected]->internal) {
            IniFile gameini;
            gameini.load(carousel.games[carousel.selected]->folder + sep + GAME_INI);
            memcard = gameini.values["memcard"];
        }
    }
    if (memcard!="SONY")
    {
        cardPath1 =  Env::getPathToMemCardsDir() + sep + memcard  +"card1.mcd";
        cardPath1 =  Env::getPathToMemCardsDir() + sep + memcard  +"card2.mcd";
        leftCardName = "[1]"+ memcard;
        rightCardName = "[2]"+ memcard;
    }

    app.audio().cursor.play();
    GuiMcManager mcManager(*gui);
    mcManager.backgroundImg=background->tex;
    mcManager.leftCardName = leftCardName;
    mcManager.rightCardName = rightCardName;
    mcManager.card1path = cardPath1;
    mcManager.card2path = cardPath2;
    mcManager.show();
}

//*******************************
// GuiLauncher::loop_crossButtonPressed_STATE_SET__OPT_RESUME_FROM_SAVESTATE
//*******************************
void GuiLauncher::loop_crossButtonPressed_STATE_SET__OPT_RESUME_FROM_SAVESTATE() {
    if (carousel.games.empty()) {
        return;
    }
    bool resumeAvailable = false;
    for (int i = 0; i < 4; i++) {
        if (carousel.selectedIsValid() && app.resumePoints().slotIsActive(*carousel.games[carousel.selected], i)) {
            resumeAvailable = true;
        }
    }

    if (resumeAvailable) {
        app.audio().cursor.play();
        sselector->visible = true;
        if (carousel.selectedIsValid())
            sselector->loadSaveStateImages(carousel.games[carousel.selected], false);
        state = LauncherScreenState::Resume;
        sselector->selSlot = 0;
        sselector->operation = OP_LOAD;
    } else {
        app.audio().cancel.play();
    }
}

//*******************************
// GuiLauncher::loop_crossButtonPressed_STATE_RESUME
//*******************************
void GuiLauncher::loop_crossButtonPressed_STATE_RESUME() {
    if (carousel.selectedIsValid()) {
        auto game = carousel.games[carousel.selected];
        int slot = sselector->selSlot;

        if (sselector->operation == OP_LOAD) {
            if (app.resumePoints().slotIsActive(*game, slot)) {
                app.audio().cursor.play();
                app.session().startingGame = true;
                app.session().runningGame = carousel.games[carousel.selected];
                app.session().resumePoint = slot;
                rememberSelection();
                sselector->cleanSaveStateImages();
                app.session().emuMode = EmuMode::Pcsx;
                menuVisible = false;
            } else {
                app.audio().cancel.play();
            }
        } else {
            //app.audio().cursor.play();
            app.resumePoints().saveAfterLaunch(*carousel.games[carousel.selected], sselector->selSlot);
            app.resumePoints().storePictureForSlot(*carousel.games[carousel.selected], sselector->selSlot);
            sselector->visible = false;
            arrow->visible = true;
            app.audio().resume.play();
            notificationLines[1].setText(
                    _("Resume point saved to slot") + " " + to_string(sselector->selSlot + 1),
                    DefaultShowingTimeout);

            menu->setResumePic(app.resumePoints().pictureForSlot(*carousel.games[carousel.selected], sselector->selSlot));

            if (sselector->operation == OP_LOAD) {
                state = LauncherScreenState::Set;
            } else {
                state = LauncherScreenState::Games;
            }
        }
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
