//
// Created by screemer on 2/8/19.
//

#include "gui_launcher.h"
#include "../gui/gui.h"
#include "../gui/menus/gui_optionsMenu.h"
#include "../gui/gui_confirm.h"
#include <algorithm>
#include <iostream>
#include "gui_mc_manager.h"
#include <cassert>

using namespace std;

const ableem::Color brightWhite = {255, 255, 255, 255};


//*******************************
// GuiLauncher::updateMeta
//*******************************
// just update metadata section to be visible on the screen
void GuiLauncher::updateMeta() {
    if (carouselGames.empty()) {
        gameName = "";
        bool internal {false};
        bool hd {false};
        bool locked {false};
        bool discs {0};
        bool favorite {false};
        bool play_using_ra {false};
        bool foreign {false};
        bool app {false};
        string last_played {""};
        meta->updateTexts(gameName, publisher, year, serial, region, players, internal, hd, locked, discs, favorite,
                          foreign,play_using_ra, app, last_played, fgColor);
        return;
    }
    if (selGameIndexInCarouselGamesIsValid())
        meta->updateTexts(carouselGames[selGameIndex], fgColor);
}

//*******************************
// GuiLauncher::getGames_SET_SUBDIR
//*******************************
void GuiLauncher::getGames_SET_SUBDIR(PsGames* gamesList, int rowIndex) {
    SubDirRowInfos gameRowInfos;
    app.library().usbGames().loadSubDirRows(&gameRowInfos);
    if (gameRowInfos.size() == 0)
        return; // no games!
    currentUSBGameDirName = gameRowInfos[rowIndex].rowName;

#if 0
    for (auto &gameRowInfo : gameRowInfos)
            cout << "game row: " << gameRowInfo.subDirRowIndex << ", " << gameRowInfo.rowName << ", " <<
                 gameRowInfo.indentLevel << ", " << gameRowInfo.numGames << endl;
#endif
    PsGames completeList = PsGame::fromRecords(app.library().usbGames().loadUsbGames());

    vector<int> gameIdsInRow;
    app.library().usbGames().loadGameIdsInSubDirRow(&gameIdsInRow, rowIndex);
#if 0
    for (auto &id : gameIdsInRow) {
            cout << "game row: " << selectedRowIndex << ", id: " << id << endl;
        }
#endif

    if (rowIndex < gameRowInfos.size()) {
        for (auto &psgame : completeList) {
            if (find(begin(gameIdsInRow), end(gameIdsInRow), psgame->gameId) != end(gameIdsInRow)) {
                //cout << "game in row: " << psgame->title << endl;
                gamesList->emplace_back(psgame);
            }
        }

    }
}

//*******************************
// GuiLauncher::appendGames_SET_INTERNAL
//*******************************
void GuiLauncher::appendGames_SET_INTERNAL(PsGames *gamesList) {
    PsGames internal = PsGame::fromRecords(app.library().internalGames().loadInternalGames());
    for (const auto &internalGame : internal) {
        gamesList->push_back(internalGame);
    }
}

//*******************************
// GuiLauncher::getGames_SET_FAVORITE
//*******************************
void GuiLauncher::getGames_SET_FAVORITE(PsGames *gamesList) {
    PsGames completeList = getAllPS1Games(true, app.config().inifile.values["origames"] == "true");

    // put only the favorites in gamesList
    copy_if(begin(completeList), end(completeList), back_inserter(*gamesList),
            [](const PsGamePtr &game) { return game->favorite; });
}

//*******************************
// GuiLauncher::getGames_SET_HISTORY
//*******************************
void GuiLauncher::getGames_SET_HISTORY(PsGames *gamesList) {
    PsGames completeList = getAllPS1Games(true, app.config().inifile.values["origames"] == "true");

    // put only the history in gamesList
    copy_if(begin(completeList), end(completeList), back_inserter(*gamesList),
            [](const PsGamePtr &game) { return game->history > 0; });
}

//*******************************
// GuiLauncher::getAllPS1Games
//*******************************
PsGames GuiLauncher::getAllPS1Games(bool includeUSB, bool includeInternal) {
    PsGames gamesList;
    if (includeUSB)
        getGames_SET_SUBDIR(&gamesList, 0);
    if (includeInternal)
        appendGames_SET_INTERNAL(&gamesList);

    return gamesList;
}

