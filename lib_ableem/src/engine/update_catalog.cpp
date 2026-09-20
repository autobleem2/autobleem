#include "ableem/engine/update_catalog.h"

#include <fstream>
#include <sstream>

#include <json.h>

#include "ableem/engine/filesystem.h"

using namespace std;
using namespace nlohmann;

namespace ableem {

namespace {

string str(const json &item, const char *key, const string &def = "") {
    auto it = item.find(key);
    if (it == item.end() || !it->is_string())
        return def;
    return it->get<string>();
}

uint64_t num(const json &item, const char *key, uint64_t def = 0) {
    auto it = item.find(key);
    if (it == item.end() || !it->is_number())
        return def;
    return it->get<uint64_t>();
}

int64_t inum(const json &item, const char *key, int64_t def = 0) {
    auto it = item.find(key);
    if (it == item.end() || !it->is_number())
        return def;
    return it->get<int64_t>();
}

bool parseFile(const json &item, UpdateFile &out) {
    if (!item.is_object())
        return false;
    out.name = str(item, "name");
    out.url = str(item, "url");
    out.sha256 = str(item, "sha256");
    out.size = num(item, "size");
    return out.valid();
}

string readText(const string &path) {
    ifstream in(path, ios::binary);
    if (!in)
        return "";
    stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// written whole to a .tmp next to the file and renamed over it: a launcher killed mid-write (the power
// button) must not leave a half file the next start reads as "no state"
bool writeText(const string &path, const string &text) {
    string tmp = path + ".tmp";
    {
        ofstream out(tmp, ios::binary);
        if (!out)
            return false;
        out << text;
        if (!out)
            return false;
    }
    return DirEntry::replaceFile(tmp, path);
}

json parseOrNull(const string &text) {
    try {
        return json::parse(text);
    } catch (const json::exception &) {
        return json();
    }
}

} // namespace

//*******************************
// ReleaseCatalog
//*******************************
bool ReleaseCatalog::parse(const string &jsonText) {
    json j = parseOrNull(jsonText);
    if (!j.is_object())
        return false;
    version = str(j, "version");
    date = str(j, "date");
    auto pre = j.find("prerelease");
    prerelease = pre != j.end() && pre->is_boolean() && pre->get<bool>();
    files.clear();
    auto f = j.find("files");
    if (f != j.end() && f->is_object()) {
        for (auto it = f->begin(); it != f->end(); ++it) {
            UpdateFile file;
            if (parseFile(it.value(), file))
                files[it.key()] = file;
        }
    }
    return !version.empty();
}

bool ReleaseCatalog::load(const string &path) {
    return parse(readText(path));
}

const UpdateFile *ReleaseCatalog::fileFor(const string &platformKey) const {
    auto it = files.find(platformKey);
    return it == files.end() ? nullptr : &it->second;
}

//*******************************
// RetroArchCatalog
//*******************************
bool RetroArchCatalog::parse(const string &jsonText) {
    json j = parseOrNull(jsonText);
    if (!j.is_object())
        return false;
    version = str(j, "version");
    files.clear();
    for (auto it = j.begin(); it != j.end(); ++it) {
        UpdateFile file;
        if (it.value().is_object() && parseFile(it.value(), file))
            files[it.key()] = file;
    }
    return !version.empty();
}

bool RetroArchCatalog::load(const string &path) {
    return parse(readText(path));
}

const UpdateFile *RetroArchCatalog::fileFor(const string &arch) const {
    auto it = files.find(arch);
    return it == files.end() ? nullptr : &it->second;
}

//*******************************
// PackCatalog
//*******************************
bool PackCatalog::parse(const string &jsonText) {
    json j = parseOrNull(jsonText);
    if (!j.is_object() || !parseFile(j, file))
        return false;
    manifestUrl = str(j, "manifest");
    date = str(j, "date");
    count = static_cast<int>(inum(j, "count"));
    totalBytes = num(j, "total_bytes");
    return true;
}

bool PackCatalog::load(const string &path) {
    return parse(readText(path));
}

//*******************************
// PscRetroArchCatalog
//*******************************
bool PscRetroArchCatalog::parse(const string &jsonText) {
    json j = parseOrNull(jsonText);
    if (!j.is_object())
        return false;
    version = str(j, "version");
    manifestUrl = str(j, "manifest");
    auto z = j.find("zip");
    if (z == j.end() || !parseFile(*z, zip))
        return false;
    return !version.empty();
}

bool PscRetroArchCatalog::load(const string &path) {
    return parse(readText(path));
}

//*******************************
// UpdateState
//*******************************
bool UpdateState::load(const string &path) {
    json j = parseOrNull(readText(path));
    if (!j.is_object())
        return false;
    lastCheck = inum(j, "last_check");
    postponedUntil = inum(j, "postponed_until");
    skippedVersion = str(j, "skipped_version");
    skippedRetroArch = str(j, "skipped_retroarch");
    return true;
}

bool UpdateState::save(const string &path) const {
    json j;
    j["last_check"] = lastCheck;
    j["postponed_until"] = postponedUntil;
    j["skipped_version"] = skippedVersion;
    j["skipped_retroarch"] = skippedRetroArch;
    return writeText(path, j.dump(2) + "\n");
}

//*******************************
// PendingUpdate
//*******************************
bool PendingUpdate::load(const string &path) {
    json j = parseOrNull(readText(path));
    if (!j.is_object())
        return false;
    autobleemVersion = str(j, "autobleem_version");
    autobleemFile = str(j, "autobleem_file");
    retroarchVersion = str(j, "retroarch_version");
    retroarchFile = str(j, "retroarch_file");
    return !autobleemFile.empty() || !retroarchFile.empty();
}

bool PendingUpdate::save(const string &path) const {
    json j;
    j["autobleem_version"] = autobleemVersion;
    j["autobleem_file"] = autobleemFile;
    j["retroarch_version"] = retroarchVersion;
    j["retroarch_file"] = retroarchFile;
    return writeText(path, j.dump(2) + "\n");
}

} // namespace ableem
