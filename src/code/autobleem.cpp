//
// AutoBleem: the program - App plus the loop that runs it.
//
#include "autobleem.h"
#include <ableem/ui/debug_driver.h>
#include "core/services/system.h"
#include <ctime>
#include "evoui/screens/evoui_launcher.h"
#include "launch_picture.h"
#include "ra_glibc_check.h"
#include "gui/screens/gui_keep_display.h"

#include <cstdlib>
#include <iostream>
#include <unistd.h>
#include <ableem/engine/log.h>
#include <ableem/engine/startup_timer.h>
#include <ableem/engine/update_catalog.h>
#include "core/version.h"
#include <ableem/engine/theme_spec.h>
#include "core/services/default_theme.h"
#include "core/services/environment.h"

using namespace std;

#ifdef AB_APPLIANCE
// How long the waiting picture is held before the display is handed to the game. It is not a delay
// for its own sake: without it the picture is drawn and taken away in the same instant and the
// hand-over looks like a flicker. A game takes seconds to come up, so this is not felt.
constexpr int WaitingPictureDuration = 400; // ms
#endif

//*******************************
// AutoBleem::AutoBleem
//*******************************
AutoBleem::AutoBleem() : App(makeProcessRunner()) {
#ifdef AB_PLATFORM_PSC
    // the console's power button: AppBase wired it to a halt; the launcher's power off is the standby
    // (App::requestPowerOff) - the tools under apps/ keep the halt
    gui_->platform().setPowerOffHandler([this]() {
        gui_->drawText(_("POWERING OFF... PLEASE WAIT"));
        requestPowerOff();
    });
#endif
}

#ifdef AB_DEBUG_HOST
namespace {
// On a PC there is no rc/launch.sh to run and no emulator behind it: show what would have happened and
// come straight back, which is what the interceptors' #ifdef used to do.
class SplashProcessRunner : public ProcessRunner {
public:
    void run(const LaunchPlan &plan) override {
        PLOG_INFO << "would run " << plan.toString();
        Gui::splash("I'm sorry Dave.  I'm afraid I can't do that.");
    }
    bool needsExclusiveDisplay() const override { return false; } // it draws on the launcher's own window
};
} // namespace
#endif

//*******************************
// AutoBleem::makeProcessRunner
//*******************************
unique_ptr<ProcessRunner> AutoBleem::makeProcessRunner() {
#if defined(AB_DEBUG_HOST)
    return unique_ptr<ProcessRunner>(new SplashProcessRunner());
#elif defined(AB_PLATFORM_WIN)
    // the window stays up behind the emulator's: its events are pumped (and dropped) through the run
    return unique_ptr<ProcessRunner>(new WinProcessRunner([]() { Gui::getInstance()->input().flushEvents(); }));
#else
    return unique_ptr<ProcessRunner>(new ForkProcessRunner());
#endif
}

//*******************************
// AutoBleem::openLibrary
//*******************************
bool AutoBleem::openLibrary() {
    if (!gameLibrary.openCoversAndUsbGames(true /* the rdb and covers dbs load in the background */)) {
        return false;
    }

#ifdef AB_PLATFORM_PSC
    // if the /System/Databases/internal.db doesn't exist make a copy from the PSC
    PLOG_INFO << "Importing internal games from PSC to USB";
    System::execUnixCommand((Env::getPathToRCDir() + sep + "backup_internal.sh").c_str());
#endif

    // off the console this opens (and so creates) an empty internal.db: nothing ever queries it - the internal
    // sets are unreachable there - but GameCatalogService and GameSettingsService still expect the handle to exist.
    return gameLibrary.openInternalGames();
}