//*******************************
// GuiLauncher::getGames_SET_RETROARCH
//*******************************
void GuiLauncher::getGames_SET_RETROARCH(const std::string &playlistName, PsGames *gamesList) {
    cout << "Getting RA games for playlist: " << playlistName << endl;
    if (playlistName != "")
        *gamesList = raIntegrator->getGames(playlistName);
}

//*******************************
// GuiLauncher::getGames_SET_APPS
//*******************************
void GuiLauncher::getGames_SET_APPS(PsGames *gamesList) {
    PsGames completeList;

    std::string appPath = Environment::getPathToAppsDir();
    if (!DirEntry::exists(appPath)) {
        return;
    }
    DirEntries dirs = DirEntry::diru_DirsOnly(appPath);
    cout << "Scanning apps in: " << appPath << endl;
    for (auto &dir : dirs) {
        std::string appIni = appPath + sep + dir.name + sep + "app.ini";
        cout << "AppIni: " << appIni << endl;
        if (DirEntry::exists(appIni)) {
            IniFile file;
            file.load(appIni);
            PsGamePtr game{new PsGame};
            game->gameId = 0;
            game->year = 0;
            game->players = 0;
            game->memcard = "";
            game->cds = 0;

            game->title = file.values["title"];
            game->publisher = file.values["author"];
            game->readme_path = appPath + sep + dir.name + sep + file.values["readme"];
            game->startup = file.values["startup"];
            game->image_path = appPath + sep + dir.name + sep + file.values["image"];
            game->base = appPath + sep + dir.name;
            game->kernel = file.values["kernel"] == "true";
            game->app = true;
            game->foreign = true;

            gamesList->push_back(game);
        }
    }
}

//*******************************
// GuiLauncher::switchSet
//*******************************
void GuiLauncher::switchSet(int newSet, bool noForce) {     // Warning: newSet is not used.  probably not the intent.
    cout << "Switching to Set: " << currentSet << endl;
    // clear the carousel text
    if (!carouselGames.empty()) {
        for (auto &game : carouselGames) {
            game.freeTex();
        }
    }

    cout << "Reloading games list" << endl; // get fresh list of games for this set
    PsGames gamesList;

    if (currentSet == SET_PS1) {

        // if do not show internal games
        if (app.config().inifile.values["origames"] != "true") {
            if (currentPS1_SelectState == SET_PS1_All_Games || currentPS1_SelectState == SET_PS1_Internal_Only) {
                currentPS1_SelectState = SET_PS1_Games_Subdir;
                //if (selGameIndexInCarouselGamesIsValid())
            }
        }

        if (currentPS1_SelectState == SET_PS1_All_Games) {
            bool includeInternal = app.config().inifile.values["origames"] == "true";
            gamesList = getAllPS1Games(true, includeInternal);

        } else if (currentPS1_SelectState == SET_PS1_Internal_Only) {
            appendGames_SET_INTERNAL(&gamesList);   // since it starts out empty this sets only internal

        } else if (currentPS1_SelectState == SET_PS1_Games_Subdir) {
            // get the games in the current subdir of /Games and on down
            getGames_SET_SUBDIR(&gamesList, currentUSBGameDirIndex);

        } else if (currentPS1_SelectState == SET_PS1_Favorites) {
            getGames_SET_FAVORITE(&gamesList);

        } else if (currentPS1_SelectState == SET_PS1_History) {
            getGames_SET_HISTORY(&gamesList);
        }

    } else if (currentSet == SET_RETROARCH) {
        getGames_SET_RETROARCH(currentRAPlaylistName, &gamesList);

    } else if (currentSet == SET_APPS) {
        getGames_SET_APPS(&gamesList);
    }

    if (!(currentSet == SET_RETROARCH && currentRAPlaylistName == raIntegrator->historyDisplayName)) {
        if (currentSet == SET_PS1 && currentPS1_SelectState == SET_PS1_History) {
            // sort by history 1-100.  1 is latest game played, 100 is the oldest
            sort(begin(gamesList), end(gamesList),
                 [&](PsGamePtr p1, PsGamePtr p2) { return p1->history < p2->history; });
        } else {
            // sort by title
            sort(gamesList.begin(), gamesList.end(), sortByTitle);
        }
    }
    cout << "Games Sorted" << endl;
    // copy the gamesList into the carousel
    carouselGames.clear();
    for_each(begin(gamesList), end(gamesList), [&](PsGamePtr &game) { carouselGames.emplace_back(game); });

    // save the actual number of (non-duplicated) games for the "showing" display
    numberOfNonDuplicatedGamesInCarousel = carouselGames.size();

    // if there are games in the carousel but not enough to fill it, duplicate the games until it is full
    if (carouselGames.size() > 0) {
        if (carouselGames.size() < 13) {    // if not enough games to fill the carousel
            // duplicate the gamesList until the carousel is full
            while (carouselGames.size() < 13) {
                for (auto &game : gamesList)
                    carouselGames.emplace_back(game);
            }
        }
    }

    cout << "Setting initial positions" << endl;
    if (carouselGames.empty()) {
        selGameIndex = -1;
    } else {
        selGameIndex = 0;
        setInitialPositions(0);
    }

    if (!noForce) {
        if ((currentSet == SET_RETROARCH) || (currentSet == SET_APPS)) {
            forceSettingsOnly();
        }
    }
}

