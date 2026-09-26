//
// GuiLauncher, the actions: what Cross does in each state and for each icon of the game menu - starting the
// game, opening the settings / game editor / memory-card manager / resume-point selector - and the L2+Select
// choosers for the games sub-directory and the RetroArch playlist. Each shows a sub-screen and reconciles
// the carousel afterwards. The event loop that dispatches to these is launcher_input.cpp.
//
#include "evoui_launcher.h"
#include "evoui_set_picker.h"
#include "gui/gui.h"
#include "../../gui/menus/gui_options_menu.h"
#include "gui/screens/gui_confirm.h"
#include "gui/screens/gui_about.h"
#include "gui/screens/gui_hardware_info.h"
#include "../../gui/menus/gui_game_editor_menu.h"
#include "gui/menus/gui_game_editor_ra_menu.h"
#include "../../gui/menus/gui_playlists_menu.h"
#include "../../gui/menus/gui_game_dir_menu.h"
#include "../../gui/menus/gui_memcards_menu.h"
#include "../../gui/menus/gui_game_manager_menu.h"
#include "core/services/environment.h"
#include "core/services/system.h"
#include "core/services/launch.h"
#include "evoui_mc_manager.h"
#include "evoui_app_start.h"
#include "evoui_system_menu.h"
#include "evoui_extensions.h"
#include "evoui_processors.h"
#ifdef AB_ONLINE_UPDATE
#include "evoui_update.h"
#include <ctime>
#endif

#include <algorithm>
#include <iostream>
#include <ableem/engine/log.h>

using namespace std;

namespace {
// the console's hardware tool, an extension shipped with the console package: Extensions/pscbios/
const char *const PscBiosExtension = "pscbios";
} // namespace

