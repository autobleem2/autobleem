//
// GuiLauncher, the screen half: assets, the sets and the metadata panel, the settings overlay's state
// transitions, and render(). Input is launcher_input.cpp, the sub-screen actions launcher_actions.cpp.
//

#include "evoui_launcher.h"
#include "gui/gui.h"
#include "../../gui/menus/gui_options_menu.h"
#include "gui/screens/gui_confirm.h"
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
        // no game: the row keeps settings alone, and no screenshot of the last set's game stays up
        if (menu != nullptr)
            showOptions();
        if (withSnap)
            loadSnap();
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
// GuiLauncher::refuseLicenceProtected
//*******************************
bool GuiLauncher::refuseLicenceProtected() {
    if (!selectedIsPs1() || !carousel.games[carousel.selected]->licenceProtected)
        return false;
    app.audio().cancel.play();
    notificationLines[1].setText(_("This game is protected by its PSN licence and cannot be started"),
                                 2 * DefaultShowingTimeout);
    return true;
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
    settleEmptyRoster();
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
    // a library game is the same game by id; a playlist game's id is only its position in the playlist,
    // which a rewrite may have moved - its image path is what names it
    int keepGameId = -1;
    bool keepInternal = false;
    bool keepForeign = false;
    string keepImagePath;
    if (carousel.selectedIsValid()) {
        const PsGame &current = *carousel.games[carousel.selected];
        keepGameId = current.gameId;
        keepInternal = current.internal;
        keepForeign = current.foreign;
        keepImagePath = current.image_path;
    }

    switchSet(selection.set, false);

    bool kept = false;
    if (keepGameId != -1) {
        for (int i = 0; i < static_cast<int>(carousel.games.size()); i++) {
            const PsGame &game = *carousel.games[i];
            bool same = keepForeign ? (game.foreign && game.image_path == keepImagePath)
                                    : (!game.foreign && game.gameId == keepGameId && game.internal == keepInternal);
            if (same) {
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
    settleEmptyRoster();
}

//*******************************
// GuiLauncher::scanStatusText
//*******************************
// the untranslated stage/detail/done/total from ScanUpdate, turned into the one line shown at the bottom of
// the screen - the same wording SplashScanProgress used to put on the splash for a blocking scan.
void GuiLauncher::scanStatusText(const ScanUpdate &update, string &title, string &detail) const {
    // the count on the title when there is one ("Scanning 12/40"), the file or folder as the detail
    const string count = update.total > 0 ? " " + to_string(update.done) + "/" + to_string(update.total) : string();
    detail = update.detail;
    switch (update.stage) {
    case ScanStage::Scanning:
        title = _("Scanning...");
        break;
    case ScanStage::Game:
        title = _("Scanning") + count;
        break;
    case ScanStage::DecompressingEcm:
        title = _("Decompressing ecm:");
        break;
    case ScanStage::UpdatingDatabase:
        title = _("Updating regional.db...");
        detail.clear();
        break;
    case ScanStage::GameFailedVerify:
        title = _("Game failed to verify:");
        detail = DirEntry::getFileNameFromPath(update.detail);
        break;
    case ScanStage::MovingFile:
        title = _("Moving :");
        break;
    case ScanStage::MergingDiscs:
        title = _("Merging discs:");
        break;
    case ScanStage::ScanningRoms:
        title = _("Scanning ROMs") + count;
        break;
    case ScanStage::FetchingBoxArt:
        title = _("Fetching box art") + count;
        break;
    }
}

//*******************************
// GuiLauncher::applyScanUpdate
//*******************************
// called once a frame (loop(), before render()) with whatever app.scans().poll() drained since the last
// frame. The roster reload is deliberately not per-event (no fine-grained carousel splicing): switchSet()
// re-running is cheap, and it is the one place duplicates-across-folders and sub-dir rows already get
// settled correctly, so reusing it here is both simpler and safer than a second code path for the same job.
void GuiLauncher::applyScanUpdate(const ScanUpdate &update) {
    // a scanner processor at work: its title (and the game) on top, its stage under it, its percent as the
    // bar; its counter joins the title
    if (update.processorProgressed) {
        const ProcessorActivity &p = update.processor;
        string title = p.item.empty() ? p.title : p.title + " - " + p.item;
        if (p.total > 0)
            title += " " + to_string(p.done) + "/" + to_string(p.total);
        scanBubble.show(title, p.stage, p.percent < 0 ? 0 : p.percent, p.percent < 0 ? 0 : 100, 0);
    }
    // "Unzip: <warning>" / "Unzip, Crash: <warning>"; "Unzip failed (Crash) - see processors.log"
    for (const ProcessorNotice &n : update.processorNotices) {
        string text;
        if (n.failed)
            text = n.title + " " + _("failed") + (n.item.empty() ? "" : " (" + n.item + ")") + " - " +
                   _("see processors.log");
        else
            text = n.title + (n.item.empty() ? "" : ", " + n.item) + ": " + n.message;
        notificationLines[1].setText(text, 2 * DefaultShowingTimeout);
    }

    if (update.progressed) {
        string title, detail;
        scanStatusText(update, title, detail);
        // the bar only for a counted stage; 0 = the bubble stays until the next message or the summary
        scanBubble.show(title, detail, update.done, update.total, 0);
    }

    if (!update.addedGames.empty() || !update.updatedGames.empty() || !update.removedGameIds.empty())
        scanRosterChangedSinceReload = true;

    if (update.finished) {
        string text = to_string(update.finishedGameCount) + " " + _("games");
        if (update.finishedFailedCount > 0)
            text += ", " + to_string(update.finishedFailedCount) + " " + _("failed");
        if (update.finishedRomCount > 0)
            text += ", " + to_string(update.finishedRomCount) + " " + _("ROMs");
        scanBubble.show(_("Scan complete:"), text, 0, 0, 2 * DefaultShowingTimeout); // the summary, then gone
        scanRosterChangedSinceReload = true; // sub-dir rows and cross-folder duplicates only settle once done
    }

    // the ROM pass rewrote playlists (the service has re-read them by now): the playlist names may have
    // changed - a first ROM in a folder makes a playlist, the last one going empties it - and so may the
    // set on screen
    if (!update.playlistsWritten.empty()) {
        refreshPlaylistNames();
        if (selection.set == GameSet::RetroArch)
            scanRosterChangedSinceReload = true;
    }

    // covers arrived from the server: the lookup's directory listings are stale, and the RetroArch set's
    // carousel has covers to pick up (reloadGames() re-creates its textures)
    if (update.boxArtFetched > 0) {
        app.thumbnails().clearCache();
        if (selection.set == GameSet::RetroArch)
            scanRosterChangedSinceReload = true;
    }

    // a games-directory scan affects the PS1 set, a ROM pass the RetroArch one; leave the rest alone, and
    // never interrupt a scroll animation - reloadGames() repositions the carousel outright.
    bool setAffected = selection.set == GameSet::PS1 || selection.set == GameSet::RetroArch;
    if (scanRosterChangedSinceReload && setAffected && !carousel.scrolling) {
        reloadGames();
    }
}

//*******************************
// GuiLauncher::refreshPlaylistNames
//*******************************
// raPlaylists as RetroArchService lists them now, keeping the selected playlist by name where it still
// exists (its index may have moved), else the first one
void GuiLauncher::refreshPlaylistNames() {
    raPlaylists.clear();
    if (DirEntry::exists(Env::getPathToRetroarchDir()))
        raPlaylists = app.retroArch().playlistNames();

    auto pick = [&](GameSetSelection &sel) {
        auto it = find(raPlaylists.begin(), raPlaylists.end(), sel.raPlaylistName);
        if (it != raPlaylists.end()) {
            sel.raPlaylistIndex = static_cast<int>(it - raPlaylists.begin());
        } else if (!raPlaylists.empty()) {
            sel.raPlaylistIndex = 0;
            sel.raPlaylistName = raPlaylists[0];
        } else {
            sel.raPlaylistIndex = 0;
            sel.raPlaylistName = "";
        }
    };
    pick(selection);
    pick(app.session().launcher);
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
    headers = {_("QUICK MENU"), _("GAME"), _("MEMORY CARD"), _("RESUME")};
    texts = {_("Re-Scan, Store, Network and more"), _("Edit game parameters"), _("Edit Memory Card information"),
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
    notificationLines.create(2);

    scanRosterChangedSinceReload = false;

    fadeAlpha = 255;
    fadeStart = gui->platform().ticks();

    // was the classic menu's gamepadNotice - shown once here since there is no classic menu screen to carry it
    // - pointing at Network & Controllers (its controller mapping wizard) where an extension provides it
    if (gui->input().joystickCount() > gui->input().activePadCount()) {
        notificationLines[1].setText(
            networkProvided()
                ? _("NOTICE: At least one connected gamepad is not recognized. Set it up in Network & Controllers.")
                : _("NOTICE: At least one connected gamepad is not recognized."),
            10 * TicksPerSecond);
    }

    // every element below is built at rest in the Games layout (the menu row closed, the play button shown,
    // the main cover in the row) - so the state is Games too, whatever it was when this was called (Options
    // or an editor closing from the open menu used to leave the state Set, or a row rebuilt open, over a
    // screen laid out closed). An empty set then opens the row (settleEmptyRoster), a resume the picker.
    state = LauncherScreenState::Games;
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

    // built lazily: render() calls updateHintsIfNeeded() every frame, which rebuilds only when the state,
    // selection or language actually changed. Force that on the first frame of this fresh screen.
    lastHintSignature.clear();

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

    // a crash's logs, which the rc scripts took from RAM to the stick (docs/quiet-stick-plan.md) - said once,
    // the first time the launcher is shown after it; over the resume messages above, which it explains
    const string crashLogs = Env::takeNewCrashLogs();
    if (!crashLogs.empty()) {
        notificationLines[1].setText(_("Crash logs:") + " System/Logs/" + crashLogs, 10 * TicksPerSecond);
    }

    showOptions();
    showSetName();
    updateMeta();

    if (carousel.selectedIsValid()) {
        menu->setResumePic(app.resumePoints().lastPicture(*carousel.games[carousel.selected]));
    }
    settleEmptyRoster();
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
// GuiLauncher::hintSignature
//*******************************
// everything buildHintLines() reads, as a short string - updateHintsIfNeeded() rebuilds and re-lays-out the
// two hint lines only when this actually changes, so render() can call it every frame for free (the "cache
// the layout" rule: the layout is redone on a state/selection/language change, not per frame).
string GuiLauncher::hintSignature() const {
    string sig = app.lang().currentLanguage();
    sig += "|s" + to_string(static_cast<int>(state));
    if (state == LauncherScreenState::Set) {
        sig += "|o" + to_string(menu ? menu->selOption : -1);
        sig += carousel.games.empty() ? "|empty" : "";
    } else if (state == LauncherScreenState::Resume) {
        if (sselector != nullptr) {
            sig += "|op" + to_string(sselector->operation);
            sig += "|sl" + to_string(sselector->selSlot);
            sig += sselector->slotActive[sselector->selSlot] ? "|act" : "";
        }
    } else { // Games
        if (carousel.games.empty()) {
            sig += "|empty";
        } else if (carousel.selectedIsValid()) {
            const PsGame &g = *carousel.games[carousel.selected];
            sig += g.foreign ? "|f1" : "|f0";
            sig += g.app ? "|a1" : "|a0";
        }
        sig += Env::retroArchInstalled() ? "|ra" : "";
    }
    return sig;
}

//*******************************
// GuiLauncher::buildHintLines
//*******************************
// line 1: what acts on the current selection right now. Line 2: what always works (Select/Start/Guide/
// System). "Play" not "Enter" (an App: "Start"); Circle only appears where it does something; L2+R2 says
// "System", never "Options" (see docs/theme-format.md's hintBar entry and PLANS-menu-hints-quickmenu.md,
// section E). The icon-row and Resume lines only ever fill line 1 - Select/Start do nothing there, so line 2
// stays just the Guide/System pair (Resume: System alone - Circle already means Back/Don't save there).
void GuiLauncher::buildHintLines(std::vector<Hint> &line1, std::vector<Hint> &line2) const {
    line1.clear();
    line2.clear();
    if (state == LauncherScreenState::Set) {
        // the game menu's icon row. Up closes it back to the games - or, on an empty set, opens the Quick
        // menu instead (settleEmptyRoster keeps this state open on an empty roster - there is no Games
        // state to show, so this is the "empty set" row the design calls out on its own)
        string openLabel = _("Open:");
        if (menu != nullptr && menu->selOption >= 0 && static_cast<size_t>(menu->selOption) < headers.size())
            openLabel += " " + headers[menu->selOption];
        line1.push_back({"|@X|", openLabel});
        if (carousel.games.empty()) {
            line1.push_back({"|@Up|", _("Quick menu")});
            line2.push_back({"|@Select|", _("Games shown")});
        } else {
            line1.push_back({"|@Left|/|@Right|", _("Choose")});
            line1.push_back({"|@Up|", _("Back to games")});
            line2.push_back({"|@T|", _("Guide")});
        }
        line2.push_back({"|@L2+R2|", _("System")});
        return;
    }
    if (state == LauncherScreenState::Resume) {
        if (sselector == nullptr)
            return;
        const string slotLabel = to_string(sselector->selSlot + 1);
        if (sselector->operation == OP_LOAD) {
            line1.push_back({"|@X|", _("Resume slot") + " " + slotLabel});
            if (sselector->slotActive[sselector->selSlot])
                line1.push_back({"|@T|", _("Delete slot")});
            line1.push_back({"|@Left|/|@Right|", _("Slot")});
            line1.push_back({"|@O|", _("Back")});
        } else {
            line1.push_back({"|@X|", _("Save to slot") + " " + slotLabel});
            line1.push_back({"|@Left|/|@Right|", _("Slot")});
            line1.push_back({"|@O|", _("Don't save")});
        }
        line2.push_back({"|@L2+R2|", _("System")});
        return;
    }
    // Games
    if (carousel.games.empty()) {
        line1.push_back({"|@Up|", _("Quick menu")});
        line2.push_back({"|@Select|", _("Games shown")});
        line2.push_back({"|@L2+R2|", _("System")});
        return;
    }
    const PsGame *game = carousel.selectedIsValid() ? carousel.games[carousel.selected].get() : nullptr;
    line1.push_back({"|@X|", game != nullptr && game->app ? _("Start") : _("Play")});
    if (game != nullptr && !game->foreign && Env::retroArchInstalled())
        line1.push_back({"|@S|", _("Play in RetroArch")});
    line1.push_back({"|@Down|", _("Game menu")});
    line1.push_back({"|@Up|", _("Quick menu")});
    line2.push_back({"|@Select|", _("Games shown")});
    line2.push_back({"|@Start|", _("Random")});
    line2.push_back({"|@T|", _("Guide")});
    line2.push_back({"|@L2+R2|", _("System")});
}

//*******************************
// GuiLauncher::updateHintsIfNeeded
//*******************************
void GuiLauncher::updateHintsIfNeeded() {
    string sig = hintSignature();
    if (sig == lastHintSignature)
        return;
    lastHintSignature = sig;
    layoutHints();
}

//*******************************
// GuiLauncher::layoutHints
//*******************************
// Lays the two hint lines buildHintLines() returns out in the theme's hintBar (the pill most themes paint at
// the bottom right), each at the largest font from 22 down to 14 that fits its own half of the bar in the
// current language; below that the gaps close up, and line 2 (never line 1, which is always short) drops
// hints from the right if it is still too wide even at the smallest font and tightest gap. A hintBar under
// 48 px tall (an old theme that never expected two lines) shows line 1 only, at the bar's full height.
void GuiLauncher::layoutHints() {
    const LauncherTheme &theme = app.theme().launcher();
    ableem::Rect bar(560, 624, 680, 72);
    if (theme.hintBar.set)
        bar = ableem::Rect(theme.hintBar.x, theme.hintBar.y, theme.hintBar.w, theme.hintBar.h);

    buildHintLines(hints, hints2);
    hintsOneLineOnly = bar.h < 48;
    if (hintsOneLineOnly)
        hints2.clear();

    PanelStyle style = gui->panelStyle();
    const int inset = 16;
    const int iconGap = 6; // icon(s) to the label
    static const int sizes[] = {22, 20, 18, 16, 14};

    // fits `items` into `rect`'s width by shrinking the font, then the gaps, then - only when allowDrop -
    // dropping hints from the right; positions each one's chip and label inside `rect`
    auto layoutLine = [&](std::vector<Hint> &items, const ableem::Rect &rect, bool allowDrop, ableem::Font &outFont,
                          int &outLabelY, int &outChipY) {
        auto iconWidth = [&](const Hint &h) { return style.buttonsWidth(*gui, h.markers) - 6; }; // buttons() adds a gap
        int gap = 28;
        int total = 0;
        for (int size : sizes) {
            outFont =
                size == 22 ? gui->assets().themeFonts[FONT_22_MED] : gui->assets().themeFonts.atSize(FONT_MED, size);
            total = items.empty() ? 0 : -gap;
            for (const Hint &h : items)
                total += iconWidth(h) + iconGap + gui->text().textWidth(outFont, h.label) + gap;
            if (total <= rect.w - 2 * inset)
                break;
        }
        while (total > rect.w - 2 * inset && gap > 10) { // the smallest font still too wide: closer together
            total -= static_cast<int>(items.size()) * 4;
            gap -= 2;
        }
        while (allowDrop && total > rect.w - 2 * inset && items.size() > 1) {
            const Hint dropped = items.back();
            total -= iconWidth(dropped) + iconGap + gui->text().textWidth(outFont, dropped.label) + gap;
            items.pop_back();
        }
        int x = rect.x + max(inset, (rect.w - total) / 2);
        outLabelY = rect.y + (rect.h - outFont.lineHeight()) / 2;
        outChipY = rect.y + (rect.h - 30) / 2;
        for (Hint &h : items) {
            const int iconW = iconWidth(h);
            h.chipX = x;
            h.labelX = x + iconW + iconGap;
            x = h.labelX + gui->text().textWidth(outFont, h.label) + gap;
        }
    };

    if (hintsOneLineOnly) {
        layoutLine(hints, bar, false, hintFont, hintLabelY, hintChipY);
    } else {
        const ableem::Rect top(bar.x, bar.y, bar.w, bar.h / 2);
        const ableem::Rect bottom(bar.x, bar.y + bar.h / 2, bar.w, bar.h - bar.h / 2);
        layoutLine(hints, top, false, hintFont, hintLabelY, hintChipY);
        layoutLine(hints2, bottom, true, hintFont2, hintLabelY2, hintChipY2);
    }
}

//*******************************
// GuiLauncher::render
//*******************************
// render method called every loop
void GuiLauncher::render() {
    gui->endBusy(); // the reload after a game, or after Options, is over once the launcher draws
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

    // the footer's two hint lines, built from the state and the selection - see buildHintLines(). Rebuilt
    // (and re-laid-out) only when updateHintsIfNeeded() finds they actually changed.
    updateHintsIfNeeded();
    PanelStyle style = gui->panelStyle();
    for (const Hint &hint : hints) {
        style.buttons(*gui, hint.markers, hint.chipX, hintChipY);
        gui->text().renderText_WithColor(hintFont, hint.label, hint.labelX, hintLabelY, hintColor);
    }
    if (!hintsOneLineOnly)
        for (const Hint &hint : hints2) {
            style.buttons(*gui, hint.markers, hint.chipX, hintChipY2);
            gui->text().renderText_WithColor(hintFont2, hint.label, hint.labelX, hintLabelY2, hintColor);
        }

    // the top-right corner: the scan's bubble, the notification lines stacked under it
    scanBubble.render(*gui, time);
    int belowScan = scanBubble.visible() ? scanBubble.top + scanBubble.height() + 8 : scanBubble.top;
    extensionBubble.top = belowScan;
    extensionBubble.render(*gui, time);
    notificationLines.render(
        *gui, time, extensionBubble.visible() ? extensionBubble.top + extensionBubble.height() + 8 : belowScan);

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
// GuiLauncher::settleEmptyRoster
//*******************************
// With no game in the set there is nothing to start: the menu row opens on the settings icon (the only
// one showOptions enables then), which hides the play button, and Up (and Circle) keep it open - see
// loop_joyMoveUp / loop_circleButton_Pressed. Called wherever the roster may have changed.
void GuiLauncher::settleEmptyRoster() {
    if (!carousel.games.empty() || state != LauncherScreenState::Games)
        return;
    if (menu == nullptr || playButton == nullptr)
        return; // loadAssets switches the set once before the elements exist; it calls again at its end
    const int now = gui->platform().ticks();
    menu->transition = TR_MENUON;
    switchState(LauncherScreenState::Set, now);
    motionStart = 0;
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
    if (!enabled[3])
        menu->resume = ableem::Texture(); // no resume icon, no picture of another game's resume point
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
    menu->direction = 0;
    menu->duration = 100;
    // at rest for the state the launcher is in (a row rebuilt while it was closing - Select from the open
    // row switching to a set of another kind - used to stay half-closed with its icon half-zoomed)
    menu->settle(state == LauncherScreenState::Set, state == LauncherScreenState::Set ? 440 : 520);
    menuHead->setText(headers[0], fgColor);
    menuText->setText(texts[0], fgColor);
}