//*******************************
// GuiLauncher::showSetName
//*******************************
void GuiLauncher::showSetName() {
    vector<string> setNames = {  "Showing: PS1 games",      // this is a dummy entry. setPS1SubStateNames is used.
                               _("Showing: Retroarch") + " ",
                               _("Showing: Apps") + " "
    };
    vector<string> setPS1SubStateNames = {_("Showing: All Games") + " ",
                                          _("Showing: Internal Games") + " ",
                                          _("Showing: Favorite Games") + " ",
                                          _("Showing: Game History") + " ",
                                          _("Showing: USB Games Directory:") + " "
    };
    assert(currentPS1_SelectState >= 0 && currentPS1_SelectState <= SET_PS1_Games_Subdir);
    assert(setPS1SubStateNames.size()-1 == SET_PS1_Games_Subdir);  // currentPS1_SelectState out of range

    string numGames = " (" + to_string(numberOfNonDuplicatedGamesInCarousel) + " " + _("games") + ")";

    long timeout = Util::toInt(app.config().inifile.values["showingtimeout"], 0) * TicksPerSecond;

    if (currentSet == SET_PS1) {
        if (currentPS1_SelectState == SET_PS1_All_Games) {
            notificationLines[0].setText(setPS1SubStateNames[currentPS1_SelectState] + numGames, timeout);
        } else if (currentPS1_SelectState == SET_PS1_Internal_Only) {
            notificationLines[0].setText(setPS1SubStateNames[currentPS1_SelectState] + numGames, timeout);
        } else if (currentPS1_SelectState == SET_PS1_Favorites) {
            notificationLines[0].setText(setPS1SubStateNames[currentPS1_SelectState] + numGames, timeout);
        } else if (currentPS1_SelectState == SET_PS1_History) {
            notificationLines[0].setText(setPS1SubStateNames[currentPS1_SelectState] + numGames, timeout);
        } else if (currentPS1_SelectState == SET_PS1_Games_Subdir) {
            notificationLines[0].setText(
                    setPS1SubStateNames[currentPS1_SelectState] + currentUSBGameDirName + numGames, timeout);
        }
    } else if (currentSet == SET_RETROARCH) {
        string playlist = DirEntry::getFileNameWithoutExtension(currentRAPlaylistName);
        notificationLines[0].setText(setNames[currentSet] + playlist + " " + numGames, timeout);
    } else if (currentSet == SET_APPS) {
        notificationLines[0].setText(setNames[currentSet] + numGames, timeout);
    }
}

