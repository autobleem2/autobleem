//
// ScanService: scans the Games directory on a background thread, applying every regional.db write on the
// main thread as it polls the worker's results. CLAUDE.md ("Straight into EvolutionUI, with the scan in the
// background") is the design note.
//
#pragma once

#include "../model/ps_game.h"
#include "online_assets.h"

#include <ableem/engine/retroarch_scanner.h>

#include <ableem/engine/game_library.h>
#include <ableem/engine/game_scanner.h>
#include <ableem/engine/games_fingerprint.h>
#include <ableem/engine/games_hierarchy.h>
#include <ableem/engine/usb_game.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class RetroArchService;

//******************
// ScanUpdate
//******************
// What changed since the last poll() - the launcher applies each field to the carousel and the status
// line. Every field sits at its default when nothing of that kind happened this poll.
struct ScanUpdate {
    bool active = false; // a scan is running (set true by ScanStarted, false again once Finished is applied)

    // the latest progress report, untranslated - the launcher builds the status line's text from these.
    // progressed says whether one arrived this poll at all: stage/detail/done/total all sit at harmless
    // defaults otherwise, indistinguishable from a genuine "just started scanning" report without this.
    bool progressed = false;
    ableem::ScanStage stage = ableem::ScanStage::Scanning;
    std::string detail;
    int done = 0;
    int total = 0;

    std::vector<int> removedGameIds; // games gone since the last poll: folder deleted, or failed verify()
    PsGames addedGames;              // newly discovered games since the last poll
    PsGames updatedGames;            // existing games whose row was refreshed since the last poll
    std::string lastFailedGamePath;  // non-empty when a game failed verify() since the last poll

    // the ROM pass rewrote RetroArch playlists since the last poll (their file names, "<system>.lpl") -
    // RetroArchService has been told to reload them by the time poll() returns; the launcher re-reads
    // the playlist names and the set it is showing
    std::vector<std::string> playlistsWritten;

    int boxArtFetched = 0; // covers the online pass brought in since the last poll (the launcher reloads them)

    bool finished = false; // a whole scan cycle completed during this poll
    int finishedGameCount = 0;
    int finishedFailedCount = 0;
    int finishedRomCount = 0; // games in the RetroArch ROM folders, every system together (0 without RetroArch)
};

//******************
// ScanService
//******************
// Owns one worker thread that does every bit of scanning filesystem work (fingerprinting, moving loose game
// files, walking the hierarchy, GameScanner::scanGamesDirectory with its own MetadataLookup) and
// hands results back through a mutex-protected queue. It never touches regional.db, Lang, Gui or App - the
// main thread does every database write, from poll(), using the GameLibrary connection App already owns.
//
// requestScan() asks for one scan as soon as the worker is free; when nothing is requested, the worker
// checks the games directory's fingerprint every ScanWatchInterval (core/model/timing.h) and starts a scan
// once it sees the same changed fingerprint twice in a row (checkForChanges() - the debounce, so a copy
// still landing on the USB stick does not trigger a scan mid-copy). setWatching(false) turns that off
// (during a game launch, say) without stopping the thread; requestScan() still works while not watching.
//
// When RetroArch is there (romScanEnabled(): its binary and its ROM folders exist - RetroArch is optional
// on every platform, and without it none of this runs) the same cycle goes on to the ROM folders:
// ableem::RetroArchScanner writes a playlist per system from the file names, the worker reports which
// ones changed (WorkerEvent::Kind::PlaylistsWritten) and poll() has the service reload them. The ROM
// folders have a fingerprint of their own (roms.fingerprint), watched the same way.
//
// The worker runs at the OS's lowest scheduling priority (System::lowerCurrentThreadPriority(), the first
// thing threadMain() does) so a scan - which nothing is waiting on - never takes CPU time away from a
// running emulator or anything else on the system.
//
// Owned by App (App::scans()).
class ScanService {
public:
    // retroArch is told to reload its playlists when the ROM pass rewrote any (nullptr: nobody to tell)
    explicit ScanService(ableem::GameLibrary &library, RetroArchService *retroArch = nullptr)
        : library_(library), retroArch_(retroArch) {}
    ~ScanService();
    ScanService(const ScanService &) = delete;
    ScanService &operator=(const ScanService &) = delete;

    void start(); // spawns the worker thread; a no-op if already running
    void stop();  // signals the worker to stop and joins it; a no-op if not running

    // true if it took (a scan was not already running); a no-op returning false while scanning() is already
    // true - the caller shows "scan already in progress" instead of queuing another
    bool requestScan();
    bool scanning() const { return scanning_.load(); }
    void setWatching(bool watching) { watching_.store(watching); }

    // drains every event the worker has queued since the last call, applying each regional.db write on the
    // way (findGameIdByPath/insertGame/updateGame/replaceDiscs, deleting a game whose folder is gone, the
    // sub-dir rows and autobleem.list once the scan finishes) - call this once a frame, before render().
    ScanUpdate poll();

    // The watcher's debounce core, and the one full scan cycle it runs when due. Both are ordinary methods,
    // not implicitly thread-affine: in production only threadMain()'s loop calls them, but a test may call
    // them directly instead of starting the real thread (start() must not also be called then - they share
    // worker-only state with no locking, same as the real worker loop assumes it owns that state alone).
    bool checkForChanges();
    void runScan();

