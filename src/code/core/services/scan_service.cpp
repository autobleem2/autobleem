#include "scan_service.h"
#include "environment.h"
#include "retroarch.h"
#include "system.h"
#include "../main.h"
#include "../model/timing.h"

#include <ableem/engine/retroarch_cores.h>
#include <ableem/engine/retroarch_scanner.h>

#include <chrono>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <ableem/engine/log.h>

using namespace std;

//******************
// ScanService::Listener
//******************
// The ScanProgressListener the worker hands to GameScanner: every callback just packages a WorkerEvent and
// pushes it, on whatever thread the scan is running on (the worker's, always - see runScan()).
class ScanService::Listener : public ScanProgressListener {
public:
    explicit Listener(ScanService *owner) : owner_(owner) {}

    void onScanProgress(ScanStage stage, const string &detail, int done, int total) override {
        WorkerEvent event;
        event.kind = WorkerEvent::Kind::Progress;
        event.stage = stage;
        event.detail = detail;
        event.done = done;
        event.total = total;
        owner_->pushEvent(std::move(event));
    }

    void onGameVerified(const UsbGame &game) override {
        WorkerEvent event;
        event.kind = WorkerEvent::Kind::GameVerified;
        event.game.fullPath = game.fullPath;
        event.game.saveStatePath = game.saveStatePath;
        event.game.title = game.title;
        event.game.publisher = game.publisher;
        event.game.year = game.year;
        event.game.players = game.players;
        event.game.serial = game.serial;
        event.game.region = game.region;
        event.game.memcard = game.memcard;
        for (const Disc &disc : game.discs)
            event.game.discNames.push_back(disc.diskName);
        owner_->pushEvent(std::move(event));
    }

    void onGameFailedVerify(const string &fullPath) override {
        failedCount++;
        WorkerEvent event;
        event.kind = WorkerEvent::Kind::GameFailedVerify;
        event.failedPath = fullPath;
        owner_->pushEvent(std::move(event));
    }

    int failedCount = 0;

private:
    ScanService *owner_;
};

//*******************************
// ScanService::~ScanService
//*******************************
ScanService::~ScanService() {
    stop();
}

//*******************************
// ScanService::fingerprintFilePath
//*******************************
string ScanService::fingerprintFilePath() {
    return Env::getWorkingPath() + sep + "games.fingerprint";
}

//*******************************
// ScanService::romsFingerprintFilePath
//*******************************
string ScanService::romsFingerprintFilePath() {
    return Env::getWorkingPath() + sep + "roms.fingerprint";
}

//*******************************
// ScanService::romScanEnabled
//*******************************
bool ScanService::romScanEnabled() {
    return Env::retroArchInstalled() && DirEntry::isDirectory(Env::getPathToRetroarchRomsDir());
}

//*******************************
// ScanService::fingerprintsMatchDisk
//*******************************
bool ScanService::fingerprintsMatchDisk() {
    GamesFingerprint stored;
    if (!stored.load(fingerprintFilePath()) || stored != GamesFingerprint::take(Env::getPathToGamesDir()))
        return false;
    if (!romScanEnabled())
        return true;
    GamesFingerprint storedRoms;
    return storedRoms.load(romsFingerprintFilePath()) &&
           storedRoms == GamesFingerprint::takeAllFiles(Env::getPathToRetroarchRomsDir());
}

//*******************************
// ScanService::start
//*******************************
void ScanService::start() {
    if (thread_.joinable())
        return; // already running
    stopping_.store(false);
    thread_ = thread(&ScanService::threadMain, this);
}

//*******************************
// ScanService::stop
//*******************************
void ScanService::stop() {
    if (!thread_.joinable())
        return;
    stopping_.store(true);
    thread_.join();
    stopping_.store(false);
}

//*******************************
// ScanService::requestScan
//*******************************
bool ScanService::requestScan() {
    if (scanning_.load())
        return false;
    scanRequested_.store(true);
    return true;
}

//*******************************
// ScanService::pushEvent
//*******************************
void ScanService::pushEvent(WorkerEvent event) {
    lock_guard<mutex> lock(queueMutex_);
    queue_.push_back(std::move(event));
}

//*******************************
// ScanService::threadMain
//*******************************
void ScanService::threadMain() {
    // this whole thread only ever does filesystem/database-adjacent work no one is waiting on; it must never
    // take CPU time away from a running emulator (or anything else on the system)
    System::lowerCurrentThreadPriority();

    lastScannedFingerprint_.load(fingerprintFilePath()); // false (left empty) if nothing was ever scanned
    lastCheckFingerprint_ = lastScannedFingerprint_;
    lastScannedRomsFingerprint_.load(romsFingerprintFilePath());
    lastCheckRomsFingerprint_ = lastScannedRomsFingerprint_;

    auto lastWatchCheck = chrono::steady_clock::now() - chrono::milliseconds(ScanWatchInterval);
    while (!stopping_.load()) {
        bool shouldScan = scanRequested_.exchange(false);

        if (!shouldScan && watching_.load()) {
            auto now = chrono::steady_clock::now();
            if (now - lastWatchCheck >= chrono::milliseconds(ScanWatchInterval)) {
                lastWatchCheck = now;
                shouldScan = checkForChanges();
            }
        }

        if (shouldScan) {
            runScan();
            lastWatchCheck = chrono::steady_clock::now(); // don't immediately re-check right after scanning
        } else {
            this_thread::sleep_for(chrono::milliseconds(250));
        }
    }
}