//*******************************
// GuiLauncher::loadAssets
//*******************************
// load all assets needed by the screengame i
void GuiLauncher::loadAssets() {
    cout << "Loading playlists" << endl;
    raPlaylists.clear();
    if (DirEntry::exists(Env::getPathToRetroarchDir())) {
        raPlaylists = raIntegrator->getPlaylists();
    }
    vector<string> headers = {_("SETTINGS"), _("GAME"), _("MEMORY CARD"), _("RESUME")};
    vector<string> texts = {_("Customize AutoBleem settings"), _("Edit game parameters"),
                            _("Edit Memory Card information"), _("Resume game from saved state point")};

    currentSet = app.session().launcher.set;
    if (currentSet == SET_PS1)
        currentPS1_SelectState = app.session().launcher.ps1SelectState;
    currentUSBGameDirIndex = app.session().launcher.usbGameDirIndex;
    currentRAPlaylistIndex = app.session().launcher.raPlaylistIndex;
    if (currentRAPlaylistIndex < raPlaylists.size())
        currentRAPlaylistName = raPlaylists[app.session().launcher.raPlaylistIndex];
    if (app.session().launcher.raPlaylistIndex < raPlaylists.size())
        app.session().launcher.raPlaylistName = raPlaylists[app.session().launcher.raPlaylistIndex];
#if 0
    if (app.session().launcher.raPlaylistName != "")
    {
        currentRAPlaylistName = app.session().launcher.raPlaylistName;
        //app.session().launcher.raPlaylistName = "";
    }
#endif

    for (int i = 0; i < 100; i++) {
        gui->input().flushEvents();
    }

    IniFile colorsFile;
    if (DirEntry::exists(app.theme().path() + sep + "colors.ini")) {
        colorsFile.load(app.theme().path() + sep + "colors.ini");
        fgColor.r = gui->getR(colorsFile.values["fg"]);
        fgColor.g = gui->getG(colorsFile.values["fg"]);
        fgColor.b = gui->getB(colorsFile.values["fg"]);
        fgColor.a = 255;
        secColor.r = gui->getR(colorsFile.values["sec"]);
        secColor.g = gui->getG(colorsFile.values["sec"]);
        secColor.b = gui->getB(colorsFile.values["sec"]);
        secColor.a = 255;
    }

    gui->themeFonts.openAllFonts(app.theme().fontPath(), renderer);

    // count, x_start, y_start, fontEnum, fontHeight, separationBetweenLines
    notificationLines.createAndSetDefaults(2, 10, 10, FONT_22_MED, 24, 8);

    staticElements.clear();
    frontElemets.clear();
    carouselGames.clear();
    carouselPositions.initCoverPositions();
    switchSet(currentSet, true);
    showSetName();

    gameName = "";
    publisher = "";
    year = "";
    players = "";
    cout << "Last Index" << app.session().launcher.selIndex << endl;
    if (app.session().launcher.selIndex != 0) {
        selGameIndex = app.session().launcher.selIndex;
        setInitialPositions(selGameIndex);
    }

    long time = gui->platform().ticks();

    cout << "Loading theme and creating objects" << endl;
    if (DirEntry::exists(app.theme().imagePath() + sep + "GR/AB_BG.png")) {
        staticMeta = true;
        background = addStaticElement(new PsObj("background", app.theme().imagePath() + sep + "GR/AB_BG.png"));
    } else {
        staticMeta = false;
        background = addStaticElement(new PsObj("background", app.theme().imagePath() + sep + "GR/JP_US_BG.png"));
    }

    background->x = 0;
    background->y = 0;
    background->visible = true;
    string footerFile = "";
    if (DirEntry::exists(app.theme().imagePath() + sep + "GR/Footer_AB.png")) {
        footerFile = "GR/Footer_AB.png";
    } else {
        footerFile = "GR/Footer.png";
    }
    PsObj *footer = addStaticElement(new PsObj("footer", app.theme().imagePath() + sep + footerFile));
    footer->y = SCREEN_HEIGHT - footer->h;
    footer->visible = true;

    playButton = addStaticElement(new PsObj("playButton", app.theme().imagePath() + sep + "GR/Acid_C_Btn.png"));
    playButton->y = 428;
    playButton->x = 540;
    playButton->visible = selGameIndex != -1;

    playText = addStaticElement(new PsZoomBtn("playText", app.theme().imagePath() + sep + "BMP_Text/Play_Text.png"));
    playText->y = 428;
    playText->x = 640 - 262 / 2;
    playText->visible = selGameIndex != -1;
    playText->ox = playText->x;
    playText->oy = playText->y;
    playText->lastTime = time;

    string settingsFile = "";
    if (DirEntry::exists(app.theme().imagePath() + sep + "CB/Function_AB.png")) {
        settingsFile = "/CB/Function_AB.png";
    } else {
        settingsFile = "/CB/Function_BG.png";
    }
    settingsBack = addStaticElement(new PsSettingsBack("playButton", app.theme().imagePath() + settingsFile));
    settingsBack->setCurLen(100);
    settingsBack->visible = true;

    meta = addStaticElement(new PsMeta("meta", app.theme().imagePath() + sep + "CB/PlayerOne.png"));
    meta->fonts = gui->themeFonts;
    meta->x = 785;
    meta->y = 285;
    meta->visible = true;
    if (selGameIndex != -1 && selGameIndexInCarouselGamesIsValid()) {
        meta->updateTexts(carouselGames[selGameIndex], fgColor);
    } else {
        bool internal {false};
        bool hd {false};
        bool locked {false};
        bool discs {0};
        bool favorite {false};
        bool play_using_ra {false};
        bool foreign {false};
        bool app {false};
        string last_played {""};
        meta->updateTexts(gameName, publisher, year, serial, region, players,
                          internal, hd, locked, discs, favorite, play_using_ra, foreign, app, last_played,
                          fgColor);
    }

    arrow = addStaticElement(new PsMoveBtn("arrow", app.theme().imagePath() + sep + "GR/arrow.png"));
    arrow->x = 640 - 12;
    arrow->y = 360;
    arrow->originaly = arrow->y;
    arrow->visible = false;

    xButton = addStaticElement(new PsObj("xbtn", app.theme().imagePath() + sep + "GR/X_Btn_ICN.png"));
    xButton->x = 605;
    xButton->y = 640;
    xButton->visible = true;

    oButton = addStaticElement(new PsObj("obtn", app.theme().imagePath() + sep + "GR/Circle_Btn_ICN.png"));
    oButton->x = 765;
    oButton->y = 640;
    oButton->visible = true;

    tButton = addStaticElement(new PsObj("tbtn", app.theme().imagePath() + sep + "GR/Tri_Btn_ICN.png"));
    tButton->x = 910;
    tButton->y = 640;
    tButton->visible = true;

    menu.reset(new PsMenu("menu", app.theme().imagePath()));
    menu->loadAssets();

    menuHead = addStaticElement(new PsCenterLabel("header"));
    menuHead->font = gui->themeFonts[FONT_28_BOLD];
    menuHead->visible = false;
    menuHead->y = 545;
    menuText = addStaticElement(new PsCenterLabel("menuText"));
    menuText->visible = false;
    menuText->font = gui->themeFonts[FONT_22_MED];
    menuText->y = 585;

    menuHead->setText(headers[0], fgColor);
    menuText->setText(texts[0], fgColor);


    sselector = addFrontElement(new PsStateSelector("selector"));
    sselector->font30 = gui->themeFonts[FONT_28_BOLD];
    sselector->font24 = gui->themeFonts[FONT_22_MED];
    sselector->visible = false;

    if (app.session().resumingGui) {
        cout << "Restoring GUI state" << endl;
        PsGamePtr &game = carouselGames[selGameIndex];

        if (app.session().emuMode == EmuMode::Pcsx) {
            if (game->isCleanExit()) {
                sselector->loadSaveStateImages(game, true);
                sselector->visible = true;
                state = STATE_RESUME;
            } else {
                notificationLines[1].setText(_("OOPS! Game crashed. Resume point not available."),
                                             DefaultShowingTimeout);
            }
        } else {
            notificationLines[1].setText(_("AutoBleem resume points not available in RetroArch."),
                                         DefaultShowingTimeout);
        }
    }


    //switchSet(currentSet,false);
    if ((currentSet == SET_RETROARCH) || (currentSet == SET_APPS)) {
        forceSettingsOnly();
    } else {
        if (menu->foreign) {
            showAllOptions();
        }
    }

    showSetName();
    updateMeta();

    if (selGameIndexInCarouselGamesIsValid()) {
        menu->setResumePic(carouselGames[selGameIndex]->findResumePicture());
    }
}

