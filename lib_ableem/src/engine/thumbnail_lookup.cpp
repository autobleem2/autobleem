#include "ableem/engine/thumbnail_lookup.h"
#include "ableem/engine/environment.h"
#include "ableem/engine/filesystem.h"

#include <cctype>
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

string lowered(string s) {
    for (auto &c : s)
        c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
    return s;
}

// the names lowered once, next to the originals, for the case-insensitive comparisons below
struct Listing {
    const vector<string> &names;
    vector<string> lower;
    explicit Listing(const vector<string> &n) : names(n) {
        lower.reserve(n.size());
        for (const auto &name : n)
            lower.push_back(lowered(name));
    }
};

// the name equal to <base>.<image ext> ignoring case, png before jpg before jpeg
string findExact(const Listing &listing, const string &base) {
    const string b = lowered(base);
    for (const auto &ext : imageExtensions()) {
        const string want = b + ext;
        for (size_t i = 0; i < listing.lower.size(); ++i) {
            if (listing.lower[i] == want)
                return listing.names[i];
        }
    }
    return "";
}

// the candidate as a file in dir (a stat per extension - not the listing, so a file that appeared after
// the listing was cached is still found), then each shorter form with one more trailing " (...)" tag
// peeled off
string existingWithTagStripping(const string &dir, string candidate) {
    while (!candidate.empty()) {
        const string base = dir + ThumbnailLookup::escapeName(candidate);
        for (const auto &ext : imageExtensions()) {
            if (DirEntry::exists(base + ext))
                return base + ext;
        }
        const auto pos = candidate.rfind(" (");
        if (pos == string::npos || candidate.back() != ')')
            return "";
        candidate.erase(pos);
    }
    return "";
}

// the same over a listing, ignoring case
string tryWithTagStripping(const Listing &listing, string candidate) {
    while (!candidate.empty()) {
        const string hit = findExact(listing, ThumbnailLookup::escapeName(candidate));
        if (!hit.empty())
            return hit;
        const auto pos = candidate.rfind(" (");
        if (pos == string::npos || candidate.back() != ')')
            return "";
        candidate.erase(pos);
    }
    return "";
}

// Any file named "<bare> (...)<ext>" - the literal " (" is what keeps "Doom" from taking "Doom 2 - Hell on
// Earth (USA).jpg". Scored by the caller's original tags (+10 each: a user with "Suikoden (USA)" gets
// "Suikoden (USA) (Rev 1).jpg" over "Suikoden (Europe).jpg"), then region (USA 3, Europe 2, World 1),
// then the name itself, so the pick is deterministic.
string fuzzyMatch(const Listing &listing, const string &bare, const vector<string> &preferredTags) {
    if (bare.empty())
        return "";
    const string prefix = lowered(ThumbnailLookup::escapeName(bare)) + " (";
    vector<string> tags;
    for (const auto &tag : preferredTags)
        tags.push_back(lowered(tag));

    string best;
    int bestScore = -1;
    for (size_t i = 0; i < listing.lower.size(); ++i) {
        const string &name = listing.lower[i];
        if (name.size() <= prefix.size())
            continue;
        if (name.compare(0, prefix.size(), prefix) != 0)
            continue;
        if (!hasImageExtension(name))
            continue;

        int score = 0;
        for (const auto &tag : tags) {
            if (!tag.empty() && name.find(tag) != string::npos)
                score += 10;
        }
        if (name.find("(usa)") != string::npos)
            score += 3;
        else if (name.find("(europe)") != string::npos)
            score += 2;
        else if (name.find("(world)") != string::npos)
            score += 1;

        if (score > bestScore || (score == bestScore && (best.empty() || name < best))) {
            bestScore = score;
            best = listing.names[i];
        }
    }
    return best;
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
// ThumbnailLookup::pickName
//*******************************
string ThumbnailLookup::pickName(const vector<string> &names, const string &title, const string &recordName) {
    if (names.empty())
        return "";
    const Listing listing(names);

    if (!recordName.empty()) {
        const string hit = tryWithTagStripping(listing, recordName);
        if (!hit.empty())
            return hit;
    }
    if (!title.empty()) {
        const string hit = tryWithTagStripping(listing, title);
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
    return fuzzyMatch(listing, stripTrailingTags(fuzzySource), preferredTags);
}

//*******************************
// ThumbnailLookup::findThumbnail
//*******************************
string ThumbnailLookup::findThumbnail(const string &dbName, const string &title, const string &thumbnailDir,
                                      const string &recordName) {
    if (dbName.empty() || thumbnailDir.empty())
        return "";
    const string dir = thumbnailsDir_ + sep + DirEntry::getFileNameWithoutExtension(dbName) + sep + thumbnailDir + sep;
    // the exact spellings by a stat first (the file may be newer than the cached listing), then the
    // listing for a different case and the fuzzy fallback
    if (!recordName.empty()) {
        const string hit = existingWithTagStripping(dir, recordName);
        if (!hit.empty())
            return hit;
    }
    if (!title.empty()) {
        const string hit = existingWithTagStripping(dir, title);
        if (!hit.empty())
            return hit;
    }
    const string name = pickName(listDir(dir), title, recordName);
    return name.empty() ? "" : dir + name;
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
