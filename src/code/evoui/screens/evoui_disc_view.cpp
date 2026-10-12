//
// GuiLauncher, the disc drive and its disc (docs/disc-drive-plan.md, pieces 5 and 8): the chip at the top of the screen
// that shows the drive, and the disc view - the carousel gives way to the disc's cover (or an audio CD's case), with
// its facts in the details section. The rules (which chip, when the view opens and closes, the crossfade) are
// discview:: in disc_view_logic.h; this file draws them and moves the carousel.
//
#include "evoui_launcher.h"
#include "evoui_cd_player.h"
#include "core/services/environment.h"
#include "core/services/linux_cd_drive.h"
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
        LinuxCdDrive linuxDrive;
        const string fake = DriveWatcher::fakeCue();
        bool opened = true;
        if (!fake.empty()) {
            screen.cuePath = fake;
        } else {
            string error;
            if (linuxDrive.open(discStatus.sgIndex, error) && linuxDrive.readToc(screen.toc, error)) {
                screen.drive = &linuxDrive;
            } else {
                PLOG_ERROR << "CD player: the drive: " << error;
                opened = false;
            }
        }
        if (opened) {
            BackdropScope backdrop(*this);
            screen.show();
        }
    }
    watcher->resume();
    forgetHeldModifiers();
}