//*******************************
// GuiLauncher::freeAssets
//*******************************
// memory cleanup for assets disposal
void GuiLauncher::freeAssets() {
    for (auto &obj : staticElements) {
        obj->destroy();
    }
    for (auto &obj : frontElemets) {
        obj->destroy();
    }
    staticElements.clear();     // deletes the elements
    frontElemets.clear();
    settingsBack = nullptr; playButton = nullptr; playText = nullptr; meta = nullptr; background = nullptr;
    arrow = nullptr; xButton = nullptr; oButton = nullptr; tButton = nullptr; sselector = nullptr;
    menuHead = nullptr; menuText = nullptr;
    for (auto &game : carouselGames) {
        game.freeTex();
    }
    carouselGames.clear();
    if (menu) {
        menu->freeAssets();
        menu.reset();
    }
}

//*******************************
// GuiLauncher::init()
//*******************************
// run when screen is loaded
void GuiLauncher::init() {
    gui = Gui::getInstance();

    raIntegrator = RAIntegrator::getInstance();
    loadAssets();
}

//*******************************
// GuiLauncher::~GuiLauncher()
//*******************************
// run when screen is loaded
GuiLauncher::~GuiLauncher() {
    freeAssets();
}

//*******************************
// GuiLauncher::scrollLeft
//*******************************
// start scroll animation to next game
void GuiLauncher::scrollLeft(int speed) {
    scrolling = true;
    long time = gui->platform().ticks();
    for (auto &game : carouselGames) {

        if (game.visible) {
            int nextIndex = game.screenPointIndex;

            if (game.screenPointIndex != 0) {
                nextIndex = game.screenPointIndex - 1;
            } else {
                game.visible = false;

            }
            game.destination = carouselPositions.coverPositions[nextIndex];
            game.animationDuration = speed;
            game.animationStart = time;

            game.screenPointIndex = nextIndex;
            game.current = game.actual;
        }
    }
}

