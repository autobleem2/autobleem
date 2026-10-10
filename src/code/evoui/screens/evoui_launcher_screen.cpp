//
// GuiLauncher, the screen half: assets, the sets and the metadata panel, the settings overlay's state
// transitions, and render(). Input is launcher_input.cpp, the sub-screen actions launcher_actions.cpp.
//

#include "evoui_launcher.h"
#include "../channel_watermark.h"
#include "../controls/hint_slots.h"
#include "../set_banner.h"
#include "ra_gates.h"
#include "core/version.h"
#include "core/services/theme.h"
#include "../evoui_plural.h"
#include "gui/gui.h"
#include "../../gui/menus/gui_options_menu.h"
#include "gui/screens/gui_confirm.h"
#include <algorithm>
#include <iostream>
#include "evoui_mc_manager.h"
#include "evoui_set_picker.h"
#include "gui/panel_style.h"
#include <ab_gui/hint_bar.h>
#include <ab_gui/screen_stack.h>
#include <ab_gui/transitions.h>
#include <cassert>
#include <memory>
#include <vector>
#include <ableem/engine/ext_trace.h>
#include <ableem/engine/log.h>
#include <ableem/engine/startup_timer.h>

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
// the carousel has come to rest: the selected game's snap and resume picture are asked of the background
// loader, and pollSettleLoads() shows them when they are decoded - no PNG decode on this thread
void GuiLauncher::finishSettleLoads() {
    settleLoadsPending = false;
    pendingSnapPath.clear();
    pendingResumePath.clear();
    vector<string> paths;

    const ableem::ThemeRect &panel = app.theme().launcher().snapPanel;
    if (!panel.set || !carousel.selectedIsValid()) {
        snapTex = ableem::Texture();
        snapForGameId = -1;
    } else {
        const PsGame &game = *carousel.games[carousel.selected];
        if (!(snapForGameId == game.gameId && snapForInternal == game.internal && snapTex.valid())) {
            snapForGameId = game.gameId;
            snapForInternal = game.internal;
            pendingSnapPath = snapPathFor(game);
            if (pendingSnapPath.empty())
                snapTex = ableem::Texture();
            else
                paths.push_back(pendingSnapPath);
        }
    }

    if (carousel.selectedIsValid()) {
        pendingResumePath = app.resumePoints().lastPicture(*carousel.games[carousel.selected]);
        if (pendingResumePath.empty())
            menu->setResumeTex(ableem::Texture());
        else if (pendingResumePath != pendingSnapPath)
            paths.push_back(pendingResumePath);
    }
    extrasLoader.want(paths);
}

//*******************************
// GuiLauncher::pollSettleLoads
//*******************************
void GuiLauncher::pollSettleLoads() {
    ableem::Image image;
    if (!pendingSnapPath.empty() && extrasLoader.take(pendingSnapPath, image)) {
        snapTex = ableem::Texture::fromImage(renderer, image);
        if (pendingResumePath == pendingSnapPath) {
            menu->setResumeTex(snapTex);
            pendingResumePath.clear();
        }
        pendingSnapPath.clear();
    }
    if (!pendingResumePath.empty() && extrasLoader.take(pendingResumePath, image)) {
        menu->setResumeTex(ableem::Texture::fromImage(renderer, image));
        pendingResumePath.clear();
    }
}

//*******************************
// GuiLauncher::hideSettlePictures
//*******************************
void GuiLauncher::hideSettlePictures() {
    menu->setResumeTex(ableem::Texture());
    snapTex = ableem::Texture();
    snapForGameId = -1;
    pendingSnapPath.clear();
    pendingResumePath.clear();
}

//*******************************
// GuiLauncher::loadSnap
//*******************************
// the selected game's screenshot, now. Only when the theme draws it.
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
    pendingSnapPath.clear(); // this one wins over a snap still being decoded
    snapTex = ableem::Texture::loadFile(renderer, snapPathFor(game));
}