//*******************************
// GuiLauncher::loop_chooseSet
//*******************************
// Select: the one screen for what the carousel shows - the PlayStation / RetroArch / Apps tabs and the
// groups in each (was Select cycling the sets and L2+Select opening a folder or playlist picker)
void GuiLauncher::loop_chooseSet() {
    powerOffShift = false;
    GuiSetPicker picker(*gui);
    picker.selection = selection;
    picker.raPlaylists = raPlaylists;
    renderer.captureNextFrame();
    render();
    picker.background = renderer.lastCapture();
    picker.show();
    forgetHeldModifiers(); // reached with L2 held, maybe; its release went to the picker
    if (picker.cancelled)
        return;

    selection = picker.selection;
    switchSet(selection.set, false); // the icon row follows the set: an empty one leaves settings alone
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
    if (carousel.games.empty() || refuseLicenceProtected()) {
        return;
    }

    app.session().startingGame = true;
    if (carousel.selectedIsValid()) {
        app.session().runningGame = carousel.games[carousel.selected];
    }
    app.session().resumePoint = -1;
    rememberSelection();
    menuVisible = false;

    if (selectedIsPs1())
        app.gameCatalog().recordGamePlayed(app.session().runningGame);

    app.session().emuMode = EmuMode::Pcsx;

    // if it's a PS1 game see if the user wants to play it in RetroArch instead
    if (selectedIsPs1()) {
        if (selection.set == GameSet::Lightgun)
            return loop_squareButton_Pressed(); // a light-gun game: RetroArch's core has the guncon
        if (app.session().runningGame->internal) {
            if (app.session().runningGame->play_using_ra)
                return loop_squareButton_Pressed(); // play internal PSX game in RA
        } else {
            IniFile gameini;
            gameini.load(carousel.games[carousel.selected]->folder + sep + GAME_INI);
            if (gameini.values["play_using_ra"] == "true")
                return loop_squareButton_Pressed(); // play PSX game in RA
        }
        if (app.config().inifile.values["play_all_psx_with_ra"] == "true")
            return loop_squareButton_Pressed(); // play PSX game in RA
    }

    if (app.session().runningGame->foreign) {
        if (!app.session().runningGame->app) {
            app.session().emuMode = EmuMode::RetroArch;
        } else {
            GuiAppStart appStartScreen(*gui);
            appStartScreen.setGame(app.session().runningGame);
            appStartScreen.show();
            bool result = appStartScreen.result;
            // Do not run
            if (!result) {
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
        // the theme, fonts and every cover reload: the spinner over the options panel meanwhile
        gui->beginBusy(_("Applying settings..."), [&option]() { option.render(); });
        app.applyOnlineSetting(); // "Fetch box art online" may have changed
#ifdef AB_ONLINE_UPDATE
        app.applyUpdateSetting(); // "Updates" (the channel) may have changed
#endif
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

        switchSet(selection.set, false);
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

        // the state is loadAssets()'s: Games, or Set with the row open when the set is empty
        gui->endBusy();
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

    // a RetroArch game has its own, one-row editor
    if (carousel.selectedIsValid() && carousel.games[carousel.selected]->foreign) {
        if (carousel.games[carousel.selected]->app)
            return;
        GuiEditorRA raEditor(*gui);
        raEditor.gameData = carousel.games[carousel.selected];
        raEditor.show();
        if (raEditor.changed && selection.set == GameSet::Lightgun)
            reloadLightgunSetAfterEdit();
        return;
    }

    GuiEditor editor(*gui);
    if (carousel.selectedIsValid()) {
        editor.gameData = carousel.games[carousel.selected];
    }

    editor.show();
    if (selection.set == GameSet::Lightgun) {
        reloadLightgunSetAfterEdit();
        return;
    }

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
                reloadFavoritesAfterRemoval();
            }
        } else {
            if (editor.changes) {
                app.library().updateTitle(*carousel.games[carousel.selected], editor.lastName);
            }
            app.library().reload(*carousel.games[carousel.selected]);
            if (selection.set == GameSet::PS1 && selection.ps1SelectState == Ps1SelectState::Favorites &&
                editor.gameData->favorite == false) {
                reloadFavoritesAfterRemoval();
            }
        }
    }

    // if the current set is favorites and the user removes the last favorite carousel.selected will be -1
    if (carousel.selected != -1 && carousel.selectedIsValid()) {
        carousel.setInitialPositions(carousel.selected);
        updateMeta();
        menu->setResumePic(app.resumePoints().lastPicture(*carousel.games[carousel.selected]));
        carousel.snapMainCover(false); // the menu is still open: the cover goes straight back above it
    }
}

//*******************************
// GuiLauncher::reloadLightgunSetAfterEdit
//*******************************
// the Lightgun set is showing and an editor may have taken the selected game out of it: reload, and
// when nothing is left, show every PS1 game instead of an empty carousel
void GuiLauncher::reloadLightgunSetAfterEdit() {
    reloadGames();
    if (carousel.games.empty()) {
        app.session().launcher.set = GameSet::PS1;
        app.session().launcher.ps1SelectState = Ps1SelectState::AllGames;
        loadAssets();
    }
}

//*******************************
// GuiLauncher::reloadFavoritesAfterRemoval
//*******************************
// The Favorites set is showing and the editor just took the selected game out of it: reload with one
// game fewer - and when it was the last one, show every PS1 game instead of an empty carousel the user
// could not leave (AutoBleem-NG's 3cc3ff87).
void GuiLauncher::reloadFavoritesAfterRemoval() {
    app.session().launcher.set = GameSet::PS1;
    app.session().launcher.ps1SelectState = Ps1SelectState::Favorites;
    loadAssets();
    if (carousel.games.empty()) {
        app.session().launcher.ps1SelectState = Ps1SelectState::AllGames;
        loadAssets();
    }
}