    // <working>/games.fingerprint and roms.fingerprint - where the fingerprints of the last completed scan
    // are kept. fingerprintsMatchDisk() is the "does the disk match what we last scanned" test, for
    // AutoBleem::run() to apply at startup, before start() has even been called, to decide whether to
    // requestScan() right away.
    static std::string fingerprintFilePath();
    static std::string romsFingerprintFilePath();
    // <state>/roms.scanstate - the ROM scanner's per-folder digests (RetroArchScanner::Options::stateFile),
    // what lets a rescan skip every ROM folder nothing changed in
    static std::string romScanStateFilePath();
    static bool fingerprintsMatchDisk();

    // resources/platform/roms_folders.cfg - the ROM folders not named as their database is ("Arcade")
    static std::string romsFolderAliasesPath();

    // RetroArch is installed and has ROM folders to scan - see the class comment
    static bool romScanEnabled();

    // the online pass (OnlineAssets): the databases bundle before the ROM scan when there are none, the
    // box art of every ROM without one after it. Off until enabled with a command to fetch with; the
    // launcher sets it from config.ini's "online" and Env::downloadCommand() at start and after Options.
    // runner is for the tests (OnlineAssets::CommandRunner), std::system otherwise.
    void setOnline(bool enabled, const OnlineAssets::Config &config,
                   OnlineAssets::CommandRunner runner = OnlineAssets::CommandRunner());

private:
    //******************
    // ScannedGame
    //******************
    // the UsbGame fields ScanService needs, copied out of the worker's UsbGame the moment it verifies - the
    // UsbGame itself belongs to the worker thread's GamesHierarchy and must not be touched from poll().
    struct ScannedGame {
        std::string fullPath;
        std::string saveStatePath;
        std::string title;
        std::string publisher;
        int year = 0;
        int players = 0;
        std::string serial;
        std::string region;
        std::string memcard;
        std::vector<std::string> discNames;
    };

    //******************
    // WorkerEvent
    //******************
    // one thing the worker wants the main thread to know about, tagged by kind; only the fields that kind
    // uses are meaningful, the rest sit at their default. See ScanService::poll().
    struct WorkerEvent {
        enum class Kind {
            ScanStarted,
            Progress,
            GameVerified,
            GameFailedVerify,
            PlaylistsWritten,
            BoxArtFetched,
            Finished
        };
        Kind kind = Kind::Progress;

        std::vector<std::string> currentPaths; // ScanStarted: every game folder this scan found

        ableem::ScanStage stage = ableem::ScanStage::Scanning; // Progress
        std::string detail;
        int done = 0;
        int total = 0;

        ScannedGame game;       // GameVerified
        std::string failedPath; // GameFailedVerify

        std::vector<std::string> playlists; // PlaylistsWritten: the "<system>.lpl" files that changed
        int boxArtFetched = 0;              // BoxArtFetched

        ableem::GamesHierarchy hierarchy; // Finished
        ableem::UsbGames gamesToAddToDB;
        ableem::GamesFingerprint fingerprint;
        ableem::GamesFingerprint romsFingerprint;
        int failedCount = 0;
        int romCount = 0;
    };

    //******************
    // VanishedGame
    //******************
    // a GAME row whose folder the scan did not find where the database says it is. Kept aside from
    // ScanStarted until Finished: a verified game at a *new* path with the same folder name and the same
    // disc file names is that game moved (into or out of a sub-folder of Games/) and takes the row over -
    // id, history and last_played included - instead of becoming a new game; whatever is still unclaimed
    // when the scan finishes is deleted. See claimMovedGame().
    struct VanishedGame {
        int gameId = 0;
        std::string folderName; // the last component of PATH
        std::vector<std::string> discNames;
    };

    class Listener;
    friend class Listener;

    void pushEvent(WorkerEvent event);
    void threadMain();
    void applyVerifiedGame(const ScannedGame &game, ScanUpdate &update);
    // a verified game the database does not know by path: if it is one of the vanished rows moved, that
    // row's PATH is rewritten to the new folder and its id returned (true); false for a genuinely new game
    bool claimMovedGame(const ScannedGame &game, int *id);
    void deleteUnclaimedVanished(ScanUpdate &update);
    // the ROM pass: every playlist a system folder yields, merged over what is there. Returns the game count.
    int scanRetroArchRoms(Listener &listener, std::vector<std::string> &playlistsWritten);
    // the PS1 covers the scan did not find, from libretro's server - where the platform goes online at all
    void fetchMissingPs1BoxArt(Listener &listener, const std::vector<ableem::UsbGamePtr> &games);
    ableem::GameLibrary &library_;
    RetroArchService *retroArch_ = nullptr;

    // what setOnline() gave, read by the worker at the start of each cycle
    std::mutex onlineMutex_;
    bool onlineEnabled_ = false;
    OnlineAssets::Config onlineConfig_;
    OnlineAssets::CommandRunner onlineRunner_;

    std::thread thread_;
    std::atomic<bool> stopping_{false};
    std::atomic<bool> scanRequested_{false};
    std::atomic<bool> watching_{true};
    std::atomic<bool> scanning_{false};

    std::mutex queueMutex_;
    std::vector<WorkerEvent> queue_;

    // main-thread-only (poll()): the rows a running scan has not found yet - see VanishedGame
    std::vector<VanishedGame> vanished_;

    // worker-thread-only state for the watcher's debounce - see checkForChanges()
    ableem::GamesFingerprint lastScannedFingerprint_;
    ableem::GamesFingerprint lastCheckFingerprint_;
    ableem::GamesFingerprint lastScannedRomsFingerprint_;
    ableem::GamesFingerprint lastCheckRomsFingerprint_;
};