//*******************************
// GuiLauncher::scrollRight
//*******************************
// start scroll animation to previous game
void GuiLauncher::scrollRight(int speed) {
    scrolling = true;
    long time = gui->platform().ticks();
    for (auto &game : carouselGames) {
        if (game.visible) {
            int nextIndex = game.screenPointIndex;
            if (game.screenPointIndex != carouselPositions.coverPositions.size() - 1) {
                nextIndex = game.screenPointIndex + 1;
            } else {
                game.visible = false;
            }
            game.destination = carouselPositions.coverPositions[nextIndex];
            game.animationDuration = speed;
            game.animationStart = time;

            game.screenPointIndex = nextIndex;
            game.current = game.actual;
        }
    }
}

//*******************************
// GuiLauncher::updateVisibility
//*******************************
// update potentially visible covers to save the memory
void GuiLauncher::updateVisibility() {
    bool allAnimationFinished = true;
    for (const auto &game : carouselGames) {
        if ((game.animationStart != 0) && game.visible) {
            allAnimationFinished = false;
        }
    }

    if (allAnimationFinished && scrolling) {
        setInitialPositions(selGameIndex);
        scrolling = false;
    }
}

//*******************************
// GuiLauncher::updatePositions
//*******************************
// this method runs during the loop to update positions of the covers during animation
void GuiLauncher::updatePositions() {
    long currentTime = gui->platform().ticks();
    for (auto &game : carouselGames) {
        if (game.visible) {
            if (game.animationStart != 0) {
                long position = currentTime - game.animationStart;
                float delta = position * 1.0f / game.animationDuration;
                game.actual.x = game.current.x + (game.destination.x - game.current.x) * delta;
                game.actual.y = game.current.y + (game.destination.y - game.current.y) * delta;
                game.actual.scale = game.current.scale + (game.destination.scale - game.current.scale) * delta;
                game.actual.shade = game.current.shade + (game.destination.shade - game.current.shade) * delta;

                if (delta > 1.0f) {
                    game.actual = game.destination;
                    game.current = game.destination;
                    game.animationStart = 0;
                }
            }
        }
    }
    updateVisibility();
}

//*******************************
// GuiLauncher::render
//*******************************
// render method called every loop
void GuiLauncher::render() {
    if (sselector != nullptr) {
        sselector->frame = menu->savestate;
    }

    renderer.setDrawColor(ableem::Color(0x00, 0x00, 0x00, 0x00));
    renderer.clear();

    for (auto &obj : staticElements) {

        obj->render();
    }
    // covers render

    if (!carouselGames.empty()) {
        for (const auto &game : carouselGames) {
            if (game.visible) {
                ableem::Texture currentGameTex = game.coverPng;
                PsScreenpoint point = game.actual;

                ableem::Rect coverRect;
                coverRect.x = point.x;
                coverRect.y = point.y;
                coverRect.w = 226 * point.scale;
                coverRect.h = 226 * point.scale;

                ableem::Rect fullRect;
                fullRect.x = 0;
                fullRect.y = 0;
                fullRect.w = 226;
                fullRect.h = 226;
                currentGameTex.setColorMod(ableem::Color(point.shade, point.shade, point.shade));
                renderer.copy(currentGameTex, &fullRect, &coverRect);
            }
        }
    }

    menu->render();

    auto font24 = gui->themeFonts[FONT_22_MED];
    gui->renderText_WithColor(font24, _("Enter"), 638, 640, secColor);
    gui->renderText_WithColor(font24, _("Cancel"), 800, 640, secColor);
    gui->renderText_WithColor(font24, _("Button Guide"), 945, 640, secColor);

    notificationLines.tickTock();

    for (auto &obj : frontElemets)
        obj->render();

    gui->renderer().present();
}

