//
// OnlineAssets: the box art and the databases the ROM scan can fetch from libretro's servers, when there
// is a network - and there may be none: the console has one only with the AutoBleem kernel and an adapter.
//
#pragma once

#include <ableem/engine/retroarch_scanner.h>
#include <ableem/engine/usb_game.h>

#include <cstdint>
#include <functional>
#include <map>
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
// else. A box art is asked for by its exact name first; when the server has no such file, the database's
// Named_Boxarts/ index (the server lists its folders) is fetched once per database and the file is picked
// from it with ThumbnailLookup::pickName - the same tag-peeling, case-insensitive, region-tolerant rules
// the carousel finds a local file with, since libretro's thumbnails follow newer No-Intro spellings than
// the rdb ("Sonic The Hedgehog" for the rdb's "Sonic the Hedgehog") - and saved under the server's name.
// A box art the server has under no matching name is remembered (Named_Boxarts/.autobleem-missing.txt,
// one escaped name per line) and not asked for again; delete the file to retry. A fetch that fails after
// a successful probe counts as missing, unless the probe fails again right after - then the network went,
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
    // the label's box art into <thumbnailsDir>/<dbName>/Named_Boxarts/ from the server - by the exact name,
    // else by the best name in the server's index - unless a matching file is there already or the
    // server was found to have none before
    BoxArt fetchBoxArt(const std::string &thumbnailsDir, const std::string &dbName, const std::string &label);
    // the file names in the server's Named_Boxarts/ folder for a database (fetched once per instance,
    // an empty list when the index could not be read)
    const std::vector<std::string> &serverIndex(const std::string &dbName);
    // the file names an nginx/Apache directory index page links to, URL-decoded, images only
    static std::vector<std::string> parseIndex(const std::string &html);
    static std::string urlDecode(const std::string &s);

    // one cover to ask for: <thumbnailsDir>/<database>/Named_Boxarts/<label>.png
    struct BoxArtRequest {
        std::string database;
        std::string label;
    };
    // called for each cover that arrived, with the file it went to
    using OnFetched = std::function<void(const BoxArtRequest &, const std::string &path)>;

    // the box art of every request without one on disk (this instance's own ThumbnailLookup says, fuzzy
    // fallback included), reported as ScanStage::FetchingBoxArt per game; the pass ends when the network
    // goes or shouldStop() says so. Returns how many covers arrived; `missing` counts the server's misses.
    int fetchMissingBoxArt(const std::vector<BoxArtRequest> &requests, const std::string &thumbnailsDir,
                           ableem::ScanProgressListener *listener, const std::function<bool()> &shouldStop,
                           int *missing = nullptr, const OnFetched &onFetched = OnFetched());
    // the same for the ROM scan's games: database + label as the playlist has them
    int fetchMissingBoxArt(const std::vector<ableem::RetroArchScanResult::Game> &games,
                           const std::string &thumbnailsDir, ableem::ScanProgressListener *listener,
                           const std::function<bool()> &shouldStop, int *missing = nullptr);
    // The PS1 games the scan left without a cover - no PNG next to the game (the user's, or the covers
    // db's) and nothing in the thumbnails tree - as requests against "Sony - PlayStation", named by the
    // rdb's record name when the scan found one (that is how libretro-thumbnails names its files) and by
    // the title otherwise, a best effort the server's miss list keeps cheap.
    static std::vector<BoxArtRequest> ps1Requests(const std::vector<ableem::UsbGamePtr> &games);

    static std::string boxArtUrl(const std::string &baseUrl, const std::string &dbName, const std::string &label);
    static std::string boxArtPath(const std::string &thumbnailsDir, const std::string &dbName,
                                  const std::string &label);
    // the same for a file name the server already has (no escaping, no .png added)
    static std::string boxArtFileUrl(const std::string &baseUrl, const std::string &dbName,
                                     const std::string &fileName);
    static std::string boxArtDir(const std::string &thumbnailsDir, const std::string &dbName);
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
    std::map<std::string, std::vector<std::string>> indexCache_; // serverIndex(), per database
};