//*******************************
// AutoBleem::retroArchRefused
//*******************************
// A RetroArch built on a newer system than the console's (a later build next to a stock RetroBoot 1.1 tree) asks
// for GLIBC_2.xx the console's loader does not have, and exits at once with only a line in its own stderr: the
// check is made on its binary before it is started (ra_glibc_check.h), and the player told what to do instead.
bool AutoBleem::retroArchRefused() {
    const string binary = LaunchService::retroArchExecutable();
    if (binary.empty())
        return false;
    const GlibcVersion needed = highestGlibcNeededInFile(binary);
    const GlibcVersion have = runningGlibc();
    if (!needsNewerGlibc(needed, have))
        return false;
    PLOG_WARNING << binary << " needs GLIBC_" << needed.major << "." << needed.minor << ", the console has "
                 << have.major << "." << have.minor << " - RetroArch is not started";
    extensionRequests_.message = _("RetroArch needs a newer system library than this console has - use the RetroArch "
                                   "that comes with AutoBleem");
    return true;
}

//*******************************
// AutoBleem::runOutside
//*******************************
void AutoBleem::runOutside(bool retroArch, const char *picture, const std::function<void()> &body) {
    if (retroArch && retroArchRefused())
        return;

    // the extensions first: they may hold textures, and their threads should leave the machine to the game
    extensions_.suspend();
    gui_->finish(); // fades the music out and closes the mixer

    gui_->input().flushPads();
    // the emulator needs the whole machine: audio (closed above), the pads (flushed above) and the display.
    // Without a compositor - a Raspberry Pi on KMS/DRM - our window is the DRM master and pcsx-ab's
    // SDL_Init(VIDEO) fails while it exists, so the window goes too; display(true) below rebuilds it.
    if (runner_->needsExclusiveDisplay()) {
#ifdef AB_APPLIANCE
        // An appliance - a Pi or the PC stick - is the whole session: bare KMS, no compositor, and a
        // text console on the tty underneath. Two things follow, and neither applies to the console
        // (Weston mediates there, which is what lets absplash hold a picture over a running game).
        //
        // First, the waiting picture has to be shown *before* the display goes, because there is only
        // one DRM master: a separate splash process holding it would stop the emulator starting, and
        // waiting for the launcher's window to come back would deadlock against it. So the last thing
        // we scan out is the picture, and it is what stands there while the game loads.
        //
        // Second, giving the display up puts the tty back into text mode, and whatever was last
        // printed on it comes back - a login prompt, a systemd line, the tail of a script. Blanked,
        // the gap reads as black instead of as somebody else's terminal.
        gui_->showSplashPicture(picture);
        usleep(WaitingPictureDuration * 1000); // long enough to be seen rather than flicker
        gui_->releaseDisplay();
        System::blankConsole();
#else
        gui_->releaseDisplay();
#endif
    }
    // on a desktop the emulator opens its own window over ours, which stays: the picture the console's
    // absplash shows around a RetroArch run is what is under the emulator's window, and what shows the
    // moment that window goes - not the desktop, and not a carousel that is not ready to be used yet
    if (runner_->keepsLauncherWindow()) {
        gui_->showSplashPicture(picture);
    }

    body();
    takeEmulatorOutputMode();

    if (runner_->keepsLauncherWindow()) {
        gui_->showSplashPicture("autobleem.jpg");
        gui_->raiseWindow(); // Windows hands the focus back to us by itself; this makes sure of it
    }

    // a moment for the machine to settle before the window comes back: on the PSC the GPU frees the
    // emulator's memory a few seconds after the process is gone, and a RetroArch 1.22.2 session (its XMB
    // alone holds hundreds of icon textures) leaves a lot to free - the launcher's own uploads failed at
    // 300 ms and at 1 s (twice, after Quake; the third rebuild at ~4 s held), so 2 s here and the
    // rebuild-on-loss in run() for the rest. A desktop keeps its window and needs none of that.
    if (runner_->needsExclusiveDisplay()) {
#ifdef AB_APPLIANCE
        // the game has just put the tty back into text mode on its way out, so blank it again: this
        // is the other half of the hand-over, and the wait below would otherwise be spent looking at
        // a console rather than at black
        System::blankConsole();
#endif
        usleep((retroArch ? 2000 : 300) * 1000);
    }

    gui_->input().probePads();
    // remove all events if something left
    gui_->input().flushEvents();

    // RetroArch may have saved a screenshot or an auto save state just now: forget the listings
    thumbnails().clearCache();

    gui_->display(true);
    session_.resumingGui = true; // the launcher fades back in over the game that just ended
    extensions_.resume();

    // RetroBoot's return splash (abimage, the AutoBleem 2 emblem) waits for this file to go; it used to
    // be rc/launch_rb.sh that removed it, before our window existed - a black gap between the two
    unlink("/tmp/.abload");
}

