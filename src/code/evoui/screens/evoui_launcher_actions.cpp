//
// GuiLauncher, the actions: what Cross does in each state and for each icon of the game menu - starting the
// game, opening the settings / game editor / memory-card manager / resume-point selector - and the L2+Select
// choosers for the games sub-directory and the RetroArch playlist. Each shows a sub-screen and reconciles
// the carousel afterwards. The event loop that dispatches to these is launcher_input.cpp.
//
#include "evoui_launcher.h"
#include "../../gui/gui.h"
#include "../../gui/menus/gui_options_menu.h"
#include "../../gui/screens/gui_confirm.h"
#include "../../gui/screens/gui_about.h"
#include "../../gui/menus/gui_game_editor_menu.h"
#include "../../gui/menus/gui_playlists_menu.h"
#include "../../gui/menus/gui_game_dir_menu.h"
#include "../../gui/menus/gui_memcards_menu.h"
#include "../../gui/menus/gui_game_manager_menu.h"
#include "../../core/services/environment.h"
#include "../../core/services/system.h"
#include "../../core/services/launch.h"
#include "evoui_mc_manager.h"
#include "evoui_app_start.h"
#include "evoui_system_menu.h"

#include <algorithm>
#include <iostream>

using namespace std;

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
    forgetHeldModifiers();   // reached with L2 held; its release went to the menu
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
    forgetHeldModifiers();   // reached with L2 held; its release went to the menu
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
    if (carousel.selectedIsValid()) {
        const PsGame &game = *carousel.games[carousel.selected];
        // the game's own cards, or the set it is mapped to (a game with no Game.ini says "" - its own)
        string memcard = app.memcards().activeCardName(game);
        if (memcard == MemcardService::SonyCard || memcard.empty()) {
            leftCardName = "[1]" + _("INTERNAL");
            rightCardName = "[2]" + _("INTERNAL");
            cardPath1 = game.ssFolder + sep + "memcards" + sep + "card1.mcd";
            cardPath2 = game.ssFolder + sep + "memcards" + sep + "card2.mcd";
        } else {
            leftCardName = "[1]" + memcard;
            rightCardName = "[2]" + memcard;
            cardPath1 = Env::getPathToMemCardsDir() + sep + memcard + sep + "card1.mcd";
            cardPath2 = Env::getPathToMemCardsDir() + sep + memcard + sep + "card2.mcd";
        }
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
// GuiLauncher::loop_openSystemMenu
//*******************************
// the system menu: everything the classic main menu used to offer, now reached with L2+R2 (docs/refactor-plan.md
// Step 4; moved off a bare R2 so reaching for it can no longer be mistaken for the L2+R2 power-off it replaced -
// Power Off is one of its own items now). GuiSystemMenu only picks; every action below is what ClassicMenuScreen
// used to do for the same item.
void GuiLauncher::loop_openSystemMenu() {
    app.audio().cursor.play();

    string retroArchLabel = _("RetroArch");
    string cfgPath = Env::getPathToRetroarchDir() + sep + "retroboot/retroboot.cfg";
    if (DirEntry::exists(cfgPath)) {
        IniFile RBcfg;
        RBcfg.load(cfgPath);
        if (RBcfg.values["use_emulationstation"] == "1")
            retroArchLabel = _("EmulationStation");
    }

    SystemMenuAction action;
    {
        GuiSystemMenu systemMenu(*gui);
        systemMenu.retroArchLabel = retroArchLabel;
        systemMenu.scanInProgress = app.scans().scanning();
        systemMenu.show();
        action = systemMenu.result;
    }
    // reached with L2 held: its release (and R2's) went to the menu, and the sub-screens below run their own
    // loops too - without this the launcher came back with the L2 shift still on, and every button behaved
    // as if L2 were down until it was pressed and released again
    forgetHeldModifiers();

    switch (action) {
        case SystemMenuAction::None:
            break;

        case SystemMenuAction::RescanGames:
            if (!app.scans().requestScan())
                notificationLines[1].setText(_("A scan is already in progress"), DefaultShowingTimeout);
            break;

        case SystemMenuAction::RetroArch: {
            if (!LaunchService::retroArchInstalled()) {
                GuiConfirm confirm(*gui);
                confirm.label = _("RetroArch is not installed");
                confirm.show();
                if (!confirm.result)
                    break;
            } else {
                app.library().exportToRetroArchPlaylist();
            }
            app.session().menuOption = MENU_OPTION_RETRO;
            menuVisible = false;
            break;
        }

        case SystemMenuAction::MemoryCards: {
            GuiMemcards memcardsScreen(*gui);
            memcardsScreen.show();
            break;
        }

        case SystemMenuAction::GameManager: {
            // it deletes game folders outright - letting the scanner read the same tree at the same time
            // is asking for trouble, so this is the one item the menu still refuses while scanning() is true
            if (app.scans().scanning()) {
                notificationLines[1].setText(_("Can't manage games while a scan is running"), DefaultShowingTimeout);
                break;
            }
            GuiManager managerScreen(*gui);
            managerScreen.show();
            if (selection.set == GameSet::PS1)
                reloadGames();
            break;
        }

        case SystemMenuAction::HardwareInfo: {
            app.audio().close();
            gui->input().flushPads();
#ifdef AB_DEBUG_HOST
            gui->drawText("Small delay to test");
            gui->platform().delay(2000);
#endif
            string cmd = Env::getPathToAppsDir() + sep + "pscbios/run.sh";
            System::runAndWait(cmd, {});
            gui->input().flushEvents();
            gui->input().probePads();
            app.audio().restart();
            app.audio().playMusic();
            break;
        }

        case SystemMenuAction::Options:
            // same screen, same reload, as the settings icon in the Set overlay
            loop_crossButtonPressed_STATE_SET__OPT_AB_SETTINGS();
            break;

        case SystemMenuAction::About: {
            GuiAbout aboutScreen(*gui);
            aboutScreen.show();
            break;
        }

        case SystemMenuAction::PowerOff: {
            GuiConfirm confirm(*gui);
            confirm.label = _("Are you sure you want to power off?");
            confirm.show();
            if (confirm.result) {
                gui->drawText(_("POWERING OFF... PLEASE WAIT"));
                System::powerOff();
            }
            break;
        }
    }
}
