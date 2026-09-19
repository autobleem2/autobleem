//
// OnlineAssets: the box art and the databases the ROM scan can fetch from libretro's servers, when there
// is a network - and there may be none: the console has one only with the AutoBleem kernel and an adapter.
//
#pragma once

#include <ableem/engine/retroarch_scanner.h>

#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <vector>

namespace ableem {
class ScanProgressListener;
}

//******************
// OnlineAssets
//******************
// The app has no HTTP client and is not getting one for this: a fetch is an external command the platform
// ini names (Config::downloadCommand - `curl -sfL -m 20 -o "%o" "%u"` on a PC and a Pi, nothing on the
// console until a tool is known to be there), with the URL and the output file put in for %u and %o, run
// through std::system from the scan worker. The command's own timeout flag is the timeout.
//
// Nothing is fetched until probe() has succeeded once for this instance - one cheap request to the
// thumbnails server - so a machine without a network pays a few seconds per scan and notices nothing
// else. A box art the server does not have is remembered (Named_Boxarts/.autobleem-missing.txt, one
// escaped name per line) and not asked for again; delete the file to retry. A fetch that fails after a
// successful probe counts as missing, unless the probe fails again right after - then the network went,
// and the pass stops without marking anything.
//
// Owned by nothing: ScanService's worker makes one per scan cycle from the config it was given.
class OnlineAssets {
public:
    struct Config {
        std::string downloadCommand; // "" = no online fetching on this platform
        std::string thumbnailsBaseUrl = "https://thumbnails.libretro.com";
        std::string databasesUrl = "https://buildbot.libretro.com/assets/frontend/database-rdb.zip";
    };
    // runs a command line, returns its exit status (0 = success); std::system by default, a fake in tests
    using CommandRunner = std::function<int(const std::string &commandLine)>;

    explicit OnlineAssets(const Config &config, CommandRunner runner = CommandRunner());

    bool enabled() const { return !config_.downloadCommand.empty(); }
    // one request against the thumbnails server; the answer is kept until probe(true) asks afresh
    bool probe(bool again = false);
    bool online() const { return online_; }

    // `url` to `outPath` (its directory created as needed); false, and no file left behind, on failure
    bool fetch(const std::string &url, const std::string &outPath);

    // <rdbDir> holds no .rdb: download the databases bundle (a zip of rdb/*.rdb, ~40 MB) and unpack it
    // there. Returns how many .rdb files are there afterwards.
    int ensureDatabases(const std::string &rdbDir);

    enum class BoxArt { Fetched, AlreadyThere, Missing, Failed };
    // <thumbnailsDir>/<dbName>/Named_Boxarts/<escaped label>.png from the server, unless it is there
    // already or was found missing before
    BoxArt fetchBoxArt(const std::string &thumbnailsDir, const std::string &dbName, const std::string &label);

    // the box art of every game in `games` without one on disk (this instance's own ThumbnailLookup says,
    // fuzzy fallback included), reported as ScanStage::FetchingBoxArt per game; the pass ends when the
    // network goes or shouldStop() says so. Returns how many covers arrived; `missing` counts the
    // server's misses.
    int fetchMissingBoxArt(const std::vector<ableem::RetroArchScanResult::Game> &games,
                           const std::string &thumbnailsDir, ableem::ScanProgressListener *listener,
                           const std::function<bool()> &shouldStop, int *missing = nullptr);

    static std::string boxArtUrl(const std::string &baseUrl, const std::string &dbName, const std::string &label);
    static std::string boxArtPath(const std::string &thumbnailsDir, const std::string &dbName,
                                  const std::string &label);
    static std::string urlEncode(const std::string &s); // RFC 3986: everything but unreserved as %XX
    static std::string missingListPath(const std::string &thumbnailsDir, const std::string &dbName);

    // the escaped names in a Named_Boxarts/.autobleem-missing.txt, and writing one back
    static std::set<std::string> loadMissingList(const std::string &path);
    static bool saveMissingList(const std::string &path, const std::set<std::string> &names);

private:
    std::string commandFor(const std::string &url, const std::string &outPath) const;

    Config config_;
    CommandRunner runner_;
    bool probed_ = false;
    bool online_ = false;
};
