#include "ableem/engine/thumbnail_lookup.h"
#include "ableem/engine/environment.h"
#include "ableem/engine/filesystem.h"

#include <utility>

using namespace std;

namespace ableem {

const char ThumbnailLookup::PlayStationDbName[] = "Sony - PlayStation";

namespace {

const vector<string> &imageExtensions() {
    static const vector<string> exts{".png", ".jpg", ".jpeg"};
    return exts;
}

bool hasImageExtension(const string &name) {
    for (const auto &ext : imageExtensions()) {
        if (DirEntry::matchExtension(name, ext))
            return true;
    }
    return false;
}

// "Foo (USA) (Rev 1)" -> ["(USA)", "(Rev 1)"]: only well-formed trailing " (...)" tags, nothing from
// the middle of a name
vector<string> extractTrailingTags(const string &name) {
    vector<string> tags;
    string remaining = name;
    while (!remaining.empty() && remaining.back() == ')') {
        const auto pos = remaining.rfind(" (");
        if (pos == string::npos)
            break;
        tags.push_back(remaining.substr(pos + 1));
        remaining.erase(pos);
    }
    return tags;
}

string stripTrailingTags(string name) {
    while (!name.empty() && name.back() == ')') {
        const auto pos = name.rfind(" (");
        if (pos == string::npos)
            break;
        name.erase(pos);
    }
    return name;
}

} // namespace

//*******************************
// ThumbnailLookup::ThumbnailLookup
//*******************************
ThumbnailLookup::ThumbnailLookup()
    : thumbnailsDir_(Environment::getPathToRetroarchThumbnailsDir()),
      screenshotsDir_(Environment::getPathToRetroarchScreenshotsDir()),
      statesDir_(Environment::getPathToRetroarchStatesDir()) {}

//*******************************
// ThumbnailLookup::escapeName
//*******************************
string ThumbnailLookup::escapeName(const string &name) {
    return DirEntry::replaceTheseCharsWithThisChar(name, "&*/:`<>?\\|", '_');
}

//*******************************
// ThumbnailLookup::listDir
//*******************************
const vector<string> &ThumbnailLookup::listDir(const string &dir) {
    auto it = dirCache_.find(dir);
    if (it != dirCache_.end())
        return it->second;
    return dirCache_.emplace(dir, DirEntry::listNames(dir)).first->second;
}

//*******************************
// ThumbnailLookup::tryWithTagStripping
//*******************************
// the candidate, then each shorter form with one more trailing " (...)" tag peeled off
string ThumbnailLookup::tryWithTagStripping(const string &dir, string candidate) {
    while (!candidate.empty()) {
        const string base = dir + escapeName(candidate);
        for (const auto &ext : imageExtensions()) {
            const string path = base + ext;
            if (DirEntry::exists(path))
                return path;
        }
        const auto pos = candidate.rfind(" (");
        if (pos == string::npos || candidate.back() != ')')
            return "";
        candidate.erase(pos);
    }
    return "";
}

//*******************************
// ThumbnailLookup::fuzzyMatch
//*******************************
// Any file named "<bare> (...)<ext>" - the literal " (" is what keeps "Doom" from taking "Doom 2 - Hell on
// Earth (USA).jpg". Scored by the caller's original tags (+10 each: a user with "Suikoden (USA)" gets
// "Suikoden (USA) (Rev 1).jpg" over "Suikoden (Europe).jpg"), then region (USA 3, Europe 2, World 1),
// then the name itself, so the pick is deterministic.
string ThumbnailLookup::fuzzyMatch(const string &dir, const string &bare, const vector<string> &preferredTags) {
    if (bare.empty())
        return "";
    const string prefix = escapeName(bare) + " (";

    string best;
    int bestScore = -1;
    for (const auto &name : listDir(dir)) {
        if (name.size() <= prefix.size())
            continue;
        if (name.compare(0, prefix.size(), prefix) != 0)
            continue;
        if (!hasImageExtension(name))
            continue;

        int score = 0;
        for (const auto &tag : preferredTags) {
            if (!tag.empty() && name.find(tag) != string::npos)
                score += 10;
        }
        if (name.find("(USA)") != string::npos)
            score += 3;
        else if (name.find("(Europe)") != string::npos)
            score += 2;
        else if (name.find("(World)") != string::npos)
            score += 1;

        if (score > bestScore || (score == bestScore && (best.empty() || name < best))) {
            bestScore = score;
            best = name;
        }
    }
    return best.empty() ? "" : dir + best;
}

//*******************************
// ThumbnailLookup::findThumbnail
//*******************************
string ThumbnailLookup::findThumbnail(const string &dbName, const string &title, const string &thumbnailDir,
                                      const string &recordName) {
    if (dbName.empty() || thumbnailDir.empty())
        return "";

    const string dir = thumbnailsDir_ + sep + DirEntry::getFileNameWithoutExtension(dbName) + sep + thumbnailDir + sep;

    if (!recordName.empty()) {
        const string hit = tryWithTagStripping(dir, recordName);
        if (!hit.empty())
            return hit;
    }
    if (!title.empty()) {
        const string hit = tryWithTagStripping(dir, title);
        if (!hit.empty())
            return hit;
    }

    // the fuzzy fallback, from the canonical name when there is one
    const string &fuzzySource = !recordName.empty() ? recordName : title;
    if (fuzzySource.empty())
        return "";
    vector<string> preferredTags = extractTrailingTags(fuzzySource);
    if (!recordName.empty() && !title.empty() && recordName != title) {
        for (auto &t : extractTrailingTags(title))
            preferredTags.push_back(std::move(t));
    }
    return fuzzyMatch(dir, stripTrailingTags(fuzzySource), preferredTags);
}

//*******************************
// ThumbnailLookup::findBoxArt
//*******************************
string ThumbnailLookup::findBoxArt(const string &dbName, const string &title, const string &recordName) {
    static const char *const dirs[] = {"Named_Boxarts", "Named_Titles", "Named_Snaps"};
    for (const char *dir : dirs) {
        const string path = findThumbnail(dbName, title, dir, recordName);
        if (!path.empty())
            return path;
    }
    return "";
}

//*******************************
// ThumbnailLookup::findLocalScreenshot
//*******************************
string ThumbnailLookup::findLocalScreenshot(const string &romPath) {
    if (romPath.empty())
        return "";
    const string base = DirEntry::getFileNameWithoutExtension(DirEntry::getFileNameFromPath(romPath));
    if (base.empty())
        return "";

    if (DirEntry::exists(screenshotsDir_)) {
        // RetroArch writes "<base>.png" or "<base>-YYMMDD-HHMMSS.png"; the timestamped names sort by
        // recency, so the last one is the newest without a stat() each
        string newest;
        for (const auto &name : listDir(screenshotsDir_)) {
            if (name.size() <= base.size() || name.compare(0, base.size(), base) != 0)
                continue;
            const char next = name[base.size()];
            if (next != '.' && next != '-')
                continue;
            if (hasImageExtension(name) && name > newest)
                newest = name;
        }
        if (!newest.empty())
            return screenshotsDir_ + sep + newest;
    }

    const string stateThumb = statesDir_ + sep + base + ".state.auto.png";
    if (DirEntry::exists(stateThumb))
        return stateThumb;
    return "";
}

//*******************************
// ThumbnailLookup::findSnap
//*******************************
string ThumbnailLookup::findSnap(const string &dbName, const string &title, const string &romPath,
                                 const string &recordName) {
    const string local = findLocalScreenshot(romPath);
    if (!local.empty())
        return local;
    return findThumbnail(dbName, title, "Named_Snaps", recordName);
}

} // namespace ableem