//*******************************
// ScanService::checkForChanges
//*******************************
bool ScanService::checkForChanges() {
    GamesFingerprint fresh = GamesFingerprint::take(Env::getPathToGamesDir());
    bool changedFromScanned = !(fresh == lastScannedFingerprint_);
    bool stableSinceLastCheck = (fresh == lastCheckFingerprint_);
    lastCheckFingerprint_ = fresh;

    // the ROM folders, the same way; a change in either tree runs the whole cycle, once both are still
    if (romScanEnabled()) {
        GamesFingerprint freshRoms = GamesFingerprint::takeAllFiles(Env::getPathToRetroarchRomsDir());
        bool romsChanged = !(freshRoms == lastScannedRomsFingerprint_);
        bool romsStable = (freshRoms == lastCheckRomsFingerprint_);
        lastCheckRomsFingerprint_ = freshRoms;
        if (!stableSinceLastCheck || !romsStable)
            return false;
        return changedFromScanned || romsChanged;
    }
    return changedFromScanned && stableSinceLastCheck;
}

//*******************************
// ScanService::scanRetroArchRoms
//*******************************
// The worker's own CoreInfoTable, not RetroArchService's: the service belongs to the main thread, and the
// .info files are a few hundred small reads. The system table is what the service would answer - the same
// .info mapping under the same platform cores.cfg.
int ScanService::scanRetroArchRoms(Listener &listener, vector<string> &playlistsWritten) {
    ableem::CoreInfoTable cores;
    cores.load(Env::getPathToRetroarchDir(), RetroArchService::coresCfgPath());

    ableem::RetroArchScanner::Options options;
    options.romsDir = Env::getPathToRetroarchRomsDir();
    options.playlistsDir = Env::getPathToRetroarchPlaylistsDir();
    ableem::RetroArchScanner scanner(&listener);
    ableem::RetroArchScanResult result = scanner.scan(options, ableem::RetroArchScanner::systemsFrom(cores));
    playlistsWritten = result.playlistsWritten;
    PLOG_INFO << "RetroArch ROM scan: " << result.systemsScanned << " systems, " << result.gamesFound << " games, "
              << result.playlistsWritten.size() << " playlists written, " << result.unknownFolders.size()
              << " folders with no core";
    return result.gamesFound;
}

//*******************************
// ScanService::runScan
//*******************************
void ScanService::runScan() {
    scanning_.store(true);

    string gamesDir = Env::getPathToGamesDir();
    Listener listener(this);

    if (GameScanner::hasLooseGameFiles(gamesDir)) {
        GameScanner mover(&listener);
        mover.moveLooseGameFilesIntoSubDirs(gamesDir);
    }
    {
        // "(Disc n)" sibling folders become one folder before the tree is read, so the scan below only
        // ever sees the merged game - and the fingerprint taken after the scan is of the merged tree, so
        // the watcher does not fire on the merge's own moves
        GameScanner merger(&listener);
        merger.mergeMultiDiscFolders(gamesDir);
    }

    GamesHierarchy hierarchy;
    hierarchy.getHierarchy(gamesDir);

    WorkerEvent started;
    started.kind = WorkerEvent::Kind::ScanStarted;
    for (const UsbGamePtr &game : hierarchy.getAllGames())
        started.currentPaths.push_back(game->fullPath);
    pushEvent(std::move(started));

    // this thread's own sqlite connection and rdb - never regional.db
    MetadataLookup metadata(Env::getPathToCoversDBDir(), Env::getPathToPlayStationRdbFile());
    GameScanner scanner(&listener);
    scanner.scanGamesDirectory(hierarchy, metadata);

    GamesFingerprint fp = GamesFingerprint::take(gamesDir);
    lastScannedFingerprint_ = fp;
    lastCheckFingerprint_ = fp;

    // the other systems' ROMs, when RetroArch is there to play them
    int romCount = 0;
    GamesFingerprint romsFp;
    if (romScanEnabled()) {
        vector<string> playlistsWritten;
        romCount = scanRetroArchRoms(listener, playlistsWritten);
        if (!playlistsWritten.empty()) {
            WorkerEvent written;
            written.kind = WorkerEvent::Kind::PlaylistsWritten;
            written.playlists = playlistsWritten;
            pushEvent(std::move(written));
        }
        romsFp = GamesFingerprint::takeAllFiles(Env::getPathToRetroarchRomsDir());
    }
    lastScannedRomsFingerprint_ = romsFp;
    lastCheckRomsFingerprint_ = romsFp;

    WorkerEvent finished;
    finished.kind = WorkerEvent::Kind::Finished;
    finished.hierarchy = std::move(hierarchy);
    finished.gamesToAddToDB = scanner.gamesToAddToDB;
    finished.fingerprint = fp;
    finished.romsFingerprint = romsFp;
    finished.failedCount = listener.failedCount;
    finished.romCount = romCount;
    pushEvent(std::move(finished));

    scanning_.store(false);
}