//*******************************
// GuiLauncher::loop_crossButtonPressed_STATE_SET__OPT_EDIT_MEMCARD
//*******************************
void GuiLauncher::loop_crossButtonPressed_STATE_SET__OPT_EDIT_MEMCARD() {
    if (carousel.games.empty()) {
        return;
    }
    if (carousel.selectedIsValid() && carousel.games[carousel.selected]->foreign) {
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
    mcManager.backgroundImg = background->tex;
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
            if (refuseLicenceProtected()) {
                return;
            }
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
            // app.audio().cursor.play();
            app.resumePoints().saveAfterLaunch(*carousel.games[carousel.selected], sselector->selSlot);
            app.resumePoints().storePictureForSlot(*carousel.games[carousel.selected], sselector->selSlot);
            sselector->visible = false;
            arrow->visible = true;
            app.audio().resume.play();
            notificationLines[1].setText(_("Resume point saved to slot") + " " + to_string(sselector->selSlot + 1),
                                         DefaultShowingTimeout);

            menu->setResumePic(
                app.resumePoints().pictureForSlot(*carousel.games[carousel.selected], sselector->selSlot));

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
// the system menu: everything the classic main menu used to offer, now reached with L2+R2 (the background-scan
// work's step 4, see CLAUDE.md; moved off a bare R2 so reaching for it can no longer be mistaken for the L2+R2
// power-off it replaced - Power Off is one of its own items now). GuiSystemMenu only picks; every action below is what
// ClassicMenuScreen used to do for the same item.
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
#ifdef AB_ONLINE_UPDATE
        systemMenu.updateAvailable = app.updates().status().info.any();
#endif
        if (background != nullptr)
            systemMenu.background = background->tex;
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
        if (!Env::retroArchInstalled()) {
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
        // the console's PSC-Bios - the facts, and the WiFi, the time zone and the pads set up - an extension
        // shipped with the console package (Extensions/pscbios/, 2026-09-24; it was an App before). Wherever it
        // cannot run - a Pi or a PC (it is built for the console only), a console without it, one built for
        // another AutoBleem or disabled after a crash - the built-in screen, rather than nothing
        ExtensionRuntime::Refusal why = ExtensionRuntime::Refusal::NotFound;
#ifdef AB_ONLINE_UPDATE
        app.extensionCatalog().scan();
        if (app.extensionCatalog().find(PscBiosExtension) != nullptr) {
            why = app.extensions().run(PscBiosExtension, System::hasDefaultRoute());
            forgetHeldModifiers(); // its screens ran their own loops
            gui->input().flushEvents();
            applyExtensionRequests();
            if (why != ExtensionRuntime::Refusal::None) {
                PLOG_WARNING << "PSC-Bios did not run (" << static_cast<int>(why) << ")";
            }
        }
#endif
        if (why == ExtensionRuntime::Refusal::Failed) {
            notificationLines[1].setText("PSC-Bios " + _("closed with an error"), 2 * DefaultShowingTimeout);
        } else if (why != ExtensionRuntime::Refusal::None) {
            GuiHardwareInfo infoScreen(*gui);
            infoScreen.show();
        }
        break;
    }

    case SystemMenuAction::Options:
        // same screen, same reload, as the settings icon in the Set overlay
        loop_crossButtonPressed_STATE_SET__OPT_AB_SETTINGS();
        break;

    case SystemMenuAction::SoftwareUpdate:
#ifdef AB_ONLINE_UPDATE
        loop_softwareUpdate();
#endif
        break;

    case SystemMenuAction::Extensions:
        loop_openExtensions();
        break;

    case SystemMenuAction::Processors:
        loop_openProcessors();
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
            app.requestPowerOff(); // the console's standby (AutoBleem::run() leaves), a halt elsewhere
        }
        break;
    }
    }
}

//*******************************
// GuiLauncher::loop_openProcessors
//*******************************
// the sequences are what a running scan reads, so they are not edited under it; a change asks for a scan,
// which is where it takes effect (and "Run again" is done)
void GuiLauncher::loop_openProcessors() {
    if (app.scans().scanning()) {
        notificationLines[1].setText(_("Wait for the scan to finish"), DefaultShowingTimeout);
        return;
    }
    bool changed = false;
    {
        GuiProcessors screen(*gui);
        if (background != nullptr)
            screen.background = background->tex;
        screen.show();
        changed = screen.changed();
    }
    forgetHeldModifiers();
    if (changed)
        app.scans().requestScan();
}