//*******************************
// AutoBleem::takeEmulatorOutputMode
//*******************************
// The display mode the player picked in the emulator's own menu (<runtime>/outputmode, OutputMode) becomes the
// launcher's: config.ini, and the window made after the game. Not on the console, where the mode is Weston's -
// the emulator there cannot change it.
void AutoBleem::takeEmulatorOutputMode() {
    string token;
    if (!OutputMode::readToken(OutputMode::emulatorFile(), token))
        return;
    DirEntry::removeFile(OutputMode::emulatorFile());
#ifndef AB_PLATFORM_PSC
    const OutputMode mode = OutputMode::parse(token);
    string &value = cfg_.inifile.values[OutputMode::ConfigKey];
    if (mode.token() == OutputMode::parse(value).token())
        return;
    PLOG_INFO << "The emulator switched the display to " << mode.token() << " - the launcher follows";
    value = mode.token();
    cfg_.save();
    ableem::Platform::setOutputMode(mode.w, mode.h);
#endif
}

//*******************************
// AutoBleem::switchOutputMode / tryOutputMode / confirmPendingOutputMode
//*******************************
// Options -> Display off the console: the window made again in the new mode, in-process - everything that
// holds a texture let go first, as for a game (runOutside)
void AutoBleem::switchOutputMode(const OutputMode &mode) {
    extensions_.suspend();
    gui_->finish();
    gui_->releaseDisplay();
#ifdef AB_APPLIANCE
    System::blankConsole();
#endif
    ableem::Platform::setOutputMode(mode.w, mode.h);
    gui_->input().flushEvents();
    gui_->display(true);
    applySafeMargin(); // the new window's shape decides (a margin left over from the old mode would not fit)
    gui_->endBusy();
    extensions_.resume();
}

// The margin and the picture height of the window as it really is (it may not be the mode asked for): 720x480 the
// tube's margin, any other 4:3 size the VGA one, a wide output none; the height adjust is one value for all
void AutoBleem::applySafeMargin() {
    const ableem::Size window = gui_->platform().windowSize();
    OutputMode shown;
    shown.w = window.w;
    shown.h = window.h;
    gui_->renderer().setSafeMargin(OutputMode::safeMarginFor(shown, cfg_.inifile.values[OutputMode::MarginKey],
                                                             cfg_.inifile.values[OutputMode::VgaMarginKey]));
    // the picture height of every 4:3 output; recorded on a wide one too, for a later switch to 4:3
    gui_->renderer().setVerticalAdjust(OutputMode::vsize(cfg_.inifile.values[OutputMode::VsizeKey]));
}

// the new mode on the screen, and kept only with a Cross within GuiKeepDisplay::Seconds - else the old one back
void AutoBleem::tryOutputMode(const string &token) {
    string &value = cfg_.inifile.values[OutputMode::ConfigKey];
    const OutputMode was = OutputMode::parse(value);
    const OutputMode want = OutputMode::parse(token);
    PLOG_INFO << "Trying display mode " << want.token() << " (was " << was.token() << ")";
    switchOutputMode(want);
    GuiKeepDisplay keep(*gui_);
    keep.modeLabel = want.isAuto() ? _("Auto") : want.label();
    keep.show();
    if (keep.result) {
        value = want.token();
        cfg_.save();
        PLOG_INFO << "Display mode " << want.token() << " kept";
        useDefaultThemeFor(want);
    } else {
        PLOG_INFO << "Display mode " << want.token() << " not confirmed - back to " << was.token();
        switchOutputMode(was);
    }
}