//*******************************
// GuiLauncher::snapPathFor
//*******************************
// the path the scan cached while its file exists, else a look in the thumbnails tree (a RetroArch game, an
// internal game); an App has none
string GuiLauncher::snapPathFor(const PsGame &game) {
    if (game.app)
        return "";
    string path = game.snapPath;
    if (path.empty() || !DirEntry::exists(path)) {
        if (game.foreign)
            path = app.thumbnails().findSnap(game.db_name, game.title, game.image_path);
        else
            path = app.thumbnails().findSnap(ableem::ThumbnailLookup::PlayStationDbName, game.title,
                                             game.folder + sep + game.base, game.recordName);
    }
    return path;
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
    // every way into a set passes here: one that needs RetroArch is never shown without it (ra_gates.h)
    selection.set = setOrFallback(selection.set, Env::retroArchInstalled());
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
// GuiLauncher::infoTimeout / showInfo
//*******************************
// Options -> Interface -> "Notification timeout" (config.ini showingtimeout, seconds): how long the informational
// bubbles stay; 0 = they are not shown. An error keeps its fixed time (DefaultShowingTimeout) and never asks.
long GuiLauncher::infoTimeout() const {
    return Strings::toInt(app.config().inifile.values["showingtimeout"], 2) * TicksPerSecond;
}

void GuiLauncher::showInfo(const string &text) {
    const long timeout = infoTimeout();
    if (timeout > 0)
        notificationLines[1].setText(text, timeout);
}

//*******************************
// GuiLauncher::showSetName
//*******************************
void GuiLauncher::showSetName() {
    vector<string> setNames = {"Showing: PS1 games", // this is a dummy entry. setPS1SubStateNames is used.
                               _("Showing: RetroArch") + " ", _("Showing: Lightgun games") + " ",
                               _("Showing: Apps") + " "};
    vector<string> setPS1SubStateNames = {_("Showing: All games") + " ", _("Showing: Internal games") + " ",
                                          _("Showing: Favorite games") + " ", _("Showing: Game history") + " ",
                                          _("Showing: USB games directory:") + " "};
    assert(setPS1SubStateNames.size() == static_cast<size_t>(Ps1SelectState::GamesSubdir) + 1);
    assert(setNames.size() == static_cast<size_t>(GameSetLast) + 1);

    string numGames = " (" + pluralGames(carousel.games.size()) + ")";

    // the set banner always shows: with Options' "Notification timeout" Off it holds for the default time
    // (Off hides the other informational bubbles only)
    const long timeout = SetBanner::holdTicks(infoTimeout(), DefaultShowingTimeout);

    if (selection.set == GameSet::PS1) {
        string name = setPS1SubStateNames[static_cast<int>(selection.ps1SelectState)];
        // every entry above carries its own trailing space (needed when a directory name follows); drop
        // it here so it doesn't double up with numGames' own leading space (was "All Games  (21 games)")
        if (!name.empty() && name.back() == ' ')
            name.pop_back();
        if (selection.ps1SelectState == Ps1SelectState::GamesSubdir) {
            name += " " + selection.usbGameDirName;
        }
        notificationLines[0].setText(name + numGames, timeout);
    } else if (selection.set == GameSet::RetroArch) {
        string playlist = DirEntry::getFileNameWithoutExtension(selection.raPlaylistName);
        // numGames already starts with a space - no extra " " here (was a double space before the count)
        notificationLines[0].setText(setNames[static_cast<int>(selection.set)] + playlist + numGames, timeout);
    } else if (selection.set == GameSet::Apps) {
        // Apps are counted as apps, not games ("Showing: Apps: Tools (3 apps)")
        string name = _("Showing: Apps");
        if (selection.appCategory != AppCategory::All)
            name += ": " + appCategoryLabel(selection.appCategory);
        string numApps = " (" +
                         (selection.appCategory == AppCategory::Packages ? pluralPackages(carousel.games.size())
                                                                         : pluralApps(carousel.games.size())) +
                         ")";
        notificationLines[0].setText(name + numApps, timeout);
    }
}

//*******************************
// GuiLauncher::selectedGameKey / findGame
//*******************************
// a library game is the same game by id; a playlist game's id is only its position in the playlist, which a
// rewrite may have moved - its image path is what names it
GuiLauncher::GameKey GuiLauncher::selectedGameKey() const {
    GameKey key;
    if (carousel.selectedIsValid()) {
        const PsGame &current = *carousel.games[carousel.selected];
        key.gameId = current.gameId;
        key.internal = current.internal;
        key.foreign = current.foreign;
        key.imagePath = current.image_path;
    }
    return key;
}

int GuiLauncher::findGame(const GameKey &key) const {
    if (key.gameId == -1)
        return -1;
    for (int i = 0; i < static_cast<int>(carousel.games.size()); i++) {
        const PsGame &game = *carousel.games[i];
        bool same = key.foreign ? (game.foreign && game.image_path == key.imagePath)
                                : (!game.foreign && game.gameId == key.gameId && game.internal == key.internal);
        if (same)
            return i;
    }
    return -1;
}

//*******************************
// GuiLauncher::reloadGames
//*******************************
// re-runs the current set's query and re-selects the highlighted game by id. When that game is gone (its
// folder removed while the scanner watched, or merged into another) the first game of the set - or none -
// is highlighted instead, and a resume-point picker that was showing its slots is closed.
void GuiLauncher::reloadGames() {
    forgetSetCounts(); // the roster changed: the picker counts again
    const GameKey keep = selectedGameKey();

    switchSet(selection.set, false);

    const int found = findGame(keep);
    const bool kept = found != -1;
    if (kept)
        carousel.selected = found;
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
// padBatteryPlayerLabel (local)
//*******************************
// literal _() calls at each branch, like psPlayerSlotLabel's other copies (evoui_launcher_input.cpp,
// gui_hardware_info.cpp) - tools/lang_tools.py's extract only recognises a literal inside _(...), not a
// runtime value, so every copy needs its own. "" for Unused: a battery matched to a third+ pad (past the
// PS1 port count) falls back to the generic label in padBatteryLabelsFor() below, same as no match at all.
static string padBatteryPlayerLabel(PsPlayerSlot slot) {
    switch (slot) {
    case PsPlayerSlot::Player1:
        return _("Player 1");
    case PsPlayerSlot::Player2:
        return _("Player 2");
    case PsPlayerSlot::Unused:
    default:
        return "";
    }
}

//*******************************
// padBatteryIconTag (local)
//*******************************
// the short form of the same label, for the icon row (evoui_launcher.h's padBatteryIconTags): "P1"/"P2",
// kept to two characters everywhere on purpose - the icon row has no room for a full word, and a plain
// number-with-letter reads as a player number in every one of the 17 languages we ship without needing a
// per-language abbreviation (a Polish "G1"/"G2" for "gracz" was considered and dropped: "P1"/"P2" already
// reads correctly to a Polish player from a PS1/PS2 pad-select screen, and one shared abbreviation is one
// less thing to keep in sync across every language file). "" for Unused, same as padBatteryPlayerLabel.
static string padBatteryIconTag(PsPlayerSlot slot) {
    switch (slot) {
    case PsPlayerSlot::Player1:
        return _("P1");
    case PsPlayerSlot::Player2:
        return _("P2");
    case PsPlayerSlot::Unused:
    default:
        return "";
    }
}

//*******************************
// GuiLauncher::padBatteryLabelsFor
//*******************************
// C12: matches each sysfs battery reading to the SDL pad it belongs to (matchPadBatteries(), by the pad's
// own serial against the sysfs address - core/model/pad_battery_match.h) and labels it "Player 1" /
// "Player 2" when that pad is one of the two the PS1 emulators actually use - the same label
// showPadAssignment()/GuiHardwareInfo already show for it - with "P1"/"P2" for the icon row in
// `iconTagsOut`. A reading with no match (no SDL pad reported that address as its serial - unplugged
// since, no serial at all on this SDL/pad combination - or it landed on a third+ pad) falls back to the
// old generic "Wireless pad N" (icon tag ""), numbered only among the *unmatched* entries so one matched
// and one unmatched pad does not jump straight to "Wireless pad 2".
vector<string> GuiLauncher::padBatteryLabelsFor(const vector<PadBatteryInfo> &batteries,
                                                vector<string> &iconTagsOut) const {
    vector<ableem::PadInfo> pads = gui->input().pads();
    vector<PadBatterySource> sources;
    for (size_t i = 0; i < pads.size(); i++)
        sources.push_back({static_cast<int>(i), pads[i].serial});
    vector<MatchedPadBattery> matches = matchPadBatteries(batteries, sources);

    // Options -> "Swap Player 1 / Player 2" (C11): the battery row's P1/P2 label has to follow the same
    // swap the carousel/notice show and LaunchService's AB_PAD_ORDER hands the emulator.
    bool padSwap = app.config().inifile.values["padswap"] == "true";
    vector<string> labels(batteries.size());
    iconTagsOut.assign(batteries.size(), "");
    vector<int> unmatchedPosition(batteries.size(), -1);
    int unmatchedSeen = 0;
    for (size_t i = 0; i < matches.size(); i++) {
        if (matches[i].padIndex >= 0) {
            PsPlayerSlot slot = psPlayerSlot(matches[i].padIndex, static_cast<int>(pads.size()), padSwap);
            string label = padBatteryPlayerLabel(slot);
            if (!label.empty()) {
                labels[i] = label;
                iconTagsOut[i] = padBatteryIconTag(slot);
                continue;
            }
        }
        unmatchedPosition[i] = unmatchedSeen++;
    }
    for (size_t i = 0; i < labels.size(); i++) {
        if (labels[i].empty())
            labels[i] =
                unmatchedSeen > 1 ? _("Wireless pad") + " " + to_string(unmatchedPosition[i] + 1) : _("Wireless pad");
    }
    return labels;
}

//*******************************
// GuiLauncher::pollPadBattery
//*******************************
// called once a frame (loop(), like applyScanUpdate) but only acts every PadBatteryPollInterval: a handful
// of sysfs reads is cheap, but nothing here changes fast enough to need it every frame.
void GuiLauncher::pollPadBattery() {
    if (time - lastPadBatteryPoll < PadBatteryPollInterval && lastPadBatteryPoll != 0)
        return;
    lastPadBatteryPoll = time;

    padBatteries = padBatteryService.list();
    padBatteryLabels = padBatteryLabelsFor(padBatteries, padBatteryIconTags);

    // PadBatteryAlert (core/model/pad_battery_alert.h) owns the rules: one warning per low pad, taken down when the
    // pad is charging, back over the reset level, or gone
    for (const PadBatteryAlert::Event &event : lowBatteryAlert.update(padBatteries)) {
        if (event.kind == PadBatteryAlert::Kind::Show) {
            const PadBatteryInfo &pad = padBatteries[event.index];
            const string text =
                padBatteryLabels[event.index] + ": " + _("battery low") + " (" + to_string(pad.percent) + "%)";
            lowBatteryText[event.address] = text;
            notificationLines[1].setText(text, 0);
        } else {
            auto shown = lowBatteryText.find(event.address);
            if (shown == lowBatteryText.end())
                continue;
            // line 1 is shared: take it down only while it still shows this pad's warning
            if (notificationLines[1].visible() && notificationLines[1].bubble.title() == shown->second)
                notificationLines[1].bubble.hide();
            lowBatteryText.erase(shown);
        }
    }
}

//*******************************
// GuiLauncher::renderPadBatteries
//*******************************
// a small icon (outline + a fill proportional to the charge, plus a nub) and the percent, one per known
// pad, stacked down from the top-left corner - the launcher's own theme colours, no new texture: the outline
// is secColor, the fill the theme's accent (`selection`, white when unset; hintColor under PadBatteryLowPercent,
// so a low pad reads as a warning).
// C12: a small plate (PanelStyle::sheet - the same dark sheet + secondary-colour edge every panel in the
// launcher uses, sized to just the icons instead of a whole screen) sits behind the row, so the icons read
// against any theme's background image instead of floating over whatever happens to be behind them there.
// G5l: a theme's `plate` frame (ab_gui) is that plate, and its `battery` icon the outline and nub - the charge is
// still drawn here, into the icon's inner rect (PadBatteryCharge::rect: a fixed 2 px inset, the art spec's
// x 2..24, y 2..11 of the 29 x 13 icon); a theme with neither draws as before. AB_FAKE_PAD_BATTERY (dev hosts,
// PadBatteryService::list) fakes the pads.
// A matched pad's icon also gets its short "P1"/"P2" tag (padBatteryIconTags, from padBatteryLabelsFor())
// drawn to its left - an unmatched one gets no tag, same spot left blank, as before C12.
// C15: the tag and the percent are drawn in FONT_15_BOLD (a fixed, always-loaded font), not `hintFont` -
// `hintFont` is sized dynamically between 14 and 22 by layoutHints() for whatever the footer's hint bar
// needs this frame (a footer-only concern, unrelated to this 13px icon), so reusing it here made the text's
// size - and with it its vertical offset from the fixed "y - 2" this function used - drift with the footer's
// current language/state, which is what threw the tag and the percent off the icon's centre. Both text
// draws now compute their y from the fixed font's own line height so their visual centre lands on the
// icon's, whatever that height turns out to be.
int GuiLauncher::renderPadBatteries() {
    if (padBatteries.empty())
        return 0;
    const int iconW = 26, iconH = 13, nubW = 3, nubH = 7;
    const int plateMargin = 14; // review: the icons sat tight on the plate's edge at 8px - more room now
    int x = 16, y = 16;         // the top-left corner, in 720p and 4:3 alike (the 4:3 logo is top right)
    const ableem::Font &battFont = ThemeAssets::fixedFonts()[FONT_15_BOLD];
    const int textY = (iconH - battFont.lineHeight()) / 2; // added to y: centres the text on the icon

    int knownCount = 0;
    int tagW = 0; // the widest icon tag actually shown right now, so unmatched pads cost no extra width
    for (size_t i = 0; i < padBatteries.size(); i++) {
        if (!padBatteries[i].known())
            continue;
        knownCount++;
        const string &tag = i < padBatteryIconTags.size() ? padBatteryIconTags[i] : string();
        if (!tag.empty())
            tagW = std::max(tagW, battFont.width(tag) + 6); // the tag plus a small gap before the icon
    }
    if (knownCount == 0)
        return 0;

    int rowWidth = tagW + iconW + nubW + 6 + 44; // [tag] + icon + nub + gap + room for "100%"
    int rowHeight = iconH + 10;
    ableem::Rect plate(x - plateMargin, y - plateMargin, rowWidth + 2 * plateMargin,
                       knownCount * rowHeight - 10 + 2 * plateMargin);
    // G5l: the theme's `plate` frame into the very same rect; no frame = the code sheet, call for call
    abgui::Context &ctx = gui->uiContext();
    if (!ctx.style().drawFrame(ctx, "plate", plate)) {
        PanelStyle plateStyle;
        plateStyle.secondary = secColor;
        plateStyle.sheet(renderer, plate);
    }
    // G5l: the theme's `battery` icon (outline and nub, at its own size) replaces the code-drawn outline and nub
    const ableem::Texture batteryIcon = ctx.icon("battery");
    // ... and its `batteryCharging` icon (the bolt alone on a transparent canvas of the same size) is drawn over the
    // outline and the charge while a pad is charging; none = a code-drawn bolt of the same shape
    const ableem::Texture batteryChargingIcon = ctx.icon("batteryCharging");

    int iconX = x + tagW;
    for (size_t i = 0; i < padBatteries.size(); i++) {
        const PadBatteryInfo &pad = padBatteries[i];
        if (!pad.known())
            continue;
        const string &tag = i < padBatteryIconTags.size() ? padBatteryIconTags[i] : string();
        if (!tag.empty())
            gui->text().renderText_WithColor(battFont, tag, x, y + textY, fgColor);
        int glyphW = iconW + nubW, glyphH = iconH;
        if (batteryIcon.valid()) {
            glyphW = batteryIcon.size().w;
            glyphH = batteryIcon.size().h;
            const ableem::Rect iconRect(iconX, y, glyphW, glyphH);
            renderer.copy(batteryIcon, nullptr, &iconRect);
        } else {
            renderer.setDrawColor(secColor);
            renderer.drawRect(ableem::Rect(iconX, y, iconW, iconH));
            renderer.fillRect(ableem::Rect(iconX + iconW, y + (iconH - nubH) / 2, nubW, nubH));
        }
        // the charge is code-drawn into the glyph's inner rect, a fixed inset from its corner (PadBatteryCharge)
        const PadBatteryCharge charge = PadBatteryCharge::rect(iconX, y, glyphW, glyphH, pad.percent);
        // above the low threshold the fill is the theme's accent (selection), white when the theme sets none
        const ableem::ThemeColor &accent = app.theme().launcher().colors.selection;
        const PadBatteryFill accentFill = PadBatteryFill::accentOrWhite(accent.set, accent.r, accent.g, accent.b);
        // a pad on a charger is not "low" however empty it is - it is filling up
        ableem::Color fillColor = pad.percent <= PadBatteryAlert::LowPercent && !PadBatteryAlert::onCharger(pad)
                                      ? hintColor
                                      : ableem::Color(accentFill.r, accentFill.g, accentFill.b, 255);
        renderer.setDrawColor(fillColor);
        renderer.fillRect(ableem::Rect(charge.x, charge.y, charge.w, charge.h));
        if (pad.charging()) {
            if (batteryChargingIcon.valid()) {
                // the theme's bolt overlay (transparent canvas of the battery icon's size), over the outline and charge
                const ableem::Rect boltRect(iconX, y, batteryChargingIcon.size().w, batteryChargingIcon.size().h);
                renderer.copy(batteryChargingIcon, nullptr, &boltRect);
            } else {
                // the code-drawn bolt: the same shape in the battery (accent) colour, a dark edge under it
                const std::vector<PadBatteryBolt::Strip> bolt = PadBatteryBolt::strips(iconX, y, glyphW, glyphH);
                const int grow = PadBatteryBolt::Outline;
                renderer.setDrawColor(ableem::Color(14, 22, 30, 255));
                for (const PadBatteryBolt::Strip &strip : bolt)
                    renderer.fillRect(
                        ableem::Rect(strip.x - grow, strip.y - grow, strip.w + 2 * grow, strip.h + 2 * grow));
                renderer.setDrawColor(ableem::Color(accentFill.r, accentFill.g, accentFill.b, 255));
                for (const PadBatteryBolt::Strip &strip : bolt)
                    renderer.fillRect(ableem::Rect(strip.x, strip.y, strip.w, strip.h));
            }
        }
        gui->text().renderText_WithColor(battFont, to_string(pad.percent) + "%", iconX + iconW + nubW + 6, y + textY,
                                         fgColor);
        y += rowHeight;
    }
    return plate.y + plate.h;
}

//*******************************
// GuiLauncher::renderChannelWatermark
//*******************************
// UIREV-40: the build's channel tag (DEV / NIGHTLY / ALPHA / BETA / RC; a release has none) in the top-left corner -
// the `chip` frame (the theme's; the code-drawn chip without one) around the channel in capitals, the short version
// beside it in the secondary colour - the whole at 80 %, so it reads as a mark, not a control. A release draws
// nothing. It sits at x 12, y 72, under a two-pad battery plate (which ends at y 66); `plateBottom`
// (renderPadBatteries' return) pushes it lower for a taller plate, so it never covers one. Not translated: the
// channel's names. The design's sizes: the word bold 13, the version medium 14 (Fonts::atSize opens each once).
void GuiLauncher::renderChannelWatermark(int plateBottom) {
    // the build's version never changes while it runs: read once (productVersion may read a VERSION file)
    static const ChannelWatermark::Tag tag =
        ChannelWatermark::tagFor(ChannelWatermark::channelFromName(AB_BUILD_CHANNEL_NAME), Env::productVersion(),
                                 std::string(Version::GIT_HASH));
    if (!tag.shown())
        return;
    const ableem::Font &wordFont = ThemeAssets::fixedFonts().boldAtSize(ChannelWatermark::WordPx);
    const ableem::Font &versionFont = ThemeAssets::fixedFonts().atSize(FONT_MED, ChannelWatermark::VersionPx);
    abgui::Context &ctx = gui->uiContext();
    const abgui::Style &style = ctx.style();
    const int x = ChannelWatermark::X;
    // under the pad plate
    const int y = ChannelWatermark::yBelow(plateBottom);
    const int wordW = gui->text().textWidth(wordFont, tag.word);
    const ableem::Rect chip(x, y, ChannelWatermark::chipWidth(wordW), ChannelWatermark::ChipHeight);
    const unsigned char alpha = ChannelWatermark::Alpha;
    if (!style.drawFrame(ctx, "chip", chip, alpha)) {
        renderer.setBlendMode(ableem::BlendMode::Blend);
        renderer.setDrawColor(ableem::Color(255, 255, 255, 24 * alpha / 255));
        renderer.fillRect(chip);
        renderer.setDrawColor(ableem::Color(style.edge.r, style.edge.g, style.edge.b, 200 * alpha / 255));
        renderer.drawRect(chip);
    }
    const int wordY = y + (ChannelWatermark::ChipHeight - wordFont.lineHeight()) / 2;
    const int versionY = y + (ChannelWatermark::ChipHeight - versionFont.lineHeight()) / 2;
    gui->text().setAlpha(alpha);
    gui->text().renderText_WithColor(wordFont, tag.word, x + ChannelWatermark::ChipPadding, wordY, style.text);
    gui->text().renderText_WithColor(versionFont, tag.version, chip.x + chip.w + ChannelWatermark::VersionGap, versionY,
                                     style.secondary);
    gui->text().setAlpha(255);
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
        string text = pluralGames(static_cast<size_t>(update.finishedGameCount));
        if (update.finishedFailedCount > 0)
            text += ", " + to_string(update.finishedFailedCount) + " " + _("failed");
        if (update.finishedRomCount > 0)
            text += ", " + to_string(update.finishedRomCount) + " " + _("ROMs");
        // the summary, then gone; with the notification timeout at 0 the progress bubble just goes
        if (infoTimeout() > 0)
            scanBubble.show(_("Scan complete:"), text, 0, 0, infoTimeout());
        else
            scanBubble.hide();
        scanRosterChangedSinceReload = true; // sub-dir rows and cross-folder duplicates only settle once done
    } else if (update.scanEnded) {
        // a scoped scan with no summary (a PE package in Mods/, Apps, Packages): the processor's bubble would stay
        scanBubble.hide();
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
    // the picker's counts are about every set, not just the one on screen
    if (scanRosterChangedSinceReload || update.finished || !update.playlistsWritten.empty() || update.appsChanged ||
        update.packagesChanged)
        forgetSetCounts(); // the Packages row's count too

    bool setAffected = selection.set == GameSet::PS1 || selection.set == GameSet::RetroArch;
    if (scanRosterChangedSinceReload && setAffected && !carousel.scrolling) {
        reloadGames();
    } else if ((update.appsChanged || (update.packagesChanged && selection.appCategory == AppCategory::Packages)) &&
               selection.set == GameSet::Apps && !carousel.scrolling) {
        // a processor changed Apps/ (a PE package turned into an App), or Packages/ changed under its own row: the
        // Apps set is read again
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
    raPlaylistsPending = false;
    if (Env::retroArchInstalled()) // the program, as everywhere (no folder: no playlists)
        raPlaylists = app.retroArch().playlistNames();
    pickPlaylistNames();
}

//*******************************
// GuiLauncher::pickPlaylistNames
//*******************************
// raPlaylists is current: the selection (and the session's copy) keeps its playlist by name where it still
// exists, else the first one. A selection that only carried an index (loadAssets ran before the playlists were
// read) is given its name from that index first.
void GuiLauncher::pickPlaylistNames() {
    auto pick = [&](GameSetSelection &sel) {
        if (sel.raPlaylistName.empty() && sel.raPlaylistIndex < raPlaylists.size())
            sel.raPlaylistName = raPlaylists[sel.raPlaylistIndex];
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
// GuiLauncher::loadPlaylistNames / pollBackgroundData
//*******************************
// wait false: the names if the background load is done, else raPlaylistsPending and an empty list (never a
// blocking read); wait true: the names, waiting for the load if it is still under way.
void GuiLauncher::loadPlaylistNames(bool wait) {
    raPlaylists.clear();
    raPlaylistsPending = false;
    if (!Env::retroArchInstalled())
        return;
    if (app.retroArch().tryPlaylistNames(raPlaylists))
        return;
    if (wait)
        raPlaylists = app.retroArch().playlistNames(); // joins the worker
    else
        raPlaylistsPending = true;
}

// once a frame: what the background loads finished since the last one
void GuiLauncher::pollBackgroundData() {
    if (raPlaylistsPending && app.retroArch().ready()) {
        loadPlaylistNames(false); // ready() is true: the names, no wait
        pickPlaylistNames();
        forgetSetCounts(); // the picker's RetroArch rows were not counted yet
        PLOG_INFO << "RetroArch playlists arrived: " << raPlaylists.size();
    }
}

//*******************************
// GuiLauncher::makePlayOutline
//*******************************
// what keeps Play readable over the covers' reflections: the dark outline the launcher's text has around Play's
// two images, following their transparency, the images drawn over it. Made once per theme on the CPU - one
// texture for the button, one for the text, which the frame draws at the text's pulse so the outline zooms with
// it. (A soft shadow and a coloured rim came first - a dead end, the owner, 2026-09-29.) The outline itself is
// PanelStyle::outlineOf (moved to autobleem-core, UIREV-2/UIREV-27) - the same halo the d-pad hint arrows and
// the meta icons now draw behind themselves, so there is one copy of this logic in the whole codebase.
void GuiLauncher::makePlayOutline(const LauncherTheme &theme) {
    playOutline = ableem::Texture();
    playTextOutline = ableem::Texture();
    if (!textShadow)
        return; // the theme said no to the text's halo (launcher.textShadow false) - Play's outline goes with it
    // the 1x files even when @2x ones are drawn (ThemeAssets::loadImage): their pixels are the logical size
    const ableem::Image button = ableem::Image::loadFile(theme.playButton);
    const ableem::Image text = ableem::Image::loadFile(theme.playText);
    // the 4:3 layout draws the images smaller: the outline with them
    const float k = layout.fourByThree ? layout.play.imageScale : 1.0f;
    auto scaled = [k](int v) { return k == 1.0f ? v : static_cast<int>(std::lround(v * k)); };
    playOutline = PanelStyle::outlineOf(renderer, button);
    if (playOutline.valid())
        playOutlineRect = ableem::Rect(playButton->x - 2, playButton->y - 2, scaled(button.size().w) + 5,
                                       scaled(button.size().h) + 5);
    playTextOutline = PanelStyle::outlineOf(renderer, text);
    if (playTextOutline.valid()) {
        playTextOutlineW = scaled(text.size().w) + 5;
        playTextOutlineH = scaled(text.size().h) + 5;
    }
}

//*******************************
// GuiLauncher::renderPlayFrame
//*******************************
// Play in a theme with the `play` frame (G5j): the frame stands still in the play button's box (540, 428, 200 x 68);
// the `play` icon and the label side by side pulse over it - drawn once into a texture (re-made when the word or
// the language changes, and when the render targets were lost) and that texture grown about the box's centre by
// playText's pulse, so no font is opened per size. An App's word is "Start".
void GuiLauncher::renderPlayFrame() {
    // the layout's box (540, 428, 200 x 68 on 16:9) and its content's sizes
    const int boxX = layout.play.box.x, boxY = layout.play.box.y, boxW = layout.play.box.w, boxH = layout.play.box.h;
    const int iconSize = layout.play.iconSize, gap = layout.play.gap, padding = layout.play.padding, margin = 4;
    const float maxZoom = 1.20f; // PsZoomBtn's: the content must still fit the box at the top of the pulse
    abgui::Context &ctx = gui->uiContext();
    const int centreX = boxX + boxW / 2, centreY = boxY + boxH / 2;
    if (layout.fourByThree) // the frame's corners and edges at the box's scale, so it keeps the 16:9 shape (200 x 68)
        ctx.style().drawFrame(ctx, "play", ableem::Rect(boxX, boxY, boxW, boxH), 255, boxH / 68.0f);
    else
        ctx.style().drawFrame(ctx, "play", ableem::Rect(boxX, boxY, boxW, boxH));

    const PsGame *game = carousel.selectedIsValid() ? carousel.games[carousel.selected].get() : nullptr;
    const ableem::Texture icon = ctx.icon("play");
    const int iconSpace = icon.valid() ? iconSize + gap : 0;
    const string label = ableem::Strings::upperUtf8(game != nullptr && game->app ? _("Start") : _("Play"));
    bool remake = !playContent.valid() || playContentAt != renderer.targetsLost();
    if (label != playLabel || !playLabelFont.valid()) {
        remake = true;
        playLabel = label;
        const int room = static_cast<int>((boxW - 2 * padding) / maxZoom) - iconSpace;
        playLabelFont = gui->text().fittingFont(FONT_BOLD, layout.play.fontMax, layout.play.fontMin, label, room);
        // even the smallest size too wide: cut characters (whole UTF-8 ones) and end on "..."
        while (gui->text().textWidth(playLabelFont, playLabel) > room && playLabel != "...") {
            size_t cut = playLabel.size() - 1;
            if (playLabel.size() > 3 && playLabel.compare(playLabel.size() - 3, 3, "...") == 0)
                cut = playLabel.size() - 4;
            while (cut > 0 && (static_cast<unsigned char>(playLabel[cut]) & 0xC0) == 0x80)
                cut--;
            playLabel = playLabel.substr(0, cut) + "...";
        }
    }
    if (remake) {
        const int textWidth = gui->text().textWidth(playLabelFont, playLabel);
        playContentW = iconSpace + textWidth + 2 * margin;
        playContentH = std::max(iconSize, playLabelFont.lineHeight()) + 2 * margin;
        playContent = ableem::Texture::createTarget(renderer, playContentW, playContentH);
        playContentAt = renderer.targetsLost();
        if (playContent.valid()) {
            const ableem::Color textColor = ctx.style().text;
            renderer.pushTarget(&playContent);
            renderer.setBlendMode(ableem::BlendMode::None);
            // cleared to the text's colour at alpha 0, so the edges blended in below do not come out dark
            renderer.setDrawColor(ableem::Color(textColor.r, textColor.g, textColor.b, 0));
            renderer.fillRect();
            renderer.setBlendMode(ableem::BlendMode::Blend);
            int x = margin;
            if (icon.valid()) {
                const ableem::Rect iconRect(x, (playContentH - iconSize) / 2, iconSize, iconSize);
                renderer.copy(icon, nullptr, &iconRect);
                x += iconSpace;
            }
            gui->text().renderText_WithColor(playLabelFont, playLabel, x,
                                             (playContentH - playLabelFont.lineHeight()) / 2, textColor);
            renderer.popTarget();
            playContent.setBlendMode(ableem::BlendMode::Blend);
        }
    }
    if (!playContent.valid())
        return;
    const ableem::FRect pulse = playText->drawRect();
    const float zoom = playText->ow > 0 ? pulse.w / static_cast<float>(playText->ow) : 1.0f;
    const float w = playContentW * zoom, h = playContentH * zoom;
    renderer.copy(playContent, nullptr, ableem::FRect(centreX - w / 2.0f, centreY - h / 2.0f, w, h));
}

//*******************************
// GuiLauncher::loadAssets
//*******************************
// load all assets needed by the screengame i
void GuiLauncher::loadAssets() {
    ableem::StartupTimer assetsTimer("launcher-load-assets");
    forgetSetCounts(); // Options, a new set of playlists, a fresh screen: count again
    PLOG_DEBUG << "Loading playlists";
    // a remembered Lightgun set is not offered without RetroArch: back to the PlayStation set
    app.session().launcher.set = setOrFallback(app.session().launcher.set, Env::retroArchInstalled());
    // the RetroArch and Lightgun sets list the playlists' games: that first screen waits for them. Any other
    // does not - the names arrive later (pollBackgroundData)
    {
        ableem::StartupTimer timer("launcher-playlist-names");
        loadPlaylistNames(app.session().launcher.set == GameSet::RetroArch ||
                          app.session().launcher.set == GameSet::Lightgun);
    }
    // the members, not locals: showOptions() reads them whenever the icon row changes (a local pair of the
    // same name here once left the members empty, and the first RetroArch game selected on a fresh screen
    // - every return from a RetroArch launch - crashed on headers[0])
    headers = {_("Settings"), _("Game"), _("Memory card"), _("Resume")};
    texts = {_("Customize AutoBleem settings"), _("Edit game parameters"), _("Edit memory card information"),
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

    // the layout profile (evoui_layout.h): on a 4:3 output a theme with a `layout4x3` block is drawn on the 640x480
    // canvas (prepareFrame asks the renderer for it); any other is the 1280x720 launcher, letterboxed at its shape
    layout = EvoLayout::wide();
    if (renderer.fourByThreeOutput()) {
        const ableem::ThemeLayout4x3 theme4x3 = ableem::loadThemeLayout4x3(app.theme().loadedPath());
        if (theme4x3.set)
            layout = EvoLayout::fromLayout4x3(theme4x3);
        PLOG_INFO << "4:3 output: the launcher is " << (layout.fourByThree ? "laid out for 4:3" : "letterboxed")
                  << " (theme " << app.theme().loadedPath() << ")";
    }
    carousel.positions.geometry = layout.carousel;
    scanBubble.right = extensionBubble.right = layout.bubbleRight;
    scanBubble.width = extensionBubble.width = layout.bubbleWidth;

    // count, x_start, y_start, fontEnum, fontHeight, separationBetweenLines
    notificationLines.create(2, layout.messageWidth, layout.bubbleRight);

    // silently record who's Player 1/2 right now (C9): loadAssets() runs at startup and every time the
    // display comes back after a game, both of which fire a burst of PadAdded/PadRemoved that must not
    // itself pop the notice - only a *later* live change should (see showPadAssignment()).
    seedPadAssignment();

    // C11: the swap was on but the emulator that just ran did not understand AB_PAD_ORDER
    // (LaunchService::launch() set this instead of silently doing nothing) - say so once, here, since the
    // screen that started the game is long gone by the time it returns.
    if (app.session().padOrderUnsupportedNotice) {
        app.session().padOrderUnsupportedNotice = false;
        notificationLines[1].setText(_("This PS1 emulator does not support swapping pads yet"), DefaultShowingTimeout);
    }

    scanRosterChangedSinceReload = false;

    startFadeIn(LauncherFadeInDuration);

    // was the classic menu's gamepadNotice - shown once here since there is no classic menu screen to carry it
    // - pointing at Network & Controllers (its controller mapping wizard) where an extension provides it
    if (gui->input().joystickCount() > gui->input().activePadCount()) {
        notificationLines[1].setText(
            networkProvided()
                ? _("NOTICE: At least one connected gamepad is not recognized. Set it up in Network & Controllers.")
                : _("NOTICE: At least one connected gamepad is not recognized."),
            10 * TicksPerSecond);
    }

    // UIREV-50/54: the theme load that just ran (start-up, or the display coming back) fell back to the default
    // theme - the picked zip is broken or cannot be unpacked, or the theme is gone: said here, after the lines were
    // created above, so no later load or line reset can take it away before the first frame
    {
        const string why = GuiOptions::themeFallbackText(Theme::takeFallbackReason());
        if (!why.empty())
            notificationLines[1].setText(why, 2 * DefaultShowingTimeout);
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
    {
        ableem::StartupTimer timer("launcher-first-set");
        switchSet(selection.set, true);
    }
    showSetName();

    gameName = "";
    publisher = "";
    year = "";
    players = "";
    PLOG_DEBUG << "Last Index " << selection.gameIndex;
    // within the set: a place carried over a restart may name a game the library no longer has (BUG-40)
    if (selection.gameIndex > 0 && selection.gameIndex < static_cast<int>(carousel.games.size())) {
        carousel.selected = selection.gameIndex;
        carousel.setInitialPositions(carousel.selected);
    }

    long time = gui->platform().ticks();

    PLOG_DEBUG << "Loading theme and creating objects";
    staticMeta = !theme.metaPanelSlides;
    textShadow = !theme.textShadow.set || theme.textShadow; // a theme has to say no
    const bool narrow = layout.fourByThree;
    background = addStaticElement(
        new PsObj("background", narrow && !layout.background.empty() ? layout.background : theme.background));
    background->x = 0;
    background->y = 0;
    background->visible = true;
    if (narrow && layout.background.empty() && background->w > 0 && background->h > 0) {
        // no 4:3 picture: the 16:9 one's middle, over the whole 4:3 canvas
        background->src = ableem::coverCrop(background->w, background->h, layout.canvasW, layout.canvasH);
        background->w = layout.canvasW;
        background->h = layout.canvasH;
    }

    // 4:3: the layout's footer picture, else none (the 16:9 band is 1280 wide; the hint bar's frame stands)
    PsObj *footer = addStaticElement(new PsObj("footer", narrow ? layout.footer : theme.footer));
    footer->y = layout.canvasH - footer->h;
    footer->visible = !narrow || !layout.footer.empty();

    playButton = addStaticElement(new PsObj("playButton", theme.playButton));
    playButton->y = layout.play.box.y;
    playButton->x = layout.play.box.x;
    playButton->visible = carousel.selected != -1;

    playText = addStaticElement(new PsZoomBtn("playText", theme.playText));
    playText->y = layout.play.box.y;
    playText->x = layout.play.textX;
    playText->visible = carousel.selected != -1;
    if (narrow) { // the two images at the layout's size (a theme without the `play` frame)
        for (PsObj *obj : {static_cast<PsObj *>(playButton), static_cast<PsObj *>(playText)}) {
            obj->w = static_cast<int>(std::lround(obj->w * layout.play.imageScale));
            obj->h = static_cast<int>(std::lround(obj->h * layout.play.imageScale));
            obj->src = ableem::Rect(0, 0, obj->ow, obj->oh);
            obj->ow = obj->w;
            obj->oh = obj->h;
        }
    }
    playText->ox = playText->x;
    playText->oy = playText->y;
    playText->lastTime = time;
    makePlayOutline(theme);

    settingsBack = addStaticElement(new PsSettingsBack(
        "playButton", narrow && !layout.settingsPanel.empty() ? layout.settingsPanel : theme.settingsPanel));
    settingsBack->bottom = layout.band.bottom;
    settingsBack->width = layout.canvasW;
    settingsBack->setCurLen(layout.band.closed);
    settingsBack->visible = true;

    meta = addStaticElement(new PsMeta("meta") /* the players icon is the icon set's since G5b */);
    meta->fonts = ThemeAssets::fixedFonts();
    meta->metrics = layout.meta.metrics;
    // a long title fits to the canvas's edge; in 4:3, where the details sit between the cover and the edge, to the
    // details' own right edge (the same margin as on their left)
    meta->screenRight = layout.fourByThree ? layout.meta.x + layout.meta.metrics.ruleWidth : layout.canvasW;
    meta->x = layout.meta.x;
    meta->y = layout.meta.y;
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
    arrow->x = layout.arrowX;
    arrow->y = layout.arrowY;
    if (layout.arrowSize > 0 && arrow->w > 0) { // the 4:3 layout's size, the image's shape kept
        arrow->h = arrow->h * layout.arrowSize / arrow->w;
        arrow->w = layout.arrowSize;
        arrow->maxMove = arrow->maxMove * layout.canvasH / 720;
    }
    arrow->originaly = arrow->y;
    arrow->visible = false;

    // built lazily: render() calls updateHintsIfNeeded() every frame, which rebuilds only when the state,
    // selection or language actually changed. Force that on the first frame of this fresh screen.
    lastHintSignature.clear();

    menu = std::make_unique<PsMenu>("menu", theme.menuIcons);
    menu->iconSize = layout.menu.icon;
    menu->pitch = layout.menu.pitch;
    menu->x = menu->ox = static_cast<float>(layout.menu.x);
    menu->y = menu->oy = static_cast<float>(layout.menu.yClosed);

    menuHead = addStaticElement(new PsCenterLabel("header"));
    menuHead->font =
        narrow ? ThemeAssets::fixedFonts().boldAtSize(layout.menu.headSize) : ThemeAssets::fixedFonts()[FONT_28_BOLD];
    menuHead->visible = false;
    menuHead->y = layout.menu.headY;
    menuText = addStaticElement(new PsCenterLabel("menuText"));
    menuText->visible = false;
    menuText->font = narrow ? ThemeAssets::fixedFonts().atSize(FONT_MED, layout.menu.textSize)
                            : ThemeAssets::fixedFonts()[FONT_22_MED];
    menuText->y = layout.menu.textY;
    if (narrow) { // centred under the selected icon, a long line shrunk to the canvas
        for (PsCenterLabel *label : {menuHead, menuText}) {
            label->centreX = layout.menu.captionX;
            label->maxWidth = std::min(layout.menu.captionX, layout.canvasW - layout.menu.captionX) * 2 - 16;
            label->fitMax = label == menuHead ? layout.menu.headSize : layout.menu.textSize;
            label->fitMin = 9;
        }
    }

    menuHead->setText(headers[0], fgColor);
    menuText->setText(texts[0], fgColor);

    sselector = addFrontElement(new PsStateSelector("selector"));
    sselector->font30 = ThemeAssets::fixedFonts()[FONT_28_BOLD];
    sselector->narrow = layout.fourByThree;
    sselector->visible = false;

    if (app.session().resumingGui) {
        PLOG_INFO << "Restoring GUI state";
        PsGamePtr &game = carousel.games[carousel.selected];

        if (app.session().emuMode == EmuMode::Pcsx) {
            if (app.resumePoints().exitedCleanly(*game)) {
                if (GameSettingsService::resumeModeOf(*game) == ResumePointService::Never) {
                    // the game editor's Resume row "Never" (EMU-26): the state the emulator wrote is not offered
                    app.resumePoints().discardRun(*game);
                } else {
                    sselector->loadSaveStateImages(game, true);
                    sselector->visible = true;
                    state = LauncherScreenState::Resume;
                }
            } else {
                notificationLines[1].setText(_("Oops! Game crashed. Resume point not available."),
                                             DefaultShowingTimeout);
            }
        } else if (app.session().emuMode == EmuMode::RetroArch) {
            if (game->foreign && !game->app) {
                // a RetroArch game: RetroArch wrote its state on the way out (a core that cannot save, or a
                // run that did not end cleanly, wrote none) - the same slot picker as after a PS1 game
                if (app.resumePoints().raStateWritten(*game)) {
                    sselector->loadSaveStateImages(game, true);
                    sselector->visible = true;
                    state = LauncherScreenState::Resume;
                } else if (app.resumePoints().raSupportsStates(*game) && app.resumePoints().raAutoSaveFailed(*game)) {
                    // the core says it can save, RetroArch tried on the way out and failed (its own log says so)
                    notificationLines[1].setText(_("This core cannot save its state"), DefaultShowingTimeout);
                }
            } else {
                // one of our PS1 games played in RetroArch's PS1 core: no slots of ours
                notificationLines[1].setText(_("AutoBleem resume points not available in RetroArch."),
                                             DefaultShowingTimeout);
            }
        }
        // EmuMode::Launcher (an App): returning to the launcher isn't a "resume points" situation - nothing to say
    }

    // a crash's logs, which the rc scripts took from RAM to the stick (autobleem-main's
    // docs/archive/quiet-stick-plan.md) - said once, the first time the launcher is shown after it; over the resume
    // messages above, which it explains
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
    playOutline = ableem::Texture();
    playTextOutline = ableem::Texture();
    playLabel.clear(); // the fitted font and the content texture go with the fonts
    playContent = ableem::Texture();
    wideLayer = ableem::Texture();
    hintLayer = ableem::Texture();
    playLabelFont = ableem::Font();
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
// GuiLauncher::retroArchInstalledCached
//*******************************
bool GuiLauncher::retroArchInstalledCached() const {
    const unsigned int now = gui->platform().ticks();
    if (raCheckedAt_ == 0 || now - raCheckedAt_ >= 2000) {
        raInstalled_ = Env::retroArchInstalled();
        raCheckedAt_ = now == 0 ? 1 : now;
    }
    return raInstalled_;
}

//*******************************
// GuiLauncher::hintSignature
//*******************************
// everything buildHintLines() reads, as a short string - updateHintsIfNeeded() rebuilds and re-lays-out the
// hint grid only when this actually changes, so render() can call it every frame for free (the "cache
// the layout" rule: the layout is redone on a state/selection/language change, not per frame).
string GuiLauncher::hintSignature() const {
    string sig = app.lang().currentLanguage();
    sig += "|s" + to_string(static_cast<int>(state));
    if (state == LauncherScreenState::Set) {
        sig += carousel.games.empty() ? "|empty" : "";
    } else if (state == LauncherScreenState::Resume) {
        if (sselector != nullptr) {
            sig += "|op" + to_string(sselector->operation);
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
        sig += retroArchInstalledCached() ? "|ra" : "";
    }
    return sig;
}

//*******************************
// hintLabel
//*******************************
// the label of a hint item, translated (the fixed labels of UIREV-36: "Open", "Resume game", "Save" - what they act
// on is on the screen, highlighted)
static string hintLabel(HintSlots::Item item) {
    using HintSlots::Item;
    switch (item) {
    case Item::Play:
        return _("Play");
    case Item::Start:
        return _("Start");
    case Item::Open:
        return _("Open");
    case Item::Resume:
        return _("Resume game");
    case Item::Save:
        return _("Save");
    case Item::GamesShown:
        return _("Games shown");
    case Item::PlayInRetroArch:
        return _("Play in RetroArch");
    case Item::DeleteSlot:
        return _("Delete slot");
    case Item::Random:
        return _("Random");
    case Item::GameMenu:
        return _("Game menu");
    case Item::Choose:
        return _("Choose");
    case Item::Slot:
        return _("Slot");
    case Item::Guide:
        return _("Guide");
    case Item::QuickMenu:
        return _("Quick menu");
    case Item::BackToGames:
        return _("Back to games");
    case Item::Back:
        return _("Back");
    case Item::DontSave:
        return _("Don't save");
    case Item::System:
        return _("System");
    case Item::None:
        break;
    }
    return "";
}

//*******************************
// GuiLauncher::buildHintLines
//*******************************
// the fixed grid of the hint bar (UIREV-36, evoui/controls/hint_slots.h - the slot of every item in every state):
// line 1 is what acts on the current selection, line 2 always Select/Start/Guide/System with the idle ones dimmed.
// "Play" not "Enter" (an App: "Start"); L2+R2 says "System", never "Options" (see docs/theme-format.md's hintBar
// entry). Both lines come back with one entry per column; an empty `markers` is an empty slot.
void GuiLauncher::buildHintLines(std::vector<Hint> &line1, std::vector<Hint> &line2) const {
    HintSlots::State slots;
    if (state == LauncherScreenState::Set) {
        slots.screen = HintSlots::Screen::GameMenu;
        slots.emptyRoster = carousel.games.empty();
    } else if (state == LauncherScreenState::Resume && sselector != nullptr) {
        slots.screen = sselector->operation == OP_LOAD ? HintSlots::Screen::ResumeLoad : HintSlots::Screen::ResumeSave;
        slots.slotUsed = sselector->slotActive[sselector->selSlot];
    } else {
        slots.emptyRoster = carousel.games.empty();
        const PsGame *game = carousel.selectedIsValid() ? carousel.games[carousel.selected].get() : nullptr;
        slots.app = game != nullptr && game->app;
        slots.retroArch = game != nullptr && !game->foreign && retroArchInstalledCached();
    }
    const HintSlots::Grid grid = HintSlots::gridFor(slots);
    auto fill = [](const std::array<HintSlots::Cell, HintSlots::Columns> &cells, std::vector<Hint> &line) {
        line.assign(HintSlots::Columns, Hint());
        for (int c = 0; c < HintSlots::Columns; c++) {
            if (cells[c].item == HintSlots::Item::None)
                continue;
            line[c].markers = HintSlots::markersOf(cells[c].item);
            line[c].label = hintLabel(cells[c].item);
            line[c].dim = cells[c].dim;
        }
    };
    fill(grid.line1, line1);
    fill(grid.line2, line2);
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
// GuiLauncher::hintBarRect
//*******************************
// the theme's launcher.hintBar - unset, the pill most themes paint at the bottom right. What the hint grid is laid
// out in and what the theme's `hintBar` frame is drawn into (G5e).
ableem::Rect GuiLauncher::hintBarRect() const {
    if (layout.hintBarFromLayout) // the 4:3 layout's
        return layout.hintBar;
    const LauncherTheme &theme = app.theme().launcher();
    if (theme.hintBar.set)
        return ableem::Rect(theme.hintBar.x, theme.hintBar.y, theme.hintBar.w, theme.hintBar.h);
    return layout.hintBar; // 560, 624, 680 x 72
}

ableem::Rect GuiLauncher::hintLayoutRect() const {
    const ableem::Rect bar = hintBarRect();
    if (layout.hintScale == 1.0f)
        return bar;
    return ableem::Rect(0, 0, static_cast<int>(std::lround(bar.w / layout.hintScale)),
                        static_cast<int>(std::lround(bar.h / layout.hintScale)));
}

//*******************************
// GuiLauncher::layoutHints
//*******************************
// Lays the hint grid out in the theme's hintBar through abgui::HintBar::layoutGrid (ab_gui UIREV-36 - the rules are
// there): the columns are as wide as the widest item that can ever sit in them, in every state, and the one font is
// the largest from 22 down to 14 at which the four fit - computed once per language and bar, never per state. An item
// is drawn at its column's left + 12, its label elided with "..." only if a column is still too narrow at the
// smallest font; no item is ever dropped. A hintBar under 48 px tall (an old theme that never expected two lines)
// shows line 1 only, at the bar's full height. The launcher measures (its buttons through PanelStyle, its fixed
// medium fonts) and keeps the result.
void GuiLauncher::layoutHints() {
    const ableem::Rect bar = hintLayoutRect();

    buildHintLines(hints, hints2);
    hintsOneLineOnly = abgui::HintBar::oneLineOnly(bar);
    if (hintsOneLineOnly)
        hints2.clear();

    PanelStyle style = gui->panelStyle();
    // the label font of a size: the 22 is the fixed FONT_22_MED, the smaller ones the medium face at that size
    auto fontOf = [](int size) -> ableem::Font & {
        return size == 22 ? ThemeAssets::fixedFonts()[FONT_22_MED] : ThemeAssets::fixedFonts().atSize(FONT_MED, size);
    };
    // buttons() adds a gap after the last button: an item's own buttons end before it
    auto buttonsWidth = [this, &style](const string &markers) { return style.buttonsWidth(*gui, markers) - 6; };

    // once per language and bar, whatever the state
    const string key = app.lang().currentLanguage() + "|" + to_string(bar.x) + "," + to_string(bar.y) + "," +
                       to_string(bar.w) + "," + to_string(bar.h);
    if (key != hintGridKey) {
        abgui::HintGridMeasure measure;
        measure.columnWidth = [this, &buttonsWidth, fontOf](int size, int column) {
            HintSlots::Item items[HintSlots::ItemCount];
            const int count = HintSlots::itemsOfColumn(column, items);
            int widest = 0;
            for (int i = 0; i < count; i++)
                widest = max(widest, buttonsWidth(HintSlots::markersOf(items[i])) + abgui::HintBar::IconGap +
                                         gui->text().textWidth(fontOf(size), hintLabel(items[i])));
            return widest;
        };
        measure.lineHeight = [fontOf](int size) { return fontOf(size).lineHeight(); };
        hintGrid = abgui::HintBar::layoutGrid(bar, measure);
        hintGridKey = key;
    }

    hintFont = fontOf(hintGrid.fontSize);
    hintLabelY = hintGrid.labelY[0];
    hintChipY = hintGrid.chipY[0];
    hintLabelY2 = hintGrid.labelY[1];
    hintChipY2 = hintGrid.chipY[1];
    // the items onto their slots; a label wider than its column's room is elided (the safety net)
    auto place = [&](std::vector<Hint> &line) {
        for (size_t c = 0; c < line.size(); c++) {
            Hint &hint = line[c];
            if (hint.markers.empty())
                continue;
            const int buttons = buttonsWidth(hint.markers);
            hint.chipX = hintGrid.itemX[c];
            hint.labelX = hint.chipX + buttons + abgui::HintBar::IconGap;
            const int labelRoom = hintGrid.itemRoom[c] - buttons - abgui::HintBar::IconGap;
            if (gui->text().textWidth(hintFont, hint.label) > labelRoom)
                hint.label = gui->text().elide(hintFont, hint.label, max(0, labelRoom));
        }
    };
    place(hints);
    place(hints2);
}

//*******************************
// GuiLauncher::prepareFrame
//*******************************
// before every frame of the loop (render() = this, then the stack's frame: cleared to transparent black - frameColor,
// the draw colour from then on - draw(), presented; docs/ab-gui-plan.md, G3e/G3z). The renderer's own clear() and
// present() still do the work, so a captureNextFrame() asked for before render() (the set picker's, an extension's
// backdrop), AB_SHOT and the DebugDriver's frame copy see this frame as they did.
bool GuiLauncher::prepareFrame() {
    gui->endBusy(); // the reload after a game, or after Options, is over once the launcher draws
    useFrameCanvas();
    return true;
}

// the 4:3 layout draws on the 640x480 canvas: asked for every frame, the renderer goes back to its rest one after it
// (the other screens have the 4:3 output's rest canvas, Gui::CrtCanvasW x H; a launcher with no 4:3 layout is the
// 1280x720 one, letterboxed)
void GuiLauncher::useFrameCanvas() {
    if (layout.fourByThree)
        renderer.setCanvas(layout.canvasW, layout.canvasH);
    else
        renderer.setCanvas(SCREEN_WIDTH, SCREEN_HEIGHT);
}

//*******************************
// GuiLauncher::drawWide
//*******************************
// The parts with no 4:3 design yet (the welcome card, the resume slots, a theme's snapPanel): on the 4:3 canvas they
// are drawn as on 1280x720 into a 16:9 picture, which goes on the canvas letterboxed at its shape (640 x 360 at y 60).
// On the 1280x720 canvas they are simply drawn.
void GuiLauncher::drawWide(const std::function<void()> &draw) {
    if (!layout.fourByThree) {
        draw();
        return;
    }
    const int w = SCREEN_WIDTH, h = SCREEN_HEIGHT;
    if (!wideLayer.valid() || wideLayerAt != renderer.targetsLost()) {
        wideLayer = ableem::Texture::createTarget(renderer, w, h);
        wideLayer.setBlendMode(ableem::BlendMode::Premultiplied);
        wideLayerAt = renderer.targetsLost();
    }
    if (!wideLayer.valid())
        return;
    const ableem::Color keep = renderer.drawColor();
    renderer.pushTarget(&wideLayer);
    renderer.setBlendMode(ableem::BlendMode::None);
    renderer.setDrawColor(ableem::Color(0, 0, 0, 0));
    renderer.fillRect();
    renderer.setBlendMode(ableem::BlendMode::Blend);
    renderer.setDrawColor(keep);
    renderer.setCanvas(w, h); // what it centres on and fills is the 1280x720 picture
    draw();
    renderer.setCanvas(layout.canvasW, layout.canvasH);
    renderer.popTarget();
    const int bandH = layout.canvasW * h / w;
    const ableem::Rect band(0, (layout.canvasH - bandH) / 2, layout.canvasW, bandH);
    renderer.copy(wideLayer, nullptr, &band);
}

//*******************************
// GuiLauncher::takeBackdrop / dropBackdrop
//*******************************
// UIREV-26 (G5r5): the launcher drawn once without the hint band and the bubbles (snapshotFrame),
// taken silently (the frame starts with the stack's clear(), so it goes straight into a render target and the window is
// left as it is - the System menu stays up, BUG-31), and handed to Gui
// for the screens opened from here. A capture that did not come (no texture) leaves no backdrop: the screens draw the
// theme's background, as before.
bool GuiLauncher::takeBackdrop(bool fresh) {
    const bool first = !gui->hasLauncherBackdrop();
    if (!first && !fresh)
        return false; // an outer screen's frame stands
    const void *previous = renderer.lastCapture().native();
    snapshotFrame = true;
    renderer.captureNextFrameSilently();
    render();
    snapshotFrame = false;
    const ableem::Texture frame = renderer.lastCapture();
    if (frame.valid() && frame.native() != previous) // the same texture as before = this capture did not come
        gui->setLauncherBackdrop(frame);
    return first && gui->hasLauncherBackdrop();
}

void GuiLauncher::dropBackdrop() {
    gui->clearLauncherBackdrop();
}

//*******************************
// GuiLauncher::welcomeCardShows
//*******************************
// UIREV-43: the PS1 "all games" set (not Favorites, History, folders, RetroArch, Apps) with no game in it - a fresh
// install. The card goes the moment a scan adds games (reloadGames() fills the carousel).
// Where there are no internal games (a Pi, a PC stick, Windows) "all games" is the USB games row (the folder tree's row
// 0): GameQueryService::gamesFor() turns AllGames into GamesSubdir there, and the set picker offers only that row.
bool GuiLauncher::welcomeCardShows() const {
    if (!carousel.games.empty() || selection.set != GameSet::PS1)
        return false;
    if (selection.ps1SelectState == Ps1SelectState::AllGames)
        return true;
    return selection.ps1SelectState == Ps1SelectState::GamesSubdir && selection.usbGameDirIndex == 0 &&
           !app.gameQuery().showInternalGames();
}

//*******************************
// GuiLauncher::renderWelcomeCard
//*******************************
// The card in the theme's panel frame (else the code sheet) where the covers would be: a bold title, a rule in the
// selection colour, the wrapped body and the signature. 660 wide, as tall as its content, centred on the empty
// cover's centre; all numbers at the 1280x720 logical canvas (the designer's welcome-a.png). On the 4:3 (CRT) canvas
// (640x480) the card is the same, 580 wide and centred on the canvas, its text 23 / 18 / 16 px, ending above the menu's
// icon row.
void GuiLauncher::renderWelcomeCard() {
    const bool narrow = layout.fourByThree;
    const int boxW = narrow ? 580 : 660, pad = narrow ? 20 : 34, centreY = 292;
    const int linePitch = narrow ? 24 : 31;
    const int titleH = narrow ? 32 : 40, ruleGap = narrow ? 10 : 16, signGap = narrow ? 8 : 14;
    const int signH = narrow ? 22 : 28, bodyTail = narrow ? 10 : 18;
    const int bottomY = 272; // 4:3: the card ends above the menu's icon row (a longer text grows it upward)
    abgui::Context &ctx = gui->uiContext();
    const abgui::Style &style = ctx.style();
    Fonts &fonts = ThemeAssets::fixedFonts();
    const ableem::Font &titleFont = narrow ? fonts.boldAtSize(23) : fonts[FONT_28_BOLD];
    const ableem::Font &bodyFont = narrow ? fonts.atSize(FONT_MED, 18) : fonts[FONT_22_MED];
    const ableem::Font &signFont = narrow ? fonts.boldAtSize(16) : fonts[FONT_20_BOLD];
    const string title = _("Hi, and welcome to AutoBleem!");
    const string signature = _("Cheers, screemer");
    // where the games go depends on the platform: the Pi reads its SD card, Windows the AutoBleem folder, the
    // console and the PC stick a USB stick
#if defined(AB_PLATFORM_RPI)
    const string body = _("Everything's set up - now drop some games into Games on your SD card, hit Re-Scan Games, "
                          "and you've got yourself a great console.");
#elif defined(AB_PLATFORM_WIN)
    const string body = _("Everything's set up - now drop some games into Games in your AutoBleem folder, hit Re-Scan "
                          "Games, and you've got yourself a great console.");
#else
    const string body = _("Everything's set up - now drop some games into Games on your stick, hit Re-Scan Games, and "
                          "you've got yourself a great console.");
#endif
    const vector<string> lines = gui->text().wrapLines(bodyFont, body, boxW - 2 * pad);

    const int boxH = pad + titleH + ruleGap + static_cast<int>(lines.size()) * linePitch + bodyTail + signH + pad - 6;
    const ableem::Rect box((renderer.width() - boxW) / 2, narrow ? bottomY - boxH : centreY - boxH / 2, boxW, boxH);
    style.sheet(ctx, box); // the theme's panel frame, else the code-drawn sheet

    const LauncherTheme &theme = app.theme().launcher();
    const ableem::Color accent =
        theme.colors.selection.set ? TextRenderer::toColor(theme.colors.selection, 255) : fgColor;

    int y = box.y + pad;
    gui->text().renderText_WithColor(titleFont, title, 0, y, fgColor, XALIGN_CENTER);
    y += titleH;
    renderer.setBlendMode(ableem::BlendMode::Blend);
    renderer.setDrawColor(ableem::Color(accent.r, accent.g, accent.b, 150));
    renderer.fillRect(ableem::Rect(box.x + pad, y + 4, boxW - 2 * pad, 1));
    y += ruleGap + 4;
    for (const string &line : lines) {
        if (!line.empty())
            gui->text().renderText_WithColor(bodyFont, line, box.x + pad, y, fgColor);
        y += linePitch;
    }
    y += signGap;
    gui->text().renderText_WithColor(signFont, signature, box.x + boxW - pad - signFont.width(signature), y, accent);
}

//*******************************
// GuiLauncher::draw
//*******************************
void GuiLauncher::draw() {
    if (ableem::ext_trace::active())
        ableem::ext_trace::note(std::string("launcher draw") + (snapshotFrame ? " (snapshot frame)" : "") +
                                " fadeAlpha=" + std::to_string(fadeAlpha));
    // every text on this screen (meta panel, labels, notifications, the state selector) gets the halo
    // for the length of this frame, on the launcher's own setting; the classic screens shown from here
    // render on the classic one, which goes back at the end of the frame
    const TextRenderer::Shadow classicShadow = gui->text().shadow();
    TextRenderer::Shadow shadow;
    shadow.enabled = textShadow;
    gui->text().setShadow(shadow);

    // the background (and the footer, which the row never reaches) behind the row, and everything else -
    // Play, the game's details, the menu's band - in front of it: the covers' reflections and the selected
    // cover's glow reach below the row, under Play and beside the details, and must not be drawn over them
    auto behindRow = [](const PsObj *obj) { return obj->name == "background" || obj->name == "footer"; };
    // the backdrop's frame (G5r5) has no hint band: the footer image (the band) and the hint bar's frame are left out
    for (auto &obj : staticElements) {
        if (behindRow(obj.get()) && !benchSkips(obj->name) && !(snapshotFrame && obj->name == "footer"))
            obj->render();
    }
    // the theme's hintBar frame (G5e): the panel behind the two hint lines, in the footer band's place (a theme with
    // the frame ships its footer image without the band - the art spec, 2.2), so whatever covered the band covers it;
    // the lines are drawn over it below. No frame = nothing drawn
    {
        abgui::Context &ctx = gui->uiContext();
        if (ctx.frame("hintBar").valid() && !benchSkips("hints") && !snapshotFrame)
            ctx.style().drawFrame(ctx, "hintBar", hintBarRect());
    }
    // the theme's logo element (G5q), above the background and under the carousel; none = nothing drawn
    if (gui->launcherLogo().valid())
        renderer.copy(gui->launcherLogo(), nullptr, layout.logoSet ? &layout.logo : &gui->launcherLogoRect());
    // an empty "all games" shelf gives way to the welcome card: no empty cover frame, no arrow
    const bool welcome = welcomeCardShows();
    if (welcome) {
        if (!benchSkips("carousel"))
            renderWelcomeCard();
    } else if (!benchSkips("carousel"))
        carousel.render();
    // a theme with the `play` frame (G5j) draws Play as that frame, the icon and the label - not the two images
    // and their outline
    const bool playFramed = gui->uiContext().frame("play").valid();
    if (!playFramed && playOutline.valid() && playButton != nullptr && playButton->visible &&
        !benchSkips("playOutline"))
        renderer.copy(playOutline, nullptr, &playOutlineRect);
    // the text's outline at the text's pulse: the text is drawn 2 px into its outline, both grown by the same zoom
    if (!playFramed && playTextOutline.valid() && playText != nullptr && playText->visible &&
        !benchSkips("playOutline")) {
        const ableem::FRect r = playText->drawRect();
        const float zoom = playText->ow > 0 ? r.w / static_cast<float>(playText->ow) : 1.0f;
        renderer.copy(
            playTextOutline, nullptr,
            ableem::FRect(r.x - 2.0f * zoom, r.y - 2.0f * zoom, playTextOutlineW * zoom, playTextOutlineH * zoom));
    }
    for (auto &obj : staticElements) {
        if (behindRow(obj.get()) || benchSkips(obj->name))
            continue;
        if (welcome && obj.get() == arrow)
            continue;
        if (obj.get() == meta && layout.meta.hideWhileScrolling && carousel.scrolling)
            continue; // a cover sliding to the shelf crosses the details' corner (EvoLayout::Meta)
        if (playFramed && obj.get() == playButton)
            continue;
        if (playFramed && obj.get() == playText) { // its pulse drives the frame, in the images' place in the order
            if (playText->visible)
                renderPlayFrame();
            continue;
        }
        obj->render();
    }
    drawWide([this] { renderSnap(); }); // a theme's snapPanel is a 1280x720 rect

    // any other set with no games shows only the empty shelf: one line under it says so
    if (carousel.games.empty() && !welcome && !snapshotFrame && !benchSkips("carousel")) {
        if (layout.fourByThree) { // inside the empty cover's place (the raised menu row is under it)
            const ableem::Font &font = ThemeAssets::fixedFonts().atSize(FONT_MED, layout.emptyTextSize);
            const string empty = _("No games here yet");
            gui->text().renderText_WithColor(font, empty, layout.carousel.centreX - font.width(empty) / 2,
                                             layout.emptyTextY, fgColor, XALIGN_LEFT);
        } else {
            gui->text().renderText_WithColor(ThemeAssets::fixedFonts()[FONT_22_MED], _("No games here yet"), 0,
                                             layout.emptyTextY, fgColor, XALIGN_CENTER);
        }
    }

    if (!benchSkips("menu"))
        menu->render();

    // the footer's two hint lines, built from the state and the selection - see buildHintLines(). Rebuilt
    // (and re-laid-out) only when updateHintsIfNeeded() finds they actually changed. Not in the backdrop's frame
    // (G5r5), nor the pad batteries and the top-right bubbles: what a screen over it draws is its own
    if (!snapshotFrame) {
        updateHintsIfNeeded();
        PanelStyle style = gui->panelStyle();
        // an item that does nothing in this state (line 2) is drawn at 35 %: the chip and the label together
        auto drawHint = [&](const Hint &hint, int chipY, int labelY) {
            if (hint.markers.empty())
                return;
            if (hint.dim) {
                gui->text().setAlpha(HintSlots::DimAlpha);
                style.buttonsFaded(gui->uiContext(), hint.markers, hint.chipX, chipY, HintSlots::DimAlpha);
            } else {
                style.buttons(*gui, hint.markers, hint.chipX, chipY);
            }
            gui->text().renderText_WithColor(hintFont, hint.label, hint.labelX, labelY, hintColor);
            gui->text().setAlpha(255);
        };
        if (!benchSkips("hints")) {
            auto drawLines = [&]() {
                for (const Hint &hint : hints)
                    drawHint(hint, hintChipY, hintLabelY);
                if (!hintsOneLineOnly)
                    for (const Hint &hint : hints2)
                        drawHint(hint, hintChipY2, hintLabelY2);
            };
            if (layout.hintScale == 1.0f) {
                drawLines();
            } else {
                // the 4:3 bar: the lines laid out at the 16:9 bar's proportions in a picture 1 / hintScale its size,
                // which goes into the bar scaled down - the chips, the pictures and the labels keep their 16:9
                // proportions to each other instead of 22 px chips beside 12 px labels
                const ableem::Rect virtualBar = hintLayoutRect();
                if (!hintLayer.valid() || hintLayerAt != renderer.targetsLost() || hintLayer.size().w != virtualBar.w ||
                    hintLayer.size().h != virtualBar.h) {
                    hintLayer = ableem::Texture::createTarget(renderer, virtualBar.w, virtualBar.h);
                    hintLayer.setBlendMode(ableem::BlendMode::Premultiplied);
                    hintLayerAt = renderer.targetsLost();
                }
                if (hintLayer.valid()) {
                    const ableem::Color keep = renderer.drawColor();
                    renderer.pushTarget(&hintLayer);
                    renderer.setBlendMode(ableem::BlendMode::None);
                    renderer.setDrawColor(ableem::Color(0, 0, 0, 0));
                    renderer.fillRect();
                    renderer.setBlendMode(ableem::BlendMode::Blend);
                    renderer.setDrawColor(keep);
                    drawLines();
                    renderer.popTarget();
                    const ableem::Rect bar = hintBarRect();
                    renderer.copy(hintLayer, nullptr, &bar);
                }
            }
        }

        // top-left corner, one icon per known wireless pad (C8); the channel tag (UIREV-40) under its plate
        renderChannelWatermark(renderPadBatteries());

        // the top-right corner: the scan's bubble, the notification lines stacked under it
        if (!benchSkips("bubbles"))
            scanBubble.render(*gui, time);
        int belowScan = scanBubble.visible() ? scanBubble.top + scanBubble.height() + 8 : scanBubble.top;
        extensionBubble.top = belowScan;
        extensionBubble.render(*gui, time);
        notificationLines.render(
            *gui, time, extensionBubble.visible() ? extensionBubble.top + extensionBubble.height() + 8 : belowScan);
    }

    for (auto &obj : frontElemets)
        if (!benchSkips("front") && (!layout.fourByThree || obj->visible))
            obj->render(); // the resume slots: 1280x720, or the 4:3 design on the 640x480 canvas
                           // (PsStateSelector::narrow)

    gui->text().setShadow(classicShadow);

    if (fadeAlpha > 0 && !snapshotFrame) {
        fadeAlpha = abgui::transition::fadeInAlpha(fadeMs, fadeDuration);
        renderer.setDrawColor(ableem::Color(0, 0, 0, fadeAlpha));
        renderer.setBlendMode(ableem::BlendMode::Blend);
        renderer.fillRect();
    }
}

//*******************************
// GuiLauncher::startFadeIn
//*******************************
// the black overlay from fully opaque to clear over durationMs: a non-ambient tween of the milliseconds gone (so the
// DebugDriver is busy while it runs), the alpha computed from it as before (abgui::transition::fadeInAlpha). Starting
// it again restarts it. The start-up drop from the top after the splash (the plan's decision 12) is the screen stack's
// transition instead (UIREV-48), so the overlay stays off while the stack brings the launcher in.
void GuiLauncher::startFadeIn(unsigned int durationMs) {
    fadeOwner.cancel();
    fadeDuration = durationMs;
    // none with the animations off (Options -> Interface -> "Animations"), and none while the screen stack brings the
    // launcher in itself (the drop from the top after the splash, UIREV-48)
    abgui::ScreenStack &stack = gui->uiContext().stack();
    if (!stack.animations() || stack.bringsIn(*this)) {
        fadeAlpha = 0;
        fadeMs = static_cast<float>(durationMs);
        return;
    }
    fadeAlpha = 255;
    fadeMs = 0.0f;
    gui->uiContext().stack().tweens().start(abgui::transition::fadeInClock(fadeMs, durationMs), fadeOwner);
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
    hideSettlePictures();
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
    hideSettlePictures();
    settleLoadsPending = true;
}

//*******************************
// GuiLauncher::switchState
//*******************************
void GuiLauncher::switchState(LauncherScreenState state, int time) {
    if (state == LauncherScreenState::Games) {
        app.audio().home_up.play();
        settingsBack->slideTo(layout.band.closed);
        playButton->visible = true;
        playText->visible = true;
        if (!staticMeta) {
            meta->slideTo(layout.meta.y);
        }
        this->state = LauncherScreenState::Games;
        arrow->visible = false;
        arrow->restart();
        menu->duration = evomotion::MenuSlideMs;
        menu->targety = layout.menu.yClosed;
        menu->active = false;
        menu->startTransition();
        menuHead->visible = false;
        menuText->visible = false;

        carousel.moveMainCover(state == LauncherScreenState::Games);
    } else {
        app.audio().home_down.play();
        settingsBack->slideTo(layout.band.open);
        playButton->visible = false;
        playText->visible = false;
        if (!staticMeta) {
            meta->slideTo(layout.meta.yRaised);
        }
        this->state = LauncherScreenState::Set;
        arrow->visible = true;
        arrow->restart();
        menu->duration = evomotion::MenuSlideMs;
        menu->targety = layout.menu.yOpen;
        menu->active = true;
        menu->startTransition();
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
// GuiLauncher::gameHasResumePoints
//*******************************
bool GuiLauncher::gameHasResumePoints(const PsGamePtr &game) const {
    if (game == nullptr || (game->foreign && game->app))
        return false;
    for (int slot = 0; slot < ResumePointService::SlotCount; slot++) {
        if (app.resumePoints().slotIsActive(*game, slot))
            return true;
    }
    return false;
}

//*******************************
// GuiLauncher::retranslateMenu
//*******************************
// the menu's headers and blurbs in the current language - once per language change (the loop calls it every pass; the
// Options screen, after a language pick, before it retakes the launcher snapshot, BUG-49)
void GuiLauncher::retranslateMenu() {
    if (headersLanguage == app.lang().currentLanguage())
        return;
    headersLanguage = app.lang().currentLanguage();
    captionOption = -1; // the caption is translated again too
    headers = {_("Settings"), _("Game"), _("Memory card"), _("Resume")};
    texts = {_("Customize AutoBleem settings"), _("Edit game parameters"), _("Edit memory card information"),
             _("Resume game from saved state point")};
}

//*******************************
// GuiLauncher::syncMenuCaption
//*******************************
void GuiLauncher::syncMenuCaption() {
    if (menu == nullptr || menuHead == nullptr || menuText == nullptr)
        return;
    // an icon move shows the caption of the icon it goes to from its first frame (the selection itself changes
    // when the move ends)
    int option = menu->selOption;
    if (menu->animating() && menu->transition == TR_OPTION)
        option += menu->direction == 0 ? -1 : 1;
    option = menu->optionAt(option); // a RetroArch game's Resume stands where the memory card does
    if (option < 0 || static_cast<size_t>(option) >= headers.size() || static_cast<size_t>(option) >= texts.size())
        return;
    if (option == captionOption)
        return;
    captionOption = option;
    if (option == 1 && menuForApp) { // an App's Game icon opens its Game settings
        menuHead->setText(_("Game settings"), fgColor);
        menuText->setText(_("Change how this program is started"), fgColor);
        return;
    }
    menuHead->setText(headers[option], fgColor);
    menuText->setText(texts[option], fgColor);
}

//*******************************
// GuiLauncher::showOptions
//*******************************
void GuiLauncher::showOptions() {
    bool enabled[4] = {true, false, false, false}; // nothing selected: AutoBleem settings only
    bool raRow = false;                            // a RetroArch game: Resume where the memory card would be
    bool forApp = false;                           // an App: settings, and its Game settings
    if (carousel.selectedIsValid()) {
        const PsGame &game = *carousel.games[carousel.selected];
        if (game.package) {
            // game data, not a program: no game settings, no resume points - Cross opens its info view
        } else if (!game.foreign) {
            enabled[1] = enabled[2] = enabled[3] = true; // a PS1 game: editor, memory cards, resume points
        } else if (game.app) {
            enabled[1] = true; // an App: its Game settings
            forApp = true;
        } else {
            enabled[1] = true; // a RetroArch game: its (light-gun) editor
            // and its save-state slots in the memory card's place - for a core that can save (a ScummVM or DOSBox
            // game has none)
            enabled[2] = app.resumePoints().raSupportsStates(game);
            raRow = true;
        }
    }
    const bool hasResume = raRow ? enabled[2] : enabled[3];
    if (!hasResume) {
        menu->resume = ableem::Texture(); // no resume icon, no picture of another game's resume point
        menu->resumeAvailable = true;
    } else {
        menu->resumeAvailable = gameHasResumePoints(carousel.games[carousel.selected]);
    }
    bool same = menu->resumeAtMemcard == raRow && menuForApp == forApp;
    for (int i = 0; i < 4; i++)
        same = same && (menu->enabled[i] == enabled[i]);
    if (same)
        return; // the row is already right - do not disturb an open menu

    for (int i = 0; i < 4; i++)
        menu->enabled[i] = enabled[i];
    menu->resumeAtMemcard = raRow;
    menuForApp = forApp;
    captionOption = -1; // the Game icon's caption is the App's "Game settings" now, or the editor's again
    menu->selOption = 0;
    menu->x = static_cast<float>(layout.menu.x);
    menu->ox = menu->x;
    menu->direction = 0;
    menu->duration = 100;
    // at rest for the state the launcher is in (a row rebuilt while it was closing - Select from the open
    // row switching to a set of another kind - used to stay half-closed with its icon half-zoomed)
    menu->settle(state == LauncherScreenState::Set,
                 state == LauncherScreenState::Set ? layout.menu.yOpen : layout.menu.yClosed);
    menuHead->setText(headers[0], fgColor);
    menuText->setText(texts[0], fgColor);
}