#ifdef AB_ONLINE_UPDATE
//*******************************
// GuiLauncher::loop_openExtensions
//*******************************
// the Extensions list (every folder in Extensions/, read again each time it opens), then the chosen one's
// run(): its own screens on our Gui, back here when it is done. A refusal is shown on the row already; what
// comes back from run() is reported on the notification line.
void GuiLauncher::loop_openExtensions() {
    app.extensionCatalog().scan();
    const bool networkUp = System::hasDefaultRoute();
    string chosen;
    {
        GuiExtensions list(*gui, app.extensionCatalog(), networkUp);
        if (background != nullptr)
            list.background = background->tex;
        list.show();
        chosen = list.chosen;
    }
    forgetHeldModifiers();
    if (chosen.empty())
        return;
    const ExtensionInfo *info = app.extensionCatalog().find(chosen);
    const string title = info != nullptr ? info->title : chosen;
    ExtensionRuntime::Refusal why = app.extensions().run(chosen, System::hasDefaultRoute());
    forgetHeldModifiers(); // its screens ran their own loops
    gui->input().flushEvents();
    if (why == ExtensionRuntime::Refusal::Failed || why == ExtensionRuntime::Refusal::LoadFailed ||
        why == ExtensionRuntime::Refusal::WrongAbi) {
        notificationLines[1].setText(title + " " + _("closed with an error"), 2 * DefaultShowingTimeout);
    } else if (why == ExtensionRuntime::Refusal::Offline) {
        notificationLines[1].setText(_("Needs a network connection"), DefaultShowingTimeout);
    }
    applyExtensionRequests();
}

//*******************************
// GuiLauncher::applyExtensionRequests
//*******************************
// once a frame (and after an extension's run()): the bubble an extension fed, a message for the line, and
// the reloads it asked for - done here, in the launcher's own frame, never from inside an extension's call
void GuiLauncher::applyExtensionRequests() {
    App::ExtensionRequests r = app.takeExtensionRequests();
    if (r.bubbleChanged) {
        if (r.bubbleVisible) {
            // the percentage next to the detail says what the bar shows - in 64 bits, as the extension
            // reports its bytes (a Store download is past a 32-bit count's reach on the console)
            string detail = r.bubbleDetail;
            if (r.bubbleTotal > 0)
                detail += "  " + to_string(min(r.bubbleDone, r.bubbleTotal) * 100 / r.bubbleTotal) + "%";
            extensionBubble.show(r.bubbleTitle, detail, static_cast<int64_t>(r.bubbleDone),
                                 static_cast<int64_t>(r.bubbleTotal), 0);
        } else {
            extensionBubble.hide();
        }
    }
    if (!r.message.empty())
        notificationLines[1].setText(r.message, 2 * DefaultShowingTimeout);
    if (r.reloadConfig) {
        // what closing Options does: config.ini read again, the theme and every cover reloaded
        Config fresh;
        app.config().inifile.values = fresh.inifile.values;
        app.applyOnlineSetting();
#ifdef AB_ONLINE_UPDATE
        app.applyUpdateSetting();
#endif
        freeAssets();
        loadAssets();
        switchSet(selection.set, false);
        showSetName();
    } else if (r.reloadApps && selection.set == GameSet::Apps && !carousel.scrolling) {
        reloadGames();
    }
}

//*******************************
// GuiLauncher::pollUpdates
//*******************************
// The check AutoBleem::run() started lands here: when it found something the user has not skipped or
// postponed, the question is asked right away, over the carousel. Also starts the daily check for a
// launcher that stays up for days.
void GuiLauncher::pollUpdates() {
    UpdateService &updates = app.updates();
    const time_t now = ::time(nullptr); // GuiLauncher::time is the frame clock
    if (updates.checkDue(now))
        updates.startCheck(now);
    const UpdateService::Status status = updates.poll();
    if (status.checkedThisPoll && status.phase == UpdateService::Phase::Checked && updates.shouldPrompt(now))
        offerUpdate(false);
}

