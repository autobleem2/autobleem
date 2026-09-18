//
// GuiLauncher, the screen half: assets, the sets and the metadata panel, the settings overlay's state
// transitions, and render(). Input is launcher_input.cpp, the sub-screen actions launcher_actions.cpp.
//

#include "evoui_launcher.h"
#include "../../gui/gui.h"
#include "../../gui/menus/gui_options_menu.h"
#include "../../gui/screens/gui_confirm.h"
#include <algorithm>
#include <iostream>
#include "evoui_mc_manager.h"
#include <cassert>
#include <memory>
#include <ableem/engine/log.h>

using namespace std;

const ableem::Color brightWhite = {255, 255, 255, 255};

//*******************************
// GuiLauncher::updateMeta
//*******************************
// just update metadata section to be visible on the screen
void GuiLauncher::updateMeta(bool withSnap) {
    if (carousel.games.empty()) {
        gameName = "";
        bool internal{false};
        bool hd{false};
        bool locked{false};
        bool discs{false};
        bool favorite{false};
        bool play_using_ra{false};
        bool foreign{false};
        bool app{false};
        string last_played{""};
        meta->updateTexts(gameName, publisher, year, serial, region, players, internal, hd, locked, discs, favorite,
                          foreign, play_using_ra, app, last_played, fgColor);
        return;
    }
    if (carousel.selectedIsValid())
        meta->updateTexts(carousel.games[carousel.selected], fgColor);
    showOptions(); // a mixed set (Lightgun) changes game type as the carousel moves
    if (withSnap)
        loadSnap();
}

//*******************************
// GuiLauncher::finishSettleLoads
//*******************************
void GuiLauncher::finishSettleLoads() {
    settleLoadsPending = false;
    loadSnap();
    if (carousel.selectedIsValid())
        menu->setResumePic(app.resumePoints().lastPicture(*carousel.games[carousel.selected]));
}

//*******************************
// GuiLauncher::loadSnap
//*******************************
// the selected game's screenshot: the path the scan cached while its file exists, else a look in the
// thumbnails tree (a RetroArch game, an internal game). Only when the theme draws it.
void GuiLauncher::loadSnap() {
    const ableem::ThemeRect &panel = app.theme().launcher().snapPanel;
    if (!panel.set || !carousel.selectedIsValid()) {
        snapTex = ableem::Texture();
        snapForGameId = -1;
        return;
    }
    const PsGame &game = *carousel.games[carousel.selected];
    if (snapForGameId == game.gameId && snapForInternal == game.internal && snapTex.valid())
        return;
    snapForGameId = game.gameId;
    snapForInternal = game.internal;
    snapTex = ableem::Texture();
    if (game.app)
        return;

    string path = game.snapPath;
    if (path.empty() || !DirEntry::exists(path)) {
        if (game.foreign)
            path = app.thumbnails().findSnap(game.db_name, game.title, game.image_path);
        else
            path = app.thumbnails().findSnap(ableem::ThumbnailLookup::PlayStationDbName, game.title,
                                             game.folder + sep + game.base, game.recordName);
    }
    if (!path.empty())
        snapTex = ableem::Texture::loadFile(renderer, path);
}

//*******************************
// GuiLauncher::renderSnap
//*******************************
void GuiLauncher::renderSnap() {
    const ableem::ThemeRect &panel = app.theme().launcher().snapPanel;
    if (!panel.set || !snapTex.valid())
        return;
    ableem::Size s = snapTex.size();
    if (s.w <= 0 || s.h <= 0)
        return;
    // aspect-fit inside the panel, centred
    ableem::Rect dst;
    if (s.w * panel.h > s.h * panel.w) { // wider than the panel
        dst.w = panel.w;
        dst.h = panel.w * s.h / s.w;
    } else {
        dst.h = panel.h;
        dst.w = panel.h * s.w / s.h;
    }
    dst.x = panel.x + (panel.w - dst.w) / 2;
    dst.y = panel.y + (panel.h - dst.h) / 2;
    renderer.copy(snapTex, nullptr, &dst);
}

