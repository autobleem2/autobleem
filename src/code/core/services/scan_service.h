//
// ScanService: scans the Games directory on a background thread, applying every regional.db write on the
// main thread as it polls the worker's results. See docs/refactor-plan.md's background-scan section.
//
#pragma once

#include "../model/ps_game.h"

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

//******************
// ScanUpdate
//******************
// What changed since the last poll() - the launcher applies each field to the carousel and the status
// line. Every field sits at its default when nothing of that kind happened this poll.
struct ScanUpdate {
    bool active = false;   // a scan is running (set true by ScanStarted, false again once Finished is applied)

    // the latest progress report, untranslated - the launcher builds the status line's text from these
    ableem::ScanStage stage = ableem::ScanStage::Scanning;
    std::string detail;
    int done = 0;
    int total = 0;

    std::vector<int> removedGameIds;   // games gone since the last poll: folder deleted, or failed verify()
    PsGames addedGames;                // newly discovered games since the last poll
    PsGames updatedGames;              // existing games whose row was refreshed since the last poll
    std::string lastFailedGamePath;    // non-empty when a game failed verify() since the last poll

    bool finished = false;   // a whole scan cycle completed during this poll
    int finishedGameCount = 0;
    int finishedFailedCount = 0;
};

//******************
// ScanService
//******************
// Owns one worker thread that does every bit of scanning filesystem work (fingerprinting, moving loose game
// files, walking the hierarchy, GameScanner::scanGamesDirectory with its own CoverDatabase connection) and
// hands results back through a mutex-protected queue. It never touches regional.db, Lang, Gui or App - the
// main thread does every database write, from poll(), using the GameLibrary connection App already owns.
//
// requestScan() asks for one scan as soon as the worker is free; when nothing is requested, the worker
// checks the games directory's fingerprint every ScanWatchInterval (core/model/timing.h) and starts a scan
// once it sees the same changed fingerprint twice in a row (checkForChanges() - the debounce, so a copy
// still landing on the USB stick does not trigger a scan mid-copy). setWatching(false) turns that off
// (during a game launch, say) without stopping the thread; requestScan() still works while not watching.
//
// The worker runs at the OS's lowest scheduling priority (System::lowerCurrentThreadPriority(), the first
// thing threadMain() does) so a scan - which nothing is waiting on - never takes CPU time away from a
// running emulator or anything else on the system.
//
// Owned by App (App::scans()).
class ScanService {
public:
    explicit ScanService(ableem::GameLibrary &library) : library_(library) {}
    ~ScanService();
    ScanService(const ScanService &) = delete;
    ScanService &operator=(const ScanService &) = delete;

    void start();   // spawns the worker thread; a no-op if already running
    void stop();    // signals the worker to stop and joins it; a no-op if not running

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
        enum class Kind { ScanStarted, Progress, GameVerified, GameFailedVerify, Finished };
        Kind kind = Kind::Progress;

        std::vector<std::string> currentPaths;   // ScanStarted: every game folder this scan found

        ableem::ScanStage stage = ableem::ScanStage::Scanning;   // Progress
        std::string detail;
        int done = 0;
        int total = 0;

        ScannedGame game;          // GameVerified
        std::string failedPath;    // GameFailedVerify

        ableem::GamesHierarchy hierarchy;   // Finished
        ableem::UsbGames gamesToAddToDB;
        ableem::GamesFingerprint fingerprint;
        int failedCount = 0;
    };

    class Listener;
    friend class Listener;

    void pushEvent(WorkerEvent event);
    void threadMain();
    void applyVerifiedGame(const ScannedGame &game, ScanUpdate &update);
    static std::string fingerprintFilePath();

    ableem::GameLibrary &library_;

    std::thread thread_;
    std::atomic<bool> stopping_{false};
    std::atomic<bool> scanRequested_{false};
    std::atomic<bool> watching_{true};
    std::atomic<bool> scanning_{false};

    std::mutex queueMutex_;
    std::vector<WorkerEvent> queue_;

    // worker-thread-only state for the watcher's debounce - see checkForChanges()
    ableem::GamesFingerprint lastScannedFingerprint_;
    ableem::GamesFingerprint lastCheckFingerprint_;
};