//*******************************
// GuiLauncher::loop_softwareUpdate
//*******************************
// The system menu's item: a check now, on the screen, then the question - or "up to date".
void GuiLauncher::loop_softwareUpdate() {
    UpdateService &updates = app.updates();
    if (!updates.enabled()) {
        GuiConfirm confirm(*gui);
        confirm.label = _("Updates are off - turn them on in Options");
        confirm.show();
        return;
    }
    if (updates.status().phase == UpdateService::Phase::Downloading)
        return;
    if (updates.config().networkUp && !updates.config().networkUp()) {
        // the console without WiFi (or without the AutoBleem kernel, which brings it): nothing to ask
        GuiConfirm confirm(*gui);
        confirm.label = _("Not connected");
        confirm.show();
        return;
    }
    updates.startCheck(::time(nullptr));
    {
        GuiUpdateProgress progress(*gui);
        if (background != nullptr)
            progress.background = background->tex;
        progress.show();
    }
    if (updates.status().phase == UpdateService::Phase::Checked && updates.status().info.any())
        offerUpdate(true);
}

//*******************************
// GuiLauncher::offerUpdate
//*******************************
// The question, and what follows a yes: the download with its bar, then - on a Pi - out to the session
// loop with MENU_OPTION_UPDATE, which runs autobleem-update over System/Updates, and on the console out to
// rc/selection.sh, which runs abupdate over it; a dev host stops at the downloaded files, there is no
// installer to run on it.
void GuiLauncher::offerUpdate(bool fromMenu) {
    UpdateService &updates = app.updates();
    const time_t now = ::time(nullptr); // GuiLauncher::time is the frame clock
    UpdateChoice choice;
    {
        GuiUpdatePrompt prompt(*gui);
        prompt.info = updates.status().info;
        if (background != nullptr)
            prompt.background = background->tex;
        prompt.show();
        choice = prompt.result;
    }
    if (!fromMenu)
        forgetHeldModifiers();
    switch (choice) {
    case UpdateChoice::Later:
        updates.postpone(now);
        return;
    case UpdateChoice::Skip:
        updates.skip(now);
        return;
    case UpdateChoice::Now:
        break;
    }
    updates.startDownload();
    UpdateService::Status outcome;
    {
        GuiUpdateProgress progress(*gui);
        if (background != nullptr)
            progress.background = background->tex;
        progress.show();
        outcome = progress.finalStatus;
    }
    if (outcome.phase != UpdateService::Phase::Downloaded)
        return;
#if defined(AB_APPLIANCE) || defined(AB_PLATFORM_PSC)
    // the session loop takes it from here (payload_linux/system/autobleem-session.sh; the console's
    // rc/boot.sh -> selection.sh -> abupdate)
    app.session().menuOption = MENU_OPTION_UPDATE;
    menuVisible = false;
#elif defined(AB_PLATFORM_WIN)
    // the downloaded installer, shown (not /S - the owner asked: a silent background update looked like
    // nothing happened) and with the launcher restarted after. /RESTART alone: the setup wizard's progress
    // is on screen while it works, and it waits for this process to leave (the launcher's mutex, main.cpp)
    // before it touches the program folder
    {
        const string setup = Env::getPathToSystemDir() + sep + "Updates" + sep + outcome.info.autobleem.name;
        if (System::startDetached(setup, {"/RESTART"})) {
            gui->drawText(_("The update is downloaded - the installer is opening..."));
            app.session().menuOption = MENU_OPTION_UPDATE;
            menuVisible = false;
        } else {
            GuiConfirm confirm(*gui);
            confirm.label = _("The installer could not be started - it is in System/Updates");
            confirm.show();
        }
    }
#else
    GuiConfirm confirm(*gui);
    confirm.label = _("Downloaded into System/Updates - an appliance would run the installer now");
    confirm.show();
#endif
}
#endif