//*******************************
// GuiLauncher::nextCarouselGame
//*******************************
// handler of next game
void GuiLauncher::nextCarouselGame(int speed) {
    app.audio().cursor.play();
    scrollLeft(speed);
    selGameIndex++;
    if (selGameIndex >= carouselGames.size()) {
        selGameIndex = 0;
    }
    updateMeta();
    if (selGameIndexInCarouselGamesIsValid())
        menu->setResumePic(carouselGames[selGameIndex]->findResumePicture());
}

//*******************************
// GuiLauncher::prevCarouselGame
//*******************************
// handler of prev game
void GuiLauncher::prevCarouselGame(int speed) {
    app.audio().cursor.play();
    scrollRight(speed);
    selGameIndex--;
    if (selGameIndex < 0) {
        selGameIndex = carouselGames.size() - 1;
    }
    updateMeta();
    if (selGameIndexInCarouselGamesIsValid())
        menu->setResumePic(carouselGames[selGameIndex]->findResumePicture());
}

//*******************************
// GuiLauncher::getNextId
//*******************************
// just small method to get next / prev game
int GuiLauncher::getNextId(int id) {
    int next = id + 1;
    if (next >= carouselGames.size()) {
        return 0;
    }
    return next;
}

//*******************************
// GuiLauncher::getPreviousId
//*******************************
int GuiLauncher::getPreviousId(int id) {
    int prev = id - 1;
    if (prev < 0) {
        return carouselGames.size() - 1;
    }
    return prev;
}


//*******************************
// GuiLauncher::setInitialPositions
//*******************************
// initialize a table with positions for covers
void GuiLauncher::setInitialPositions(int selected) {
    for (auto &game : carouselGames) {
        game.visible = false;
    }

    carouselGames[selected].visible = true;
    carouselGames[selected].current = this->carouselPositions.coverPositions[6];
    carouselGames[selected].screenPointIndex = 6;

    int prev = getPreviousId(selected);
    if (!carouselGames[prev].visible) {
        carouselGames[prev].current = this->carouselPositions.coverPositions[5];
        carouselGames[prev].visible = true;
        carouselGames[prev].screenPointIndex = 5;
    }
    prev = getPreviousId(prev);
    if (!carouselGames[prev].visible) {
        carouselGames[prev].current = this->carouselPositions.coverPositions[4];
        carouselGames[prev].visible = true;
        carouselGames[prev].screenPointIndex = 4;
    }
    prev = getPreviousId(prev);
    if (!carouselGames[prev].visible) {
        carouselGames[prev].current = this->carouselPositions.coverPositions[3];
        carouselGames[prev].visible = true;
        carouselGames[prev].screenPointIndex = 3;
    }
    prev = getPreviousId(prev);
    if (!carouselGames[prev].visible) {
        carouselGames[prev].current = this->carouselPositions.coverPositions[2];
        carouselGames[prev].visible = true;
        carouselGames[prev].screenPointIndex = 2;
    }
    prev = getPreviousId(prev);
    if (!carouselGames[prev].visible) {
        carouselGames[prev].current = this->carouselPositions.coverPositions[1];
        carouselGames[prev].visible = true;
        carouselGames[prev].screenPointIndex = 1;
    }
    prev = getPreviousId(prev);
    if (!carouselGames[prev].visible) {
        carouselGames[prev].current = this->carouselPositions.coverPositions[0];
        carouselGames[prev].visible = true;
        carouselGames[prev].screenPointIndex = 0;
    }

    int next = getNextId(selected);
    if (!carouselGames[next].visible) {
        carouselGames[next].current = this->carouselPositions.coverPositions[7];
        carouselGames[next].visible = true;
        carouselGames[next].screenPointIndex = 7;
    }
    next = getNextId(next);
    if (!carouselGames[next].visible) {
        carouselGames[next].current = this->carouselPositions.coverPositions[8];
        carouselGames[next].visible = true;
        carouselGames[next].screenPointIndex = 8;
    }
    next = getNextId(next);
    if (!carouselGames[next].visible) {
        carouselGames[next].current = this->carouselPositions.coverPositions[9];
        carouselGames[next].visible = true;
        carouselGames[next].screenPointIndex = 9;
    }
    next = getNextId(next);
    if (!carouselGames[next].visible) {
        carouselGames[next].current = this->carouselPositions.coverPositions[10];
        carouselGames[next].visible = true;
        carouselGames[next].screenPointIndex = 10;
    }
    next = getNextId(next);
    if (!carouselGames[next].visible) {
        carouselGames[next].current = this->carouselPositions.coverPositions[11];
        carouselGames[next].visible = true;
        carouselGames[next].screenPointIndex = 11;
    }
    next = getNextId(next);
    if (!carouselGames[next].visible) {
        carouselGames[next].current = this->carouselPositions.coverPositions[12];
        carouselGames[next].visible = true;
        carouselGames[next].screenPointIndex = 12;
    }

    for (auto &game : carouselGames) {
        game.actual = game.current;
        game.destination = game.current;
        if (game.visible) {
            game.loadTex(renderer);
        } else {
            game.freeTex();
        }
    }
}

