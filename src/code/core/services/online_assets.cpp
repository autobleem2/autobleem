//
// OnlineAssets - see the header.
//
#include "online_assets.h"
#include "environment.h"
#include "../main.h"

#include <ableem/engine/game_scanner.h>
#include <ableem/engine/log.h>
#include <ableem/engine/thumbnail_lookup.h>
#include <ableem/engine/zip_archive.h>

#include <cstdio>
#include <cstdlib>
#include <fstream>

using namespace std;

namespace {

const char *const MissingListName = ".autobleem-missing.txt";

// creates every level of `dir`; true if it exists afterwards
bool createDirs(const string &dir) {
    if (dir.empty() || DirEntry::isDirectory(dir))
        return true;
    size_t slash = dir.find_last_of('/');
    if (slash != string::npos && slash > 0 && !createDirs(dir.substr(0, slash)))
        return false;
    DirEntry::createDir(dir);
    return DirEntry::isDirectory(dir);
}

string dirOf(const string &path) {
    size_t slash = path.find_last_of('/');
    return slash == string::npos ? "" : path.substr(0, slash);
}

} // namespace

//*******************************
// OnlineAssets::OnlineAssets
//*******************************
OnlineAssets::OnlineAssets(const Config &config, CommandRunner runner) : config_(config), runner_(std::move(runner)) {
    if (!runner_)
        runner_ = [](const string &commandLine) { return system(commandLine.c_str()); };
}

//*******************************
// OnlineAssets::urlEncode
//*******************************
string OnlineAssets::urlEncode(const string &s) {
    static const char *const hex = "0123456789ABCDEF";
    string out;
    for (unsigned char c : s) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            out += static_cast<char>(c);
        } else {
            out += '%';
            out += hex[c >> 4];
            out += hex[c & 15];
        }
    }
    return out;
}

//*******************************
// OnlineAssets::boxArtUrl / boxArtPath / missingListPath
//*******************************
string OnlineAssets::boxArtUrl(const string &baseUrl, const string &dbName, const string &label) {
    return DirEntry::removeSeparatorFromEndOfPath(baseUrl) + "/" + urlEncode(dbName) + "/Named_Boxarts/" +
           urlEncode(ableem::ThumbnailLookup::escapeName(label) + ".png");
}

string OnlineAssets::boxArtPath(const string &thumbnailsDir, const string &dbName, const string &label) {
    return DirEntry::removeSeparatorFromEndOfPath(thumbnailsDir) + sep + dbName + sep + "Named_Boxarts" + sep +
           ableem::ThumbnailLookup::escapeName(label) + ".png";
}

string OnlineAssets::missingListPath(const string &thumbnailsDir, const string &dbName) {
    return DirEntry::removeSeparatorFromEndOfPath(thumbnailsDir) + sep + dbName + sep + "Named_Boxarts" + sep +
           MissingListName;
}

//*******************************
// OnlineAssets::loadMissingList / saveMissingList
//*******************************
set<string> OnlineAssets::loadMissingList(const string &path) {
    set<string> names;
    ifstream in(path);
    string line;
    while (getline(in, line)) {
        trim(line);
        if (!line.empty())
            names.insert(line);
    }
    return names;
}

bool OnlineAssets::saveMissingList(const string &path, const set<string> &names) {
    if (!createDirs(dirOf(path)))
        return false;
    ofstream out(path);
    if (!DirEntry::checkWritable(out, path))
        return false;
    for (const string &name : names)
        out << name << "\n";
    return true;
}

//*******************************
// OnlineAssets::commandFor
//*******************************
string OnlineAssets::commandFor(const string &url, const string &outPath) const {
    string cmd = config_.downloadCommand;
    Strings::replaceAll(cmd, "%u", url);
    Strings::replaceAll(cmd, "%o", outPath);
    return cmd;
}

//*******************************
// OnlineAssets::fetch
//*******************************
bool OnlineAssets::fetch(const string &url, const string &outPath) {
    if (!enabled())
        return false;
    if (!createDirs(dirOf(outPath))) {
        PLOG_WARNING << "Cannot create " << dirOf(outPath);
        return false;
    }
    // to a .part next to the target, so a fetch cut short never leaves a half file under the real name
    const string part = outPath + ".part";
    DirEntry::removeFile(part);
    const int status = runner_(commandFor(url, part));
    if (status != 0 || DirEntry::fileSize(part) <= 0) {
        PLOG_DEBUG << "Fetch failed (" << status << "): " << url;
        DirEntry::removeFile(part);
        return false;
    }
    DirEntry::removeFile(outPath);
    if (!DirEntry::renameFile(part, outPath)) {
        DirEntry::removeFile(part);
        return false;
    }
    return true;
}

//*******************************
// OnlineAssets::probe
//*******************************
bool OnlineAssets::probe(bool again) {
    if (!enabled())
        return false;
    if (probed_ && !again)
        return online_;
    const bool first = !probed_;
    probed_ = true;
    const string probeFile = Env::getWorkingPath() + sep + ".online-probe";
    online_ = fetch(config_.thumbnailsBaseUrl + "/", probeFile);
    DirEntry::removeFile(probeFile);
    // the first answer is news; a re-probe after a server miss is routine unless the answer changed
    if (first || !online_) {
        PLOG_INFO << (online_ ? "Online: " : "Not online: ") << config_.thumbnailsBaseUrl;
    } else {
        PLOG_DEBUG << "Still online: " << config_.thumbnailsBaseUrl;
    }
    return online_;
}

