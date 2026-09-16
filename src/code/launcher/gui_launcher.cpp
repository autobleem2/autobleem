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
    if (carousel.games.empty()) {
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
    if (carousel.selectedIsValid())
        meta->updateTexts(carousel.games[carousel.selected], fgColor);
}

//*******************************
// GuiLauncher::rememberSelection
//*******************************
// Hands the carousel's position back to the Session, so pressing Start later reopens it where it was.
// Called on every path that starts a game.
//
// The PS1 sub-set is the one field not written back from another set: loadAssets() does not restore it
// unless the PS1 set is showing, so this screen's copy would be a default, and writing that back would
// lose the sub-set the user actually left the PS1 carousel in. The two halves have always behaved this
// way - worth revisiting as a behaviour question, not as part of a structural move.
//*******************************
void GuiLauncher::rememberSelection() {
    if (carousel.selectedIsValid())
        selection.gameIndex = carousel.selected;

    Ps1SelectState rememberedPS1SubSet = app.session().launcher.ps1SelectState;
    app.session().launcher = selection;
    if (selection.set != GameSet::PS1)
        app.session().launcher.ps1SelectState = rememberedPS1SubSet;
}

//*******************************
// GuiLauncher::switchSet
//*******************************
void GuiLauncher::switchSet(GameSet newSet, bool noForce) {     // Warning: newSet is not used.  probably not the intent.
    cout << "Switching to Set: " << static_cast<int>(selection.set) << endl;

    cout << "Reloading games list" << endl; // get fresh list of games for this set
    // which games, and in what order, is GameQueryService's question. It may adjust the selection: the PS1
    // sub-set falls back off the internal-games views when origames is off, and the sub-dir view fills in
    // its row name.
    PsGames gamesList = app.gameQuery().gamesFor(selection);
    cout << "Games Sorted" << endl;
    carousel.setGames(gamesList);

    if (!noForce) {
        if ((selection.set == GameSet::RetroArch) || (selection.set == GameSet::Apps)) {
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
    assert(setPS1SubStateNames.size() == static_cast<size_t>(Ps1SelectState::GamesSubdir) + 1);
    assert(setNames.size() == static_cast<size_t>(GameSetLast) + 1);

    string numGames = " (" + to_string(carousel.numberOfNonDuplicatedGames) + " " + _("games") + ")";

    long timeout = Util::toInt(app.config().inifile.values["showingtimeout"], 0) * TicksPerSecond;

    if (selection.set == GameSet::PS1) {
        string name = setPS1SubStateNames[static_cast<int>(selection.ps1SelectState)];
        if (selection.ps1SelectState == Ps1SelectState::GamesSubdir) {
            name += selection.usbGameDirName;
        }
        notificationLines[0].setText(name + numGames, timeout);
    } else if (selection.set == GameSet::RetroArch) {
        string playlist = DirEntry::getFileNameWithoutExtension(selection.raPlaylistName);
        notificationLines[0].setText(setNames[static_cast<int>(selection.set)] + playlist + " " + numGames, timeout);
    } else if (selection.set == GameSet::Apps) {
        notificationLines[0].setText(setNames[static_cast<int>(selection.set)] + numGames, timeout);
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
        raPlaylists = app.retroArch().playlistNames();
    }
    vector<string> headers = {_("SETTINGS"), _("GAME"), _("MEMORY CARD"), _("RESUME")};
    vector<string> texts = {_("Customize AutoBleem settings"), _("Edit game parameters"),
                            _("Edit Memory Card information"), _("Resume game from saved state point")};

    selection = app.session().launcher;
    if (selection.set != GameSet::PS1)
        selection.ps1SelectState = Ps1SelectState::AllGames;   // see rememberSelection()
    if (selection.raPlaylistIndex < raPlaylists.size())
        selection.raPlaylistName = raPlaylists[selection.raPlaylistIndex];
    // also into the Session directly: rememberSelection() only runs when a game starts, and leaving the
    // launcher with Circle should not leave a stale playlist name behind.
    if (app.session().launcher.raPlaylistIndex < raPlaylists.size())
        app.session().launcher.raPlaylistName = raPlaylists[app.session().launcher.raPlaylistIndex];
#if 0
    if (app.session().launcher.raPlaylistName != "")
    {
        selection.raPlaylistName = app.session().launcher.raPlaylistName;
        //app.session().launcher.raPlaylistName = "";
    }
#endif

    for (int i = 0; i < 100; i++) {
        gui->input().flushEvents();
    }

    IniFile colorsFile;
    if (DirEntry::exists(app.theme().path() + sep + "colors.ini")) {
        colorsFile.load(app.theme().path() + sep + "colors.ini");
        fgColor.r = gui->text().getR(colorsFile.values["fg"]);
        fgColor.g = gui->text().getG(colorsFile.values["fg"]);
        fgColor.b = gui->text().getB(colorsFile.values["fg"]);
        fgColor.a = 255;
        secColor.r = gui->text().getR(colorsFile.values["sec"]);
        secColor.g = gui->text().getG(colorsFile.values["sec"]);
        secColor.b = gui->text().getB(colorsFile.values["sec"]);
        secColor.a = 255;
    }

    gui->assets().themeFonts.openAllFonts(app.theme().fontPath(), renderer);

    // count, x_start, y_start, fontEnum, fontHeight, separationBetweenLines
    notificationLines.createAndSetDefaults(2, 10, 10, FONT_22_MED, 24, 8);

    staticElements.clear();
    frontElemets.clear();
    carousel.games.clear();
    carousel.initPositions();
    switchSet(selection.set, true);
    showSetName();

    gameName = "";
    publisher = "";
    year = "";
    players = "";
    cout << "Last Index" << selection.gameIndex << endl;
    if (selection.gameIndex != 0) {
        carousel.selected = selection.gameIndex;
        carousel.setInitialPositions(carousel.selected);
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
    playButton->visible = carousel.selected != -1;

    playText = addStaticElement(new PsZoomBtn("playText", app.theme().imagePath() + sep + "BMP_Text/Play_Text.png"));
    playText->y = 428;
    playText->x = 640 - 262 / 2;
    playText->visible = carousel.selected != -1;
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
    meta->fonts = gui->assets().themeFonts;
    meta->x = 785;
    meta->y = 285;
    meta->visible = true;
    if (carousel.selected != -1 && carousel.selectedIsValid()) {
        meta->updateTexts(carousel.games[carousel.selected], fgColor);
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
    menuHead->font = gui->assets().themeFonts[FONT_28_BOLD];
    menuHead->visible = false;
    menuHead->y = 545;
    menuText = addStaticElement(new PsCenterLabel("menuText"));
    menuText->visible = false;
    menuText->font = gui->assets().themeFonts[FONT_22_MED];
    menuText->y = 585;

    menuHead->setText(headers[0], fgColor);
    menuText->setText(texts[0], fgColor);


    sselector = addFrontElement(new PsStateSelector("selector"));
    sselector->font30 = gui->assets().themeFonts[FONT_28_BOLD];
    sselector->font24 = gui->assets().themeFonts[FONT_22_MED];
    sselector->visible = false;

    if (app.session().resumingGui) {
        cout << "Restoring GUI state" << endl;
        PsGamePtr &game = carousel.games[carousel.selected];

        if (app.session().emuMode == EmuMode::Pcsx) {
            if (app.resumePoints().exitedCleanly(*game)) {
                sselector->loadSaveStateImages(game, true);
                sselector->visible = true;
                state = LauncherScreenState::Resume;
            } else {
                notificationLines[1].setText(_("OOPS! Game crashed. Resume point not available."),
                                             DefaultShowingTimeout);
            }
        } else {
            notificationLines[1].setText(_("AutoBleem resume points not available in RetroArch."),
                                         DefaultShowingTimeout);
        }
    }


    //switchSet(selection.set,false);
    if ((selection.set == GameSet::RetroArch) || (selection.set == GameSet::Apps)) {
        forceSettingsOnly();
    } else {
        if (menu->foreign) {
            showAllOptions();
        }
    }

    showSetName();
    updateMeta();

    if (carousel.selectedIsValid()) {
        menu->setResumePic(app.resumePoints().lastPicture(*carousel.games[carousel.selected]));
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
    for (auto &game : carousel.games) {
        game.freeTex();
    }
    carousel.games.clear();
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
    carousel.render();

    menu->render();

    auto font24 = gui->assets().themeFonts[FONT_22_MED];
    gui->text().renderText_WithColor(font24, _("Enter"), 638, 640, secColor);
    gui->text().renderText_WithColor(font24, _("Cancel"), 800, 640, secColor);
    gui->text().renderText_WithColor(font24, _("Button Guide"), 945, 640, secColor);

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
    carousel.scrollLeft(speed);
    carousel.selectNext();
    updateMeta();
    if (carousel.selectedIsValid())
        menu->setResumePic(app.resumePoints().lastPicture(*carousel.games[carousel.selected]));
}

//*******************************
// GuiLauncher::prevCarouselGame
//*******************************
// handler of prev game
void GuiLauncher::prevCarouselGame(int speed) {
    app.audio().cursor.play();
    carousel.scrollRight(speed);
    carousel.selectPrevious();
    updateMeta();
    if (carousel.selectedIsValid())
        menu->setResumePic(app.resumePoints().lastPicture(*carousel.games[carousel.selected]));
}

//*******************************
// GuiLauncher::switchState
//*******************************
void GuiLauncher::switchState(LauncherScreenState state, int time) {
    if (state == LauncherScreenState::Games) {
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
        this->state = LauncherScreenState::Games;
        arrow->visible = false;
        arrow->animationStarted = time;
        menu->duration = 200;
        menu->targety = 520;
        menu->animationStarted = time;
        menu->active = false;
        menuHead->visible = false;
        menuText->visible = false;

        carousel.moveMainCover(state == LauncherScreenState::Games);
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
        this->state = LauncherScreenState::Set;
        arrow->visible = true;
        arrow->animationStarted = time;
        menu->duration = 200;
        menu->targety = 440;
        menu->animationStarted = time;
        menu->active = true;
        menuHead->visible = true;
        menuText->visible = true;
        carousel.moveMainCover(state == LauncherScreenState::Games);
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