//*******************************
// GuiLauncher::selectedIsPs1
//*******************************
bool GuiLauncher::selectedIsPs1() const {
    return carousel.selectedIsValid() && !carousel.games[carousel.selected]->foreign;
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
void GuiLauncher::switchSet(GameSet newSet, bool noForce) { // Warning: newSet is not used.  probably not the intent.
    PLOG_DEBUG << "Switching to Set: " << static_cast<int>(selection.set);

    PLOG_DEBUG << "Reloading games list"; // get fresh list of games for this set
    // which games, and in what order, is GameQueryService's question. It may adjust the selection: the PS1
    // sub-set falls back off the internal-games views when origames is off, and the sub-dir view fills in
    // its row name.
    PsGames gamesList = app.gameQuery().gamesFor(selection);
    PLOG_DEBUG << "Games Sorted";
    carousel.setGames(gamesList, selection.set == GameSet::PS1 ? BoxKind::JewelCase : BoxKind::BigBox);

    if (!noForce) {
        showOptions();
    }
}

//*******************************
// GuiLauncher::showSetName
//*******************************
void GuiLauncher::showSetName() {
    vector<string> setNames = {"Showing: PS1 games", // this is a dummy entry. setPS1SubStateNames is used.
                               _("Showing: Retroarch") + " ", _("Showing: Lightgun Games") + " ",
                               _("Showing: Apps") + " "};
    vector<string> setPS1SubStateNames = {_("Showing: All Games") + " ", _("Showing: Internal Games") + " ",
                                          _("Showing: Favorite Games") + " ", _("Showing: Game History") + " ",
                                          _("Showing: USB Games Directory:") + " "};
    assert(setPS1SubStateNames.size() == static_cast<size_t>(Ps1SelectState::GamesSubdir) + 1);
    assert(setNames.size() == static_cast<size_t>(GameSetLast) + 1);

    string numGames = " (" + to_string(carousel.games.size()) + " " + _("games") + ")";

    long timeout = Strings::toInt(app.config().inifile.values["showingtimeout"], 0) * TicksPerSecond;

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
// GuiLauncher::reloadGames
//*******************************
// re-runs the current set's query and re-selects the highlighted game by id. When that game is gone (its
// folder removed while the scanner watched, or merged into another) the first game of the set - or none -
// is highlighted instead, and a resume-point picker that was showing its slots is closed.
void GuiLauncher::reloadGames() {
    int keepGameId = -1;
    bool keepInternal = false;
    if (carousel.selectedIsValid()) {
        keepGameId = carousel.games[carousel.selected]->gameId;
        keepInternal = carousel.games[carousel.selected]->internal;
    }

    switchSet(selection.set, false);

    bool kept = false;
    if (keepGameId != -1) {
        for (int i = 0; i < static_cast<int>(carousel.games.size()); i++) {
            if (carousel.games[i]->gameId == keepGameId && carousel.games[i]->internal == keepInternal) {
                carousel.selected = i;
                kept = true;
                break;
            }
        }
    }
    if (carousel.selectedIsValid()) {
        carousel.setInitialPositions(carousel.selected);
    }

    if (!kept && state == LauncherScreenState::Resume) {
        // the picker was showing the slots of a game that no longer exists: close it as Circle does
        sselector->visible = false;
        arrow->visible = true;
        sselector->cleanSaveStateImages();
        state = LauncherScreenState::Set;
    }
    if (state != LauncherScreenState::Games) {
        // setInitialPositions put the selected cover in the row; with the menu open it belongs above it
        carousel.snapMainCover(false);
    }

    showSetName();
    updateMeta();
    if (carousel.selectedIsValid())
        menu->setResumePic(app.resumePoints().lastPicture(*carousel.games[carousel.selected]));

    scanRosterChangedSinceReload = false;
}

//*******************************
// GuiLauncher::scanStatusText
//*******************************
// the untranslated stage/detail/done/total from ScanUpdate, turned into the one line shown at the bottom of
// the screen - the same wording SplashScanProgress used to put on the splash for a blocking scan.
string GuiLauncher::scanStatusText(const ScanUpdate &update) const {
    switch (update.stage) {
    case ScanStage::Scanning:
        return _("Scanning...");
    case ScanStage::Game: {
        int percent = update.total > 0 ? (update.done * 100 / update.total) : 0;
        return _("Scanning") + " " + to_string(update.done) + "/" + to_string(update.total) + " (" +
               to_string(percent) + "%): " + update.detail;
    }
    case ScanStage::DecompressingEcm:
        return update.detail.empty() ? _("Decompressing ecm:") : update.detail;
    case ScanStage::UpdatingDatabase:
        return _("Updating regional.db...");
    case ScanStage::GameFailedVerify:
        return _("Game failed to verify:") + " " + DirEntry::getFileNameFromPath(update.detail);
    case ScanStage::MovingFile:
        return _("Moving :") + " " + update.detail;
    case ScanStage::MergingDiscs:
        return _("Merging discs:") + " " + update.detail;
    }
    return "";
}

//*******************************
// GuiLauncher::applyScanUpdate
//*******************************
// called once a frame (loop(), before render()) with whatever app.scans().poll() drained since the last
// frame. The roster reload is deliberately not per-event (no fine-grained carousel splicing): switchSet()
// re-running is cheap, and it is the one place duplicates-across-folders and sub-dir rows already get
// settled correctly, so reusing it here is both simpler and safer than a second code path for the same job.
void GuiLauncher::applyScanUpdate(const ScanUpdate &update) {
    if (update.progressed) {
        scanStatusLine.setText(scanStatusText(update), 0); // 0 = no timeout: stays up while scanning
    }

    if (!update.lastFailedGamePath.empty()) {
        notificationLines[1].setText(_("Game failed to verify:") + " " +
                                         DirEntry::getFileNameFromPath(update.lastFailedGamePath),
                                     DefaultShowingTimeout);
    }

    if (!update.addedGames.empty() || !update.updatedGames.empty() || !update.removedGameIds.empty())
        scanRosterChangedSinceReload = true;

    if (update.finished) {
        string text = _("Scan complete:") + " " + to_string(update.finishedGameCount) + " " + _("games");
        if (update.finishedFailedCount > 0)
            text += ", " + to_string(update.finishedFailedCount) + " " + _("failed");
        scanStatusLine.setText(text, DefaultShowingTimeout);
        scanRosterChangedSinceReload = true; // sub-dir rows and cross-folder duplicates only settle once done
    }

    // PS1/USB is the only set a games-directory scan can affect; leave RetroArch/Apps alone, and never
    // interrupt a scroll animation - reloadGames() repositions the carousel outright.
    if (scanRosterChangedSinceReload && selection.set == GameSet::PS1 && !carousel.scrolling) {
        reloadGames();
    }
}

//*******************************
// GuiLauncher::loadAssets
//*******************************
// load all assets needed by the screengame i
void GuiLauncher::loadAssets() {
    PLOG_DEBUG << "Loading playlists";
    raPlaylists.clear();
    if (DirEntry::exists(Env::getPathToRetroarchDir())) {
        raPlaylists = app.retroArch().playlistNames();
    }
    // the members, not locals: showOptions() reads them whenever the icon row changes (a local pair of the
    // same name here once left the members empty, and the first RetroArch game selected on a fresh screen
    // - every return from a RetroArch launch - crashed on headers[0])
    headers = {_("SETTINGS"), _("GAME"), _("MEMORY CARD"), _("RESUME")};
    texts = {_("Customize AutoBleem settings"), _("Edit game parameters"), _("Edit Memory Card information"),
             _("Resume game from saved state point")};

    selection = app.session().launcher;
    if (selection.set != GameSet::PS1)
        selection.ps1SelectState = Ps1SelectState::AllGames; // see rememberSelection()
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

    const LauncherTheme &theme = app.theme().launcher();
    if (theme.colors.text.set)
        fgColor = TextRenderer::toColor(theme.colors.text, 255);
    if (theme.colors.secondary.set)
        secColor = TextRenderer::toColor(theme.colors.secondary, 255);
    hintColor = theme.colors.hint.set ? TextRenderer::toColor(theme.colors.hint, 255) : secColor;

    // count, x_start, y_start, fontEnum, fontHeight, separationBetweenLines
    notificationLines.createAndSetDefaults(2, 10, 10, FONT_22_MED, 24, 8);

    scanStatusLine.x = 10;
    scanStatusLine.y = SCREEN_HEIGHT - 30;
    scanStatusLine.fontEnum = FONT_22_MED;
    scanStatusLine.textColor = brightWhite;
    scanStatusLine.text = "";
    scanStatusLine.timed = true;
    scanStatusLine.notificationTime = 0; // nothing to show until the first ScanUpdate arrives
    scanRosterChangedSinceReload = false;

    fadeAlpha = 255;
    fadeStart = gui->platform().ticks();

    // was the classic menu's gamepadNotice - shown once here since there is no classic menu screen to carry it
    if (gui->input().joystickCount() > gui->input().activePadCount()) {
        notificationLines[1].setText(
            _("NOTICE: At least one connected gamepad is not recognized. Use Hardware Information page to setup."),
            10 * TicksPerSecond);
    }

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
    PLOG_DEBUG << "Last Index " << selection.gameIndex;
    if (selection.gameIndex != 0) {
        carousel.selected = selection.gameIndex;
        carousel.setInitialPositions(carousel.selected);
    }

    long time = gui->platform().ticks();

    PLOG_DEBUG << "Loading theme and creating objects";
    staticMeta = !theme.metaPanelSlides;
    textShadow = !theme.textShadow.set || theme.textShadow; // a theme has to say no
    background = addStaticElement(new PsObj("background", theme.background));
    background->x = 0;
    background->y = 0;
    background->visible = true;

    PsObj *footer = addStaticElement(new PsObj("footer", theme.footer));
    footer->y = SCREEN_HEIGHT - footer->h;
    footer->visible = true;

    playButton = addStaticElement(new PsObj("playButton", theme.playButton));
    playButton->y = 428;
    playButton->x = 540;
    playButton->visible = carousel.selected != -1;

    playText = addStaticElement(new PsZoomBtn("playText", theme.playText));
    playText->y = 428;
    playText->x = 640 - 262 / 2;
    playText->visible = carousel.selected != -1;
    playText->ox = playText->x;
    playText->oy = playText->y;
    playText->lastTime = time;

    settingsBack = addStaticElement(new PsSettingsBack("playButton", theme.settingsPanel));
    settingsBack->setCurLen(100);
    settingsBack->visible = true;

    meta = addStaticElement(new PsMeta("meta", theme.metaPanel));
    meta->fonts = gui->assets().themeFonts;
    meta->x = 785;
    meta->y = 285;
    meta->visible = true;
    if (carousel.selected != -1 && carousel.selectedIsValid()) {
        meta->updateTexts(carousel.games[carousel.selected], fgColor);
    } else {
        bool internal{false};
        bool hd{false};
        bool locked{false};
        bool discs{false};
        bool favorite{false};
        bool play_using_ra{false};
        bool foreign{false};
        bool app{false};
        string last_played{""};
        meta->updateTexts(gameName, publisher, year, serial, region, players, internal, hd, locked, discs, favorite,
                          play_using_ra, foreign, app, last_played, fgColor);
    }

    arrow = addStaticElement(new PsMoveBtn("arrow", theme.arrow));
    arrow->x = 640 - 12;
    arrow->y = 360;
    arrow->originaly = arrow->y;
    arrow->visible = false;

    xButton = addStaticElement(new PsObj("xbtn", theme.hints.cross));
    xButton->x = 605;
    xButton->y = 640;
    xButton->visible = true;

    oButton = addStaticElement(new PsObj("obtn", theme.hints.circle));
    oButton->x = 765;
    oButton->y = 640;
    oButton->visible = true;

    tButton = addStaticElement(new PsObj("tbtn", theme.hints.triangle));
    tButton->x = 910;
    tButton->y = 640;
    tButton->visible = true;

    menu = std::make_unique<PsMenu>("menu", theme.menuIcons);

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
        PLOG_INFO << "Restoring GUI state";
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

    showOptions();
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
    staticElements.clear(); // deletes the elements
    frontElemets.clear();
    settingsBack = nullptr;
    playButton = nullptr;
    playText = nullptr;
    meta = nullptr;
    background = nullptr;
    arrow = nullptr;
    xButton = nullptr;
    oButton = nullptr;
    tButton = nullptr;
    sselector = nullptr;
    menuHead = nullptr;
    menuText = nullptr;
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

    // every text on this screen (meta panel, labels, notifications, the state selector) gets the halo
    // for the length of this frame, on the launcher's own setting; the classic screens shown from here
    // render on the classic one, which goes back at the end of the frame
    const TextRenderer::Shadow classicShadow = gui->text().shadow();
    TextRenderer::Shadow shadow;
    shadow.enabled = textShadow;
    gui->text().setShadow(shadow);

    for (auto &obj : staticElements) {

        obj->render();
    }
    carousel.render();
    renderSnap();

    menu->render();

    auto font24 = gui->assets().themeFonts[FONT_22_MED];
    gui->text().renderText_WithColor(font24, _("Enter"), 638, 640, hintColor);
    gui->text().renderText_WithColor(font24, _("Cancel"), 800, 640, hintColor);
    gui->text().renderText_WithColor(font24, _("Button Guide"), 945, 640, hintColor);

    notificationLines.tickTock();
    scanStatusLine.tickTock();

    for (auto &obj : frontElemets)
        obj->render();

    gui->text().setShadow(classicShadow);

    if (fadeAlpha > 0) {
        long elapsed = gui->platform().ticks() - fadeStart;
        fadeAlpha =
            elapsed >= LauncherFadeInDuration ? 0 : 255 - (255 * static_cast<int>(elapsed) / LauncherFadeInDuration);
        renderer.setDrawColor(ableem::Color(0, 0, 0, fadeAlpha));
        renderer.setBlendMode(ableem::BlendMode::Blend);
        renderer.fillRect();
    }

    gui->renderer().present();
}

//*******************************
// GuiLauncher::nextCarouselGame
//*******************************
// handler of next game
void GuiLauncher::nextCarouselGame(int speed, bool eased) {
    if (!carousel.canSelectNext()) {
        motionStart = 0; // a held stick stops at the end of the row rather than retrying every frame
        return;
    }
    app.audio().cursor.play();
    carousel.scrollLeft(speed, eased);
    carousel.selectNext();
    updateMeta(false);
    settleLoadsPending = true;
}

//*******************************
// GuiLauncher::prevCarouselGame
//*******************************
// handler of prev game
void GuiLauncher::prevCarouselGame(int speed, bool eased) {
    if (!carousel.canSelectPrevious()) {
        motionStart = 0;
        return;
    }
    app.audio().cursor.play();
    carousel.scrollRight(speed, eased);
    carousel.selectPrevious();
    updateMeta(false);
    settleLoadsPending = true;
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
// GuiLauncher::showOptions
//*******************************
void GuiLauncher::showOptions() {
    bool enabled[4] = {true, false, false, false}; // an App, or nothing selected: AutoBleem settings only
    if (carousel.selectedIsValid()) {
        const PsGame &game = *carousel.games[carousel.selected];
        if (!game.foreign) {
            enabled[1] = enabled[2] = enabled[3] = true; // a PS1 game: editor, memory cards, resume points
        } else if (!game.app) {
            enabled[1] = true; // a RetroArch game: its (light-gun) editor
        }
    }
    bool same = true;
    for (int i = 0; i < 4; i++)
        same = same && (menu->enabled[i] == enabled[i]);
    if (same)
        return; // the row is already right - do not disturb an open menu

    for (int i = 0; i < 4; i++)
        menu->enabled[i] = enabled[i];
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
    menuHead->setText(headers[0], fgColor);
    menuText->setText(texts[0], fgColor);
}