//*******************************
// OnlineAssets::ensureDatabases
//*******************************
int OnlineAssets::ensureDatabases(const string &rdbDir) {
    auto countRdbs = [&]() {
        int n = 0;
        for (const DirEntry &e : DirEntry::diru_FilesOnly(rdbDir))
            n += DirEntry::matchExtension(e.name, "rdb") ? 1 : 0;
        return n;
    };
    int have = countRdbs();
    if (have > 0 || !probe())
        return have;

    const string dir = DirEntry::removeSeparatorFromEndOfPath(rdbDir);
    const string zipPath = dir + sep + "database-rdb.zip";
    PLOG_INFO << "Fetching the databases bundle: " << config_.databasesUrl;
    if (!fetch(config_.databasesUrl, zipPath))
        return 0;

    // the bundle holds rdb/<system>.rdb; a flat one would land in rdbDir itself
    vector<string> names;
    bool nested = false;
    if (ZipArchive::list(zipPath, names)) {
        for (const string &name : names)
            nested = nested || name.rfind("rdb/", 0) == 0;
    }
    const string dest = nested ? dirOf(dir) : dir;
    if (!ZipArchive::extract(zipPath, dest)) {
        PLOG_WARNING << "Could not unpack " << zipPath;
    }
    DirEntry::removeFile(zipPath);
    have = countRdbs();
    PLOG_INFO << "Databases in " << rdbDir << ": " << have;
    return have;
}

//*******************************
// OnlineAssets::fetchMissingBoxArt
//*******************************
int OnlineAssets::fetchMissingBoxArt(const vector<BoxArtRequest> &requests, const string &thumbnailsDir,
                                     ableem::ScanProgressListener *listener, const function<bool()> &shouldStop,
                                     int *missing, const OnFetched &onFetched) {
    if (missing)
        *missing = 0;
    ableem::ThumbnailLookup thumbnails;
    vector<const BoxArtRequest *> wanted;
    for (const auto &request : requests) {
        if (thumbnails.findBoxArt(request.database, request.label).empty())
            wanted.push_back(&request);
    }
    if (wanted.empty() || !probe())
        return 0;

    int fetched = 0, notOnServer = 0, index = 0;
    for (const auto *request : wanted) {
        index++;
        if (listener)
            listener->onScanProgress(ableem::ScanStage::FetchingBoxArt, request->label, index,
                                     static_cast<int>(wanted.size()));
        BoxArt outcome = fetchBoxArt(thumbnailsDir, request->database, request->label);
        if (outcome == BoxArt::Fetched) {
            fetched++;
            if (onFetched)
                onFetched(*request, boxArtPath(thumbnailsDir, request->database, request->label));
        } else if (outcome == BoxArt::Missing) {
            notOnServer++;
        } else if (outcome == BoxArt::Failed && !online_) {
            break; // the network went
        }
        if (shouldStop && shouldStop())
            break;
    }
    PLOG_INFO << "Box art: " << fetched << " fetched, " << notOnServer << " not on the server, of " << wanted.size()
              << " games without one";
    if (missing)
        *missing = notOnServer;
    return fetched;
}

int OnlineAssets::fetchMissingBoxArt(const vector<ableem::RetroArchScanResult::Game> &games,
                                     const string &thumbnailsDir, ableem::ScanProgressListener *listener,
                                     const function<bool()> &shouldStop, int *missing) {
    vector<BoxArtRequest> requests;
    requests.reserve(games.size());
    for (const auto &game : games)
        requests.push_back({game.database, game.label});
    return fetchMissingBoxArt(requests, thumbnailsDir, listener, shouldStop, missing);
}

//*******************************
// OnlineAssets::ps1Requests
//*******************************
vector<OnlineAssets::BoxArtRequest> OnlineAssets::ps1Requests(const vector<ableem::UsbGamePtr> &games) {
    vector<BoxArtRequest> requests;
    for (const auto &game : games) {
        if (!game || game->coverImageFound || !game->coverPath.empty())
            continue;
        const string &label = game->recordName.empty() ? game->title : game->recordName;
        if (label.empty())
            continue;
        requests.push_back({ableem::ThumbnailLookup::PlayStationDbName, label});
    }
    return requests;
}

//*******************************
// OnlineAssets::fetchBoxArt
//*******************************
OnlineAssets::BoxArt OnlineAssets::fetchBoxArt(const string &thumbnailsDir, const string &dbName, const string &label) {
    const string path = boxArtPath(thumbnailsDir, dbName, label);
    if (DirEntry::exists(path))
        return BoxArt::AlreadyThere;
    const string listPath = missingListPath(thumbnailsDir, dbName);
    set<string> missing = loadMissingList(listPath);
    const string name = ableem::ThumbnailLookup::escapeName(label);
    if (missing.count(name))
        return BoxArt::Missing;
    if (!probe())
        return BoxArt::Failed;
    if (fetch(boxArtUrl(config_.thumbnailsBaseUrl, dbName, label), path))
        return BoxArt::Fetched;
    // the server said no - or the network went: the probe tells which
    if (!probe(true))
        return BoxArt::Failed;
    missing.insert(name);
    saveMissingList(listPath, missing);
    return BoxArt::Missing;
}