//*******************************
// GuiLauncher::moveMainCover
//*******************************
void GuiLauncher::moveMainCover(int state) {
    if (selGameIndex == -1) {
        return;
    }
    PsScreenpoint point1;
    point1.x = 640 - 113;
    point1.y = 180;
    point1.scale = 1;
    point1.shade = 255;

    PsScreenpoint point2;
    point2.x = 640 - 113;
    point2.y = 90;
    point2.scale = 1;
    point2.shade = 220;

    long time = gui->platform().ticks();

    if (selGameIndexInCarouselGamesIsValid()) {
        if (state == STATE_GAMES) {
            carouselGames[selGameIndex].destination = point1;
            carouselGames[selGameIndex].animationStart = time;
            carouselGames[selGameIndex].animationDuration = 200;
        } else {
            carouselGames[selGameIndex].destination = point2;
            carouselGames[selGameIndex].animationStart = time;
            carouselGames[selGameIndex].animationDuration = 200;
        }
    }
}

//*******************************
// GuiLauncher::switchState
//*******************************
void GuiLauncher::switchState(int state, int time) {
    if (state == STATE_GAMES) {
        app.audio().home_up.play();
        settingsBack->animEndTime = time + 100;
        settingsBack->nextLen = 100;
        playButton->visible = true;
        playText->visible = true;
        if (!staticMeta) {
            meta->animEndTime = time + 200;
            meta->nextPos = 285;
            meta->prevPos = meta->y;
        }
        this->state = STATE_GAMES;
        arrow->visible = false;
        arrow->animationStarted = time;
        menu->duration = 200;
        menu->targety = 520;
        menu->animationStarted = time;
        menu->active = false;
        menuHead->visible = false;
        menuText->visible = false;

        moveMainCover(state);
    } else {
        app.audio().home_down.play();
        settingsBack->animEndTime = time + 100;
        settingsBack->nextLen = 280;
        playButton->visible = false;
        playText->visible = false;
        if (!staticMeta) {
            meta->animEndTime = time + 200;
            meta->nextPos = 215;
            meta->prevPos = meta->y;
        }
        this->state = STATE_SET;
        arrow->visible = true;
        arrow->animationStarted = time;
        menu->duration = 200;
        menu->targety = 440;
        menu->animationStarted = time;
        menu->active = true;
        menuHead->visible = true;
        menuText->visible = true;
        moveMainCover(state);
    }
}

//*******************************
// GuiLauncher::forceSettingsOnly
//*******************************
void GuiLauncher::forceSettingsOnly() {
    menu->foreign = true;
    menu->selOption = 0;
    menu->x = 640 - 118 / 2;
    menu->ox = menu->x;
    menu->xoff[0] = 0;
    menu->xoff[1] = 0;
    menu->xoff[2] = 0;
    menu->xoff[3] = 0;

    menu->direction = 0;
    menu->duration = 100;
    menu->animationStarted = 0;
}

//*******************************
// GuiLauncher::showAllOptions
//*******************************
void GuiLauncher::showAllOptions() {
    menu->foreign = false;
    menu->selOption = 0;
    menu->x = 640 - 118 / 2;
    menu->ox = menu->x;
    menu->xoff[0] = 0;
    menu->xoff[1] = 0;
    menu->xoff[2] = 0;
    menu->xoff[3] = 0;
    menu->animationStarted = 0;
}