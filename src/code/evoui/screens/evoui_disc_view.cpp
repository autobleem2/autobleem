//
// GuiLauncher, the disc drive and its disc (docs/disc-drive-plan.md, pieces 5 and 8): the chip at the top of the screen
// that shows the drive, and the disc view - the carousel gives way to the disc's cover (or an audio CD's case), with
// its facts in the details section. The rules (which chip, when the view opens and closes, the crossfade) are
// discview:: in disc_view_logic.h; this file draws them and moves the carousel.
//
#include "evoui_launcher.h"
#include "evoui_cd_player.h"
#include "evoui_disc_rip.h"
#include "disc_rip_logic.h"
#include "gui/screens/gui_confirm.h"
#include "core/services/image_cd_drive.h"
#include "core/services/environment.h"
#ifndef _WIN32
#include "core/services/linux_cd_drive.h" // not built on Windows (core's CMakeLists): the fake drive only there
#endif
#include "gui/gui.h"
#include <ableem/engine/log.h>
#include <cstdlib>
#include <fstream>

using namespace std;

namespace {

// the file's bytes, empty when it cannot be read
vector<char> bytesOf(const string &path) {
    ifstream in(path, ios::binary);
    if (!in.good())
        return {};
    return vector<char>((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
}

} // namespace

//*******************************
// GuiLauncher::discDriveNow
//*******************************
discview::Drive GuiLauncher::discDriveNow() const {
    using State = DriveWatcher::State;
    switch (discStatus.state) {
    case State::NoDrive:
        return discview::Drive::NoDrive;
    case State::Empty:
        return discview::Drive::Empty;
    case State::Reading:
        return discview::Drive::Reading;
    case State::Ready:
        return discStatus.disc.kind == DiscIdentity::Kind::AudioCd ? discview::Drive::ReadyAudio
                                                                   : discview::Drive::ReadyGame;
    case State::NotPs1:
        return discview::Drive::NotPs1;
    case State::Unreadable:
        return discview::Drive::Unreadable;
    }
    return discview::Drive::NoDrive;
}

//*******************************
// GuiLauncher::buildDisc
//*******************************
// The disc in the drive as something the view can show. A PS1 disc: DiscGameService makes its PsGame (the library
// game's folder when one matches) with the covers db's picture; an audio CD: a bare PsGame and the theme's case.
void GuiLauncher::buildDisc() {
    discBuilt = false;
    discGame.reset();
    discPsGame.reset();
    discArt.clear();
    discTitle.clear();
    const DiscIdentity &id = discStatus.disc;
    if (id.kind == DiscIdentity::Kind::AudioCd) {
        auto game = make_shared<PsGame>();
        game->title = _("Audio CD");
        game->gameId = -1;
        discPsGame = game;
        discTitle = game->title;
        discArt = bytesOf(audioCdCaseFile());
    } else if (id.kind == DiscIdentity::Kind::Ps1Game) {
        DiscGameService service(app.library(), app.library().metadata());
        discGame = make_shared<DiscGame>(service.make(id.serial, id.region, id.volumeLabel, discStatus.sgIndex));
        discPsGame = discGame->game;
        if (!discPsGame) {
            discGame.reset();
            return;
        }
        discTitle = discPsGame->title;
        discArt = discGame->cover;
        if (discArt.empty())
            discArt = bytesOf(Env::getWorkingPath() + sep + "default.png");
    } else {
        return;
    }
    discAudio = id.kind == DiscIdentity::Kind::AudioCd;
    discBuiltFor = discStatus.changes;
    discBuilt = true;
}

//*******************************
// GuiLauncher::pollDisc
//*******************************
void GuiLauncher::pollDisc() {
    DriveWatcher *watcher = app.driveWatcher();
    if (watcher == nullptr)
        return;
    discStatus = watcher->status();
    const discview::Drive drive = discDriveNow();
    if (discview::isReady(drive) && (!discBuilt || discBuiltFor != discStatus.changes))
        buildDisc();

    const long now = gui->platform().ticks();
    if (discFading) {
        const long elapsed = now - discFadeStart;
        if (discview::fadeSwapDue(elapsed, discFadeSwapped)) {
            discFadeSwapped = true;
            swapDiscView(discFadeEntering);
        }
        if (discview::fadeDone(elapsed))
            discFading = false;
        return;
    }
    if (!discViewCanOpenIn() || r2Held)
        return;
    if (!carousel.games.empty() && (carousel.scrolling || menu->animating()))
        return;
    if (discview::closesByItself(drive, discViewOpen)) {
        leaveDiscView();
    } else if (discview::opensByItself(drive, discStatus.changes, app.discSeenChanges, discViewOpen) && discBuilt) {
        enterDiscView();
    }
}

//*******************************
// GuiLauncher::discViewCanOpenIn
//*******************************
// The view opens whatever the library holds: on the carousel, and also on an empty roster (an empty library, a set or
// the Apps tab with no games), where the screen rests in the Set state with only the settings icon. Not over the
// resume picker or the info screen.
bool GuiLauncher::discViewCanOpenIn() const {
    return state == LauncherScreenState::Games || (state == LauncherScreenState::Set && carousel.games.empty());
}

//*******************************
// GuiLauncher::startDiscFade
//*******************************
void GuiLauncher::startDiscFade(bool entering) {
    discFadeEntering = entering;
    discFadeSwapped = false;
    discFadeStart = gui->platform().ticks();
    // no animations (Options -> Interface): the swap is at once
    if (!gui->uiContext().stack().animations()) {
        swapDiscView(entering);
        discFadeSwapped = true;
        return;
    }
    discFading = true;
}

//*******************************
// GuiLauncher::enterDiscView / leaveDiscView
//*******************************
bool GuiLauncher::enterDiscView() {
    if (discViewOpen || discFading || !discBuilt || !discview::isReady(discDriveNow()))
        return false;
    if (!discViewCanOpenIn())
        return false;
    if (!carousel.games.empty() && carousel.scrolling)
        return false;
    app.discSeenChanges = discStatus.changes;
    // an empty roster leaves the menu row open on its settings (settleEmptyRoster): the disc's cover takes the
    // carousel's place, so the row closes first; leaving the view settles the empty roster again
    if (state != LauncherScreenState::Games)
        switchState(LauncherScreenState::Games, gui->platform().ticks());
    discSavedKey = selectedGameKey();
    rememberSelection(); // the Session keeps the library's place while the view is up
    app.audio().cursor.play();
    startDiscFade(true);
    return true;
}

bool GuiLauncher::leaveDiscView() {
    if (!discViewOpen || discFading)
        return false;
    app.audio().cancel.play();
    startDiscFade(false);
    return true;
}

//*******************************
// GuiLauncher::swapDiscView
//*******************************
// at the fade's middle: the carousel holds the disc alone, or the set again with the game it was on
void GuiLauncher::swapDiscView(bool entering) {
    if (entering) {
        if (!discBuilt || !discPsGame)
            return;
        discViewOpen = true;
        app.pendingDisc = discAudio ? nullptr : discGame;
        carousel.setSolo(discPsGame, discArt);
        carousel.snapMainCover(false);
        updateMeta();
        if (!discAudio)
            menu->setResumePic(app.resumePoints().lastPicture(*discPsGame));
    } else {
        discViewOpen = false;
        app.pendingDisc.reset();
        reloadGames(discSavedKey);
    }
    lastHintSignature.clear();
}

//*******************************
// GuiLauncher::restoreDiscView
//*******************************
// After a reload of the launcher's assets (Options closed, an extension's settings applied): the reload is the
// library's, so a disc view that was up comes back at once - the carousel holds the disc alone again, its library
// place still in discSavedKey. The disc taken out meanwhile: the library, as it now is (BUG-7).
void GuiLauncher::restoreDiscView(bool wasOpen) {
    if (!wasOpen)
        return;
    discViewOpen = false;
    app.pendingDisc.reset();
    if (discBuilt && discview::isReady(discDriveNow()))
        swapDiscView(true);
}

//*******************************
// GuiLauncher::reenterDiscViewAfterRun
//*******************************
// discBuilt stays false: the view is rebuilt from the watcher when the drive reads the disc again (buildDisc), and it
// stays up while the resume screen is, because the disc view only closes by itself from the Games state.
void GuiLauncher::reenterDiscViewAfterRun(const shared_ptr<DiscGame> &disc) {
    discSavedKey = selectedGameKey();
    discGame = disc;
    discPsGame = disc->game;
    discAudio = false;
    discArt = disc->cover;
    if (discArt.empty())
        discArt = bytesOf(Env::getWorkingPath() + sep + "default.png");
    discTitle = discPsGame->title;
    discViewOpen = true;
    app.pendingDisc = disc;
    carousel.setSolo(discPsGame, discArt);
    carousel.snapMainCover(false);
    lastHintSignature.clear();
}

//*******************************
// GuiLauncher::applyDiscMeta
//*******************************
// the details section's disc mode: the source tag "DISC", or an audio CD's tracks and total time
void GuiLauncher::applyDiscMeta() {
    if (meta == nullptr)
        return;
    if (!discViewOpen) {
        meta->discMode = PsMeta::DiscMode::None;
        return;
    }
    if (discAudio) {
        meta->updateTexts(discTitle, "", "", "", "", "", false, false, false, 1, false, false, false, false, "",
                          fgColor);
        meta->discMode = PsMeta::DiscMode::Audio;
        meta->audioTracks = discStatus.disc.trackCount;
        meta->audioTime = discview::totalTimeText(discStatus.disc.totalFrames);
    } else {
        meta->discMode = PsMeta::DiscMode::Game;
    }
}

//*******************************
// GuiLauncher::renderDiscChip
//*******************************
// Centred at the top, at the height of the notifications' first line (scanBubble.top) - never beside them, which sit at
// the right. The disc icon (the theme's `discDrive`) at the look the state gives, its words beside it.
void GuiLauncher::renderDiscChip() {
    if (app.driveWatcher() == nullptr)
        return;
    const discview::Drive drive = discDriveNow();
    const discview::Chip chip = discview::chipFor(drive);
    if (chip.look == discview::ChipLook::Hidden)
        return;
    string words;
    switch (chip.text) {
    case discview::ChipText::Reading:
        words = _("Reading the disc...");
        break;
    case discview::ChipText::Disc:
        // the view shows the disc itself: the lit icon alone marks it there
        if (!discViewOpen && !discTitle.empty())
            words = _("Disc") + ": " + discTitle;
        break;
    case discview::ChipText::NotPs1:
        words = _("This is not a PlayStation disc");
        break;
    case discview::ChipText::Unreadable:
        words = _("This disc cannot be played");
        break;
    case discview::ChipText::None:
        break;
    }
    const int alpha = discview::chipAlpha(chip.look, gui->platform().ticks());
    abgui::Context &ctx = gui->uiContext();
    ableem::Texture icon = ctx.icon("discDrive");
    const ableem::Size iconSize = icon.valid() ? icon.size() : ableem::Size{0, 0};
    const int iconBox = 30; // the icon is drawn square at this size
    const ableem::Font &font = ThemeAssets::fixedFonts().atSize(FONT_MED, 16);
    const int pad = 8, gap = 8;
    const int textW = words.empty() ? 0 : font.width(words);
    const int width = pad + (iconSize.w > 0 ? iconBox : 0) + (textW > 0 ? gap + textW : 0) + pad;
    const int height = iconBox + 2 * pad;
    const int screenW = layout.fourByThree ? 640 : 1280;
    const int left = (screenW - width) / 2;
    const int top = scanBubble.top;

    renderer.setBlendMode(ableem::BlendMode::Blend);
    renderer.setDrawColor(ableem::Color(0, 0, 0, 140 * alpha / 255));
    renderer.fillRect(ableem::Rect(left, top, width, height));
    int x = left + pad;
    if (iconSize.w > 0) {
        const ableem::Rect box(x, top + pad, iconBox, iconBox);
        icon.setAlphaMod(static_cast<unsigned char>(alpha));
        renderer.copy(icon, nullptr, &box);
        icon.setAlphaMod(255);
        x += iconBox + gap;
    }
    if (textW > 0) {
        gui->text().setAlpha(alpha);
        gui->text().renderText_WithColor(font, words, x, top + pad + (iconBox - font.lineHeight()) / 2, fgColor,
                                         XALIGN_LEFT);
        gui->text().setAlpha(255);
    }
}

//*******************************
// GuiLauncher::renderDiscFade
//*******************************
// The crossfade: the theme's background over the carousel with the alpha of discview::fadeAlpha - the old picture goes
// under it in the first half, the new one comes out from under it in the second. The footer band and the logo, which
// lie around the carousel, are drawn again at full strength on top of it.
void GuiLauncher::renderDiscFade() {
    if (!discFading || background == nullptr)
        return;
    const int alpha = discview::fadeAlpha(gui->platform().ticks() - discFadeStart);
    if (alpha <= 0)
        return;
    background->tex.setBlendMode(ableem::BlendMode::Blend);
    background->tex.setAlphaMod(static_cast<unsigned char>(alpha));
    background->render();
    background->tex.setAlphaMod(255);
    for (auto &obj : staticElements)
        if (obj->name == "footer")
            obj->render();
    abgui::Context &ctx = gui->uiContext();
    if (ctx.frame("hintBar").valid())
        ctx.style().drawFrame(ctx, "hintBar", hintBarRect());
    if (gui->launcherLogo().valid())
        renderer.copy(gui->launcherLogo(), nullptr, layout.logoSet ? &layout.logo : &gui->launcherLogoRect());
}

//*******************************
// GuiLauncher::openCdPlayer
//*******************************
// Cross on an audio CD: the player on the drive. The emulator-style hand-over - the watcher lets go of the drive
// while the player has it, and reads the disc again after.
void GuiLauncher::openCdPlayer() {
    DriveWatcher *watcher = app.driveWatcher();
    if (watcher == nullptr)
        return;
    watcher->pause();
    {
        GuiCdPlayer screen(*gui);
#ifndef _WIN32
        LinuxCdDrive linuxDrive;
#endif
        const string fake = DriveWatcher::fakeCue();
        bool opened = true;
        if (!fake.empty()) {
            screen.cuePath = fake;
        } else {
#ifndef _WIN32
            string error;
            if (linuxDrive.open(discStatus.sgIndex, error) && linuxDrive.readToc(screen.toc, error)) {
                screen.drive = &linuxDrive;
            } else {
                PLOG_ERROR << "CD player: the drive: " << error;
                opened = false;
            }
#else
            PLOG_ERROR << "CD player: no real drive on Windows";
            opened = false;
#endif
        }
        if (opened) {
            BackdropScope backdrop(*this);
            screen.show();
        }
    }
    watcher->resume();
    forgetHeldModifiers();
}

//*******************************
// GuiLauncher::copyDiscOffered
//*******************************
bool GuiLauncher::copyDiscOffered() const {
    return discGame != nullptr && discrip::offered(discDriveNow(), discViewOpen);
}

//*******************************
// GuiLauncher::copyDiscToGames
//*******************************
// Square on a PS1 disc (named by the databases' title, or by its serial when they do not know it). The question (what, where, the room it takes), then the rip under a popup
// that blocks the launcher with Abort as its one action, then the rescan and the new game selected. The drive is the
// watcher's no longer while this runs (it is handed back after, as the CD player does).
void GuiLauncher::copyDiscToGames() {
    DriveWatcher *watcher = app.driveWatcher();
    if (watcher == nullptr || !copyDiscOffered())
        return;
    auto message = [this](const string &text) {
        GuiConfirm box(*gui);
        box.title = _("Copy disc");
        box.label = text;
        box.confirmLabel = _("OK");
        box.cancelLabel = _("Back");
        BackdropScope backdrop(*this);
        box.show();
    };

    const string title = discrip::copyTitle(discGame->inDatabase, discPsGame->title, discPsGame->serial);
    const int disc = discrip::discNumberOf(discPsGame->recordName);
    const string gamesDir = Env::getPathToGamesDir();
    const string target = discrip::chdPath(gamesDir, title, disc);
    const string folder = discrip::folderOf(gamesDir, title);
    if (ableem::DirEntry::exists(target)) {
        message(discrip::fill(_("Already in Games: %1"), {discrip::chdName(title, disc)}));
        return;
    }

    watcher->pause();
    unique_ptr<CdDrive> drive;
    CdToc toc;
    string error;
    const string fake = DriveWatcher::fakeCue();
    if (!fake.empty()) {
        auto image = make_unique<ImageCdDrive>();
        if (image->open(fake, error) && image->readToc(toc, error))
            drive = move(image);
    } else {
#ifndef _WIN32
        auto linuxDrive = make_unique<LinuxCdDrive>();
        if (linuxDrive->open(discStatus.sgIndex, error) && linuxDrive->readToc(toc, error))
            drive = move(linuxDrive);
#else
        error = "no real drive on Windows";
#endif
    }
    if (!drive) {
        PLOG_ERROR << "Disc copy: the drive: " << error;
        watcher->resume();
        message(_("The disc cannot be read now"));
        return;
    }

    // the room: the folder does not exist yet, so the Games directory's filesystem is asked
    const uint64_t needed = RipJob::neededBytes(toc);
    uint64_t freeBytes = 0;
    if (discrip::preflight(target, gamesDir, needed, &freeBytes) == discrip::Check::NoRoom) {
        watcher->resume();
        message(discrip::fill(_("Not enough room in Games: the copy needs %1, %2 is free."),
                              {discrip::bytesText(needed), discrip::bytesText(freeBytes)}));
        return;
    }

    {
        GuiConfirm ask(*gui);
        ask.title = _("Copy the disc to Games");
        ask.label =
            discrip::fill(_("Copy \"%1\" to %2? It takes %3 of the %4 free."),
                          {title, "Games/" + GameInstaller::folderNameFor(title) + "/" + discrip::chdName(title, disc),
                           discrip::bytesText(needed), freeBytes > 0 ? discrip::bytesText(freeBytes) : "-"});
        ask.confirmLabel = _("Copy");
        ask.cancelLabel = _("Back");
        BackdropScope backdrop(*this);
        ask.show();
        if (!ask.result) {
            watcher->resume();
            return;
        }
    }

    // LibCrypt: the .sbi the launcher fetched for the serial is copied beside the CHD; without one the marks are read
    // from the drive after the rip
    const string sbi =
        discrip::chooseSbi({discGame->game->ssFolder, DiscGameService::folderForKey(discGame->keySerial)},
                           discStatus.disc.serial, ableem::DirEntry::exists);
    ableem::DirEntry::createDirs(folder);
    RipJob::Result result;
    {
        GuiDiscRip rip(*gui);
        rip.drive = drive.get();
        rip.target = target;
        rip.writeSbi = sbi.empty();
        BackdropScope backdrop(*this);
        rip.show();
        result = rip.result;
    }
    drive.reset();
    watcher->resume();
    forgetHeldModifiers();

    if (result.cancelled) {
        message(_("Cancelled"));
        return;
    }
    if (!result.ok) {
        message(discrip::fill(_("The copy failed: %1"), {result.error}));
        return;
    }
    if (!sbi.empty())
        ableem::DirEntry::copyFile(sbi, discrip::sbiPathFor(result.file));
    if (discrip::inSet(discPsGame->recordName))
        discrip::updateM3u(gamesDir, title);

    ripSelectFolder = folder;
    ripScanFinished = false;
    app.scans().requestScan();

    string done = _("The disc is copied to Games.");
    if (result.badSectors > 0)
        done += " " + discrip::fill(_("%1 sectors could not be read and were written as zeros."),
                                    {to_string(result.badSectors)});
    if (!result.sbiNote.empty())
        done += " " + result.sbiNote;
    message(done);
}

//*******************************
// GuiLauncher::selectRippedGame
//*******************************
// Once the rescan has found the copied disc: the disc view gives way to the library, and the new game is selected
void GuiLauncher::selectRippedGame() {
    if (ripSelectFolder.empty() || !ripScanFinished || discFading || carousel.scrolling)
        return;
    if (discViewOpen) {
        leaveDiscView();
        return;
    }
    if (state != LauncherScreenState::Games)
        return;
    for (int i = 0; i < static_cast<int>(carousel.games.size()); i++) {
        const string &folder = carousel.games[i]->folder;
        if (folder == ripSelectFolder || folder == ripSelectFolder + "/") {
            carousel.selected = i;
            carousel.setInitialPositions(i);
            updateMeta();
            menu->setResumePic(app.resumePoints().lastPicture(*carousel.games[i]));
            break;
        }
    }
    ripSelectFolder.clear();
    ripScanFinished = false;
}