// The console, right after the start: rc/boot.sh started Weston in the pending mode Options asked for (the
// launcher left for it). Kept with a Cross - config.ini takes it; not kept, the launcher leaves again (false)
// and boot.sh goes back to config.ini's mode. No pending file: nothing to ask.
bool AutoBleem::confirmPendingOutputMode() {
    string token;
    if (!OutputMode::readToken(OutputMode::pendingFile(), token))
        return true;
    DirEntry::removeFile(OutputMode::pendingFile()); // asked once: a crash from here comes back in the old mode
    // The mode counts only when the window really has its size: when Weston did not take it (its restart failed,
    // or this launcher was started under the old loop) the player is looking at another mode, and a confirm would
    // keep one he never saw. Not applied: no question, the launcher leaves for the previous mode.
    const OutputMode pending = OutputMode::parse(token);
    const ableem::Size window = gui_->platform().windowSize();
    if (!pending.shownAt(window.w, window.h)) {
        PLOG_WARNING << "Display mode " << token << " not applied: the window is " << window.w << "x" << window.h
                     << ", not " << pending.w << "x" << pending.h << " - leaving for the previous one";
        return false;
    }
    GuiKeepDisplay keep(*gui_);
    keep.modeLabel = OutputMode::parse(token).label();
    keep.show();
    if (!keep.result) {
        PLOG_INFO << "Display mode " << token << " not confirmed - leaving for the previous one";
        return false;
    }
    cfg_.inifile.values[OutputMode::ConfigKey] = OutputMode::parse(token).token();
    cfg_.save();
    PLOG_INFO << "Display mode " << token << " kept";
    useDefaultThemeFor(OutputMode::parse(token));
    return true;
}

// The CRT 4:3 mode is in use - kept (the player confirmed it works) or already in config.ini at the start: a theme
// without a 4:3 layout is replaced by the default theme - never while the mode is still being tried, so a mode the
// display cannot show costs the player nothing. The assets are loaded again from the new theme, and the launcher
// says so on its notification line.
void AutoBleem::useDefaultThemeFor(const OutputMode &inUse) {
    string &theme = cfg_.inifile.values["theme"];
    const string themes = Env::getPathToThemesDir();
    const string target = OutputMode::themeToSwitchTo(
        inUse, theme, ableem::ThemeSpec::supports4x3(themes + sep + theme + sep + "theme.json"), DefaultTheme::Name,
        DirEntry::isDirectory(themes + sep + DefaultTheme::Name));
    if (target.empty())
        return;
    PLOG_INFO << "Theme " << theme << " has no 4:3 layout - switching to " << target;
    theme = target;
    cfg_.save();
    gui_->loadAssets(true);
    extensionRequests_.message = _("Theme switched to the default (CRT 4:3)"); // the launcher's notification line
}

//*******************************
// AutoBleem::saveCarouselSession / restoreCarouselSession
//*******************************
// The launcher leaves to be started over (a display change or "Restart launcher": rc/boot.sh on the console, the
// session script on a Pi / PC stick) - its carousel place goes to a small file in the runtime dir and the next
// start puts it back into the Session, so GuiLauncher::loadAssets() opens on the same set and game (BUG-40).
// Not the after-game state: session_.resumingGui is untouched, so no resume point is looked for (BUG-39).
void AutoBleem::saveCarouselSession() {
    if (CarouselSession::save(CarouselSession::file(), session_.launcher))
        PLOG_INFO << "Carousel place saved for the next start (set " << static_cast<int>(session_.launcher.set)
                  << ", game " << session_.launcher.gameIndex << ")";
    else
        PLOG_WARNING << "Could not save the carousel place";
}

void AutoBleem::restoreCarouselSession() {
    if (CarouselSession::take(CarouselSession::file(), session_.launcher))
        PLOG_INFO << "Carousel place restored (set " << static_cast<int>(session_.launcher.set) << ", game "
                  << session_.launcher.gameIndex << ")";
}