//*******************************
// ScanService::applyVerifiedGame
//*******************************
void ScanService::applyVerifiedGame(const ScannedGame &game, ScanUpdate &update) {
    GameDatabase &db = library_.usbGames();

    // the PATH column always carries a trailing separator (insertGame below adds it - "path + sep" is a
    // no-op when one is already there), but a UsbGame's fullPath never does; every lookup by path has to
    // add it back to match what is actually stored
    int id = 0;
    bool existed = db.findGameIdByPath(game.fullPath + sep, &id);
    if (!existed) {
        id = db.maxGameId() + 1;
        db.insertGame(id, game.title, game.publisher, game.players, game.year, game.fullPath + sep,
                      game.saveStatePath + sep, game.memcard);
    } else {
        db.updateGame(id, game.title, game.publisher, game.players, game.year, game.saveStatePath + sep, game.memcard);
    }
    db.replaceDiscs(id, game.discNames);

    GameRecord record;
    record.gameId = id;
    if (!db.reloadUsbGame(record)) {
        PLOG_WARNING << "ScanService: could not reload game id " << id << " (" << game.fullPath << ") after writing it";
        return;
    }

    PsGamePtr psGame = std::make_shared<PsGame>();
    static_cast<GameRecord &>(*psGame) = record;
    if (existed)
        update.updatedGames.push_back(psGame);
    else
        update.addedGames.push_back(psGame);
}

//*******************************
// ScanService::poll
//*******************************
ScanUpdate ScanService::poll() {
    ScanUpdate update;

    vector<WorkerEvent> events;
    {
        lock_guard<mutex> lock(queueMutex_);
        events.swap(queue_);
    }

    for (WorkerEvent &event : events) {
        switch (event.kind) {
        case WorkerEvent::Kind::ScanStarted: {
            update.active = true;

            // currentPaths are bare UsbGame::fullPath values (no trailing separator); PATH always has
            // one (see applyVerifiedGame) - add it back so the comparison below means what it looks like
            set<string> current;
            for (const string &path : event.currentPaths)
                current.insert(path + sep);

            for (const GamePath &row : library_.usbGames().loadGamePaths()) {
                if (current.find(row.path) == current.end()) {
                    if (library_.usbGames().deleteGame(row.gameId))
                        update.removedGameIds.push_back(row.gameId);
                }
            }
            break;
        }

        case WorkerEvent::Kind::Progress:
            update.progressed = true;
            update.stage = event.stage;
            update.detail = event.detail;
            update.done = event.done;
            update.total = event.total;
            break;

        case WorkerEvent::Kind::GameVerified:
            applyVerifiedGame(event.game, update);
            break;

        case WorkerEvent::Kind::GameFailedVerify: {
            int id = 0;
            if (library_.usbGames().findGameIdByPath(event.failedPath + sep, &id)) {
                if (library_.usbGames().deleteGame(id))
                    update.removedGameIds.push_back(id);
            }
            update.lastFailedGamePath = event.failedPath;
            break;
        }

        case WorkerEvent::Kind::PlaylistsWritten:
            if (retroArch_)
                retroArch_->reloadPlaylists();
            update.playlistsWritten.insert(update.playlistsWritten.end(), event.playlists.begin(),
                                           event.playlists.end());
            break;

        case WorkerEvent::Kind::Finished: {
            // writeSubDirRows/writeAutobleemList look games up by UsbGame::fullPath (no trailing
            // separator) - strip the one loadGamePaths() rows always carry so the keys match
            map<string, int> idByPath;
            for (const GamePath &row : library_.usbGames().loadGamePaths())
                idByPath[DirEntry::removeSeparatorFromEndOfPath(row.path)] = row.gameId;

            GameScanner::writeSubDirRows(event.hierarchy, library_.usbGames(), idByPath);
            GameScanner::writeAutobleemList(event.gamesToAddToDB, idByPath);
            library_.writeEmulationStationGamelist();
            library_.exportToRetroArchPlaylist();
            event.fingerprint.save(fingerprintFilePath());
            event.romsFingerprint.save(romsFingerprintFilePath());

            update.active = false;
            update.finished = true;
            update.finishedGameCount = static_cast<int>(event.gamesToAddToDB.size());
            update.finishedFailedCount = event.failedCount;
            update.finishedRomCount = event.romCount;
            break;
        }
        }
    }

    return update;
}