//*******************************
// AutoBleem::launchGame
//*******************************
void AutoBleem::launchGame() {
    PLOG_INFO << "Starting game";
    const bool retroArch = (session_.runningGame && session_.runningGame->foreign) || session_.emuMode != EmuMode::Pcsx;
    const PsGame *running = session_.runningGame.get();
    const char *picture =
        waitingPictureName(running && running->foreign, running && running->app, session_.emuMode != EmuMode::Pcsx);
    runOutside(retroArch, picture, [this]() {
        launcher_.launch(session_.runningGame, session_.emuMode, session_.resumePoint, session_.package.get());
    });

    bool reloadFavHist{false};
    if (session_.runningGame->foreign)
        reloadFavHist = true;
    else if (session_.emuMode != EmuMode::Pcsx)
        reloadFavHist = true;

    if (reloadFavHist) {
        retroArch_.reloadFavoritesAndHistory(); // they could have changed
    }

    takeAppMessage();

    session_.runningGame.reset(); // replace with shared_ptr pointing to nullptr
    session_.package.reset();
    session_.startingGame = false;
}

//*******************************
// AutoBleem::takeAppMessage
//*******************************
// An App's script that refuses to run (rc/pe_run.sh on a launcher the compat list blocks) cannot draw anything
// itself: it leaves <runtime>/app-message.txt - line 1 the App's title, line 2 the reason - and the launcher
// says it on its notification line when it is back. The file is taken (removed) either way.
void AutoBleem::takeAppMessage() {
    const string path = Env::getPathToRuntimeDir() + sep + "app-message.txt";
    if (!DirEntry::exists(path))
        return;
    string contents;
    DirEntry::readFile(path, contents);
    vector<string> lines = Strings::getTokens(
        contents, '\n'); // empty tokens are dropped: the reason is line 2 only if the title is line 1
    remove(path.c_str());
    if (lines.empty() || Strings::trim(lines[0]).empty())
        return;
    string text = Strings::trim(lines[0]) + " " + _("cannot run on this console");
    if (lines.size() > 1 && !Strings::trim(lines[1]).empty())
        text += " - " + Strings::trim(lines[1]);
    extensionRequests_.message = text;
}

//*******************************
// AutoBleem::runRetroArchMenu
//*******************************
// The system menu's RetroArch item where there is no rc/retroarch.sh to leave to (the Windows product):
// RetroArch's own menu in front of the launcher, and its playlists re-read after - the user may have
// scanned content in there.
void AutoBleem::runRetroArchMenu() {
    PLOG_INFO << "Starting RetroArch's menu";
    runOutside(true, "retroarch.jpg", [this]() { launcher_.launchRetroArchMenu(); });
    retroArch_.reloadPlaylists();
    retroArch_.reloadFavoritesAndHistory();
}

//*******************************
// AutoBleem::run
// the whole program from here on. main() only wraps this so a stray exception is logged instead of a silent abort.
//*******************************
int AutoBleem::run() {
    ableem::StartupTimer::milestone("run-entered");
    {
        ableem::StartupTimer timer("open-library");
        if (!openLibrary()) {
            return EXIT_FAILURE;
        }
    }
    string pathToGamesDir = Env::getPathToGamesDir();

    restoreCarouselSession(); // a display change / restart left the carousel's place: the launcher opens on it

    // the safe area (overscan): the 4:3 frame goes into a centred rectangle of the output, the margin from config.ini -
    // the tube's on the 720x480 CRT mode, the VGA one (0 unless set) on any other 4:3 size. Before display(): its boot
    // splash is the first 4:3 frame, and it would be shown with the renderer's default margin instead of config.ini's
    // (CRT 4:3 round 2: Crtmargin=0 still drew the 5 % margin), and before the keep-mode question, which is drawn in
    // it too
    // RetroArch's core info and playlists are read on a worker while the window, fonts and theme come up; the
    // first screen only waits for them when it shows a RetroArch set (GuiLauncher::loadAssets)
    if (Env::retroArchInstalled())
        retroArch_.startBackgroundLoad();
    applySafeMargin();
    {
        ableem::StartupTimer timer("display-and-theme"); // window, fonts, the theme's assets
        gui_->display(false);
    }
    ableem::StartupTimer::milestone("splash-shown");
    unlink("/tmp/.abload"); // the console's wake-up picture (rc/selection.sh's standby) waits for this

    // what used to stand between the process and the splash (a black screen of seconds on the Pi 400): the memory
    // card restore and the scan triggers run now, with the splash up. Both finish before anything can start a game
    // or a scan - the flags are read by the scan trigger below.
    {
        ableem::StartupTimer timer("memcard-restore");
        MemcardManager memcardOperation(pathToGamesDir);
        memcardOperation.restoreAll(Env::getPathToSaveStatesDir());
    }

    // the same triggers the classic menu's forceScan prompt used to check, minus autobleem.prev (a
    // GamesFingerprint now stands in for it - see ScanService); moving loose game files into their own
    // sub-directories is ScanService's worker's job now, the first thing runScan() does.
    ableem::StartupTimer fingerprintTimer("fingerprint-check");
    bool fingerprintOnDiskMatches = ScanService::fingerprintsMatchDisk();
    // RetroBoot's EmulationStation reads this list; without RetroBoot nobody does, and its absence must not
    // cost a full scan on every boot (it did, on the Pi)
    bool gamelistXmlExists =
        !Env::hasRetroBoot() ||
        DirEntry::exists(Env::getPathToRetroarchDir() + sep +
                         "retroboot/emulationstation/.emulationstation/gamelists/psx/gamelist.xml");
    bool thereAreRawGameFilesInGamesDir = GameScanner::hasLooseGameFiles(pathToGamesDir);
    fingerprintTimer.stop();

    // Options -> Display on the console: rc/boot.sh has just restarted Weston in the mode to try - kept, or the
    // launcher leaves again at once for the old one. Elsewhere the mode is tried in-process, nothing is pending.
#ifdef AB_PLATFORM_PSC
    const bool leaveForDisplay = !confirmPendingOutputMode();
#else
    DirEntry::removeFile(OutputMode::pendingFile());
    const bool leaveForDisplay = false;
#endif
    // a 4:3 mode config.ini already holds (a theme installed or chosen since): no confirm - it is in use. "auto" is
    // the window the display gave (a 4:3 monitor on a Pi or a PC stick)
    if (!leaveForDisplay) {
        OutputMode inUse = OutputMode::parse(cfg_.inifile.values[OutputMode::ConfigKey]);
        if (inUse.isAuto()) {
            const ableem::Size window = gui_->platform().windowSize();
            inUse.w = window.w;
            inUse.h = window.h;
        }
        useDefaultThemeFor(inUse);
    }

    // the files only: the databases themselves are still being read on the library's worker, and a stick that
    // has the files but broken ones is told in the log (covers-dbs-open) rather than stopped for
    if (!ableem::MetadataLookup::sourcesPresent(Env::getPathToCoversDBDir(), Env::getPathToPlayStationRdbFile())) {
        // was ClassicMenuScreen::init()'s check; still worth stopping for before anything else runs, since
        // every game would otherwise scan in with no title/cover. RetroArch's "Sony - PlayStation.rdb"
        // is the other source, so a stick with that tree but no covers*.db is fine. After display(): the
        // theme's font and background are loaded there, and the message drawn before it was a black screen.
        gui_->criticalException(_("WARNING: NO COVER DB FOUND. PRESS ANY BUTTON."));
    }

    applyOnlineSetting();
#ifdef AB_ONLINE_UPDATE
    applyUpdateSetting();
#ifdef AB_PLATFORM_WIN
    // the installer the last update downloaded: this build is what it installed, so it and pending.json
    // go (the Pi's autobleem-update helper clears the folder itself)
    {
        const string updatesDir = Env::getPathToSystemDir() + sep + "Updates";
        ableem::PendingUpdate pending;
        if (pending.load(updatesDir + sep + "pending.json") &&
            pending.autobleemVersion == string(Version::VERSION) + "-" + Version::GIT_HASH) {
            PLOG_INFO << "Update " << pending.autobleemVersion << " is what runs now - removing its installer";
            DirEntry::removeFile(updatesDir + sep + pending.autobleemFile);
            DirEntry::removeFile(updatesDir + sep + "pending.json");
        }
    }
#endif
    if (updates().checkDue(time(nullptr)))
        updates().startCheck(time(nullptr)); // once a day, and at every start - the launcher asks when it lands
#endif
    ableem::StartupTimer scansTimer("scans-start");
    scans().start();
    scansTimer.stop();
    if (!fingerprintOnDiskMatches || !gamelistXmlExists || thereAreRawGameFilesInGamesDir) {
        scans().requestScan();
    }

    // the extensions (docs/extensions-plan.md): what is in Extensions/, the crash guard's verdict on the last
    // run - an extension that was running when the launcher died is disabled and the user told - and the
    // background ones started
    ableem::StartupTimer extensionsTimer("extensions-start");
    extensionCatalog_.scan();
    const string crashed = extensionCatalog_.takeCrashed();
    if (!crashed.empty()) {
        const ExtensionInfo *info = extensionCatalog_.find(crashed);
        extensionRequests_.message = (info ? info->title : crashed) + " " + _("stopped AutoBleem and was disabled");
    }
    extensions_.startBackground();
    extensionsTimer.stop();

    // On the console a Quit event is never a window's close button: it is SDL giving up on the display -
    // seen on the PSC on 2026-09-20 coming back from RetroArch 1.22.2 (Doom): the GPU had not returned the
    // emulator's memory yet, the launcher's first buffer uploads failed ("PVR: glBufferSubData: No memory
    // for object data"), Weston then dropped the client ("wl_display@1: error 0: invalid object 16") and
    // the Quit that followed took the whole program out through selection.sh's reboot. So the display is
    // rebuilt a few times, a second apart, before that is accepted.
    int displayLost = 0;
    if (leaveForDisplay) {
        session_.menuOption = MENU_OPTION_DISPLAY;
        saveCarouselSession(); // leaving again for the old mode: the place restored above goes on to the next start
        launcher_.writeSelectionScript();
    }
    while (!leaveForDisplay) {
        bool quitRequested = false;
        {
            GuiLauncher launcherScreen(*gui_);
            launcherScreen.show();
            quitRequested = launcherScreen.quitRequested;
        }
        if (session_.menuOption == MENU_OPTION_POWEROFF) {
            launcher_.writeSelectionScript(); // the console's rc/selection.sh does the standby
            break;
        }
        if (quitRequested) {
            // Input's quit request (SIGTERM/SIGINT, the DebugDriver's `quit`) is a leave, never a lost
            // display, and no selection is written for it (rc/selection.sh finds none: a stop, not a choice)
            if (gui_->platform().isDevHost() || gui_->input().quitRequested() || ++displayLost > 3) {
                break; // the window's own close button - see GuiLauncher::loop()'s comment - or hopeless
            }
            PLOG_WARNING << "The display went away (attempt " << displayLost << " of 3) - rebuilding it";
            gui_->releaseDisplay();
            usleep(1000 * 1000);
            gui_->input().flushEvents();
            gui_->display(true);
            // not resumingGui: no game just ended, so the launcher must not look for its resume point (BUG-39)
            continue;
        }
        displayLost = 0;

        session_.resumingGui = false;

        // the launcher asks for a game with session().startingGame + runningGame (three places in
        // evoui_launcher_actions/input.cpp); turning that into MENU_OPTION_START used to be the classic
        // menu screen's job, and went missing with it - nothing launched anywhere until this line
        if (session_.startingGame && session_.runningGame) {
            session_.menuOption = MENU_OPTION_START;
        }

        // Options -> Display changed: the console leaves for rc/boot.sh to restart Weston in the new mode (the
        // pending file says which) and start the launcher again; elsewhere the window is remade here. With no mode
        // to try it is the Quick menu's "Restart launcher": the same leave, and the session loop (boot.sh on the
        // console, autobleem-session on a Pi / PC stick) starts the launcher over.
        if (session_.menuOption == MENU_OPTION_DISPLAY) {
            const bool restartOnly = session_.pendingOutputMode.empty();
#ifdef AB_PLATFORM_PSC
            if (!restartOnly) {
                DirEntry::createDirs(Env::getPathToRuntimeDir());
                DirEntry::writeFileIfChanged(OutputMode::pendingFile(), session_.pendingOutputMode + "\n");
            }
            saveCarouselSession();
            launcher_.writeSelectionScript();
            break;
#else
            if (restartOnly) {
                saveCarouselSession();
                launcher_.writeSelectionScript();
                break;
            }
            tryOutputMode(session_.pendingOutputMode);
            session_.pendingOutputMode.clear();
            session_.menuOption = MENU_OPTION_IDLE;
            // the launcher comes back on the same game, but not as after a game: resumingGui stays false, or
            // it would check the last game's resume point and call the run a crash (BUG-39)
            continue;
#endif
        }

        // the console leaves for rc/retroarch.sh to run RetroArch: not when the loader would refuse it
        if (session_.menuOption == MENU_OPTION_RETRO && !Env::directLaunch() && retroArchRefused()) {
            session_.menuOption = MENU_OPTION_IDLE;
            continue;
        }

        // for the rc scripts, when the process is about to leave - never for a game, which comes back here:
        // a selection left over from one would hide a later crash from them (autobleem-main's
        // docs/archive/quiet-stick-plan.md)
        if (session_.menuOption != MENU_OPTION_START)
            launcher_.writeSelectionScript();

        if (session_.menuOption == MENU_OPTION_START) {
            scans().setWatching(false);           // the emulator gets the CPU, not the scanner
            scans().setProcessorsSuspended(true); // nor the stick: a processor rewriting a disc image stops
            launchGame();
            scans().setProcessorsSuspended(false);
            scans().setWatching(true);
            session_.menuOption = MENU_OPTION_IDLE;
            continue;
        }

        // the launcher closed asking to exit to RetroArch/EmulationStation (the system menu's item): on a
        // desktop that is RetroArch run in front of us and the launcher again after; on the console and the
        // Pi the process leaves and rc/retroarch.sh takes over
        if (session_.menuOption == MENU_OPTION_RETRO && Env::directLaunch()) {
            scans().setWatching(false);
            scans().setProcessorsSuspended(true);
            runRetroArchMenu();
            scans().setProcessorsSuspended(false);
            scans().setWatching(true);
            session_.menuOption = MENU_OPTION_IDLE;
            continue;
        }
        // Circle alone in the launcher is a no-op - there is nothing else to show - so any other return
        // from show() is unexpected and the safest thing is to just show it again
        if (session_.menuOption == MENU_OPTION_RETRO || session_.menuOption == MENU_OPTION_UPDATE) {
            break;
        }
    }

    PLOG_INFO << "Quit: leaving the main loop with menuOption " << session_.menuOption
              << (gui_->input().quitRequested() ? " (Input quit requested)" : "")
              << (leaveForDisplay ? " (display change)" : "");

    // the extensions go before the services they may reach
    extensions_.shutdown();
    scans().stop();

    // close the databases before the gui goes away.
    gameLibrary.close();

    if (session_.menuOption == MENU_OPTION_POWEROFF) {
        gui_->drawText(_("POWERING OFF... PLEASE WAIT")); // the standby follows within a second
    } else {
        Gui::splash(_("Loading ... Please Wait ..."));
    }
    gui_->finish();

    return EXIT_SUCCESS;
}
