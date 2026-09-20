#include "core/install_job_base.h"

#include <ableem/engine/filesystem.h>
#include <ableem/engine/log.h>
#include <ableem/engine/sha256.h>
#include <ableem/engine/strings.h>

#include <fstream>
#include <sstream>

using namespace std;
using ableem::DirEntry;
using ableem::PackCatalog;
using ableem::Sha256;
using ableem::TarArchive;
using ableem::UpdateFile;

string readText(const string &path) {
    ifstream in(path, ios::binary);
    if (!in)
        return "";
    stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

bool writeText(const string &path, const string &text) {
    ofstream out(path, ios::binary | ios::trunc);
    out << text;
    return static_cast<bool>(out);
}

string trimmed(const string &s) {
    return ableem::Strings::trim(s);
}

string firstLine(const string &text) {
    size_t nl = text.find_first_of("\r\n");
    return trimmed(nl == string::npos ? text : text.substr(0, nl));
}

string sidecarHash(const string &text) {
    string line = firstLine(text);
    size_t sp = line.find(' ');
    return sp == string::npos ? line : line.substr(0, sp);
}

string humanSize(uint64_t bytes) {
    char buf[32];
    if (bytes >= 1024ull * 1024 * 1024)
        snprintf(buf, sizeof(buf), "%.1f GB", bytes / (1024.0 * 1024 * 1024));
    else if (bytes >= 1024ull * 1024)
        snprintf(buf, sizeof(buf), "%.1f MB", bytes / (1024.0 * 1024));
    else
        snprintf(buf, sizeof(buf), "%.0f KB", bytes / 1024.0);
    return buf;
}

//*******************************
// InstallJobBase::stopped / phase / say
//*******************************
bool InstallJobBase::stopped(string &error) {
    if (stop && stop()) {
        error = "Stopped";
        return true;
    }
    return false;
}

void InstallJobBase::phase(const string &title) {
    phaseIndex++;
    out.onPhase(phaseIndex, static_cast<int>(phases.size()), title);
    out.onProgress(0, 0);
    say("== " + title);
}

void InstallJobBase::say(const string &line) {
    out.onLine(line);
    PLOG_INFO << line;
}

//*******************************
// InstallJobBase::download
//*******************************
bool InstallJobBase::download(const string &url, const string &dest, string &error) {
    DirEntry::createDirs(dest.substr(0, dest.find_last_of('/')));
    bool ok = dl.fetch(
        url, dest,
        [this](uint64_t done, uint64_t total) {
            out.onProgress(done, total);
            return !(stop && stop());
        },
        error);
    if (!ok && stop && stop())
        error = "Stopped";
    return ok;
}

//*******************************
// InstallJobBase::downloadVerified
//*******************************
bool InstallJobBase::downloadVerified(const UpdateFile &file, const string &dest, string &error) {
    if (DirEntry::exists(dest) && !file.sha256.empty() && Sha256::ofFile(dest) == file.sha256) {
        say("  " + file.name + " is already there");
        return true;
    }
    say("  " + file.name + (file.size ? " (" + humanSize(file.size) + ")" : ""));
    const string part = dest + ".part";
    if (!download(file.url, part, error))
        return false;
    if (!file.sha256.empty() && Sha256::ofFile(part) != file.sha256) {
        DirEntry::removeFile(part);
        error = file.name + ": the download does not match its sha256";
        return false;
    }
    DirEntry::removeFile(dest);
    if (!DirEntry::renameFile(part, dest)) {
        error = "cannot write " + dest;
        return false;
    }
    return true;
}

//*******************************
// InstallJobBase::fetchCatalog / untar
//*******************************
bool InstallJobBase::fetchCatalog(const string &rel, string &text, string &error) {
    return dl.fetchText(repoUrl + "/" + rel, scratch + "/catalog.json", text, error);
}

bool InstallJobBase::untar(const string &tarball, const string &dest, string &error, const TarArchive::Filter &filter,
                           const string &prefix) {
    return TarArchive::extract(
        tarball, dest, error, filter, [this](uint64_t done, uint64_t total) { out.onProgress(done, total); }, prefix);
}

//*******************************
// InstallJobBase::fetchBiosPack
//*******************************
bool InstallJobBase::fetchBiosPack(const string &catalogRel, const string &dir, string &error) {
    string text;
    PackCatalog cat;
    if (!fetchCatalog(catalogRel, text, error) || !cat.parse(text)) {
        if (error.empty())
            error = catalogRel + " is not what was expected";
        return false;
    }
    string list;
    if (!dl.fetchText(cat.file.url, scratch + "/biospack.txt", list, error))
        return false;
    struct Item {
        string sha, url, path;
        uint64_t size;
    };
    vector<Item> items;
    istringstream in(list);
    string line;
    while (getline(in, line)) {
        if (line.empty() || line[0] == '#')
            continue;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        // <sha256> <size> <url> <path> - the path may contain spaces
        size_t a = line.find(' '), b = line.find(' ', a + 1), c = line.find(' ', b + 1);
        if (a == string::npos || b == string::npos || c == string::npos)
            continue;
        Item it;
        it.sha = line.substr(0, a);
        it.size = strtoull(line.substr(a + 1, b - a - 1).c_str(), nullptr, 10);
        it.url = line.substr(b + 1, c - b - 1);
        it.path = line.substr(c + 1);
        if (TarArchive::isSafeName(it.path))
            items.push_back(it);
    }
    say("  " + to_string(items.size()) + " files, " + humanSize(cat.totalBytes) + " - what is there already is kept");
    int fetched = 0, kept = 0, failed = 0;
    for (size_t i = 0; i < items.size(); i++) {
        if (stopped(error))
            return false;
        const Item &it = items[i];
        out.onProgress(i, items.size());
        const string dest = dir + "/" + it.path;
        if (DirEntry::exists(dest) && static_cast<uint64_t>(DirEntry::fileSize(dest)) == it.size &&
            Sha256::ofFile(dest) == it.sha) {
            kept++;
            continue;
        }
        DirEntry::createDirs(dest.substr(0, dest.find_last_of('/')));
        const string part = dest + ".part";
        string why;
        bool ok = dl.fetch(it.url, part, [this](uint64_t, uint64_t) { return !(stop && stop()); }, why);
        if (ok && Sha256::ofFile(part) != it.sha) {
            ok = false;
            why = "checksum mismatch";
        }
        if (ok) {
            DirEntry::removeFile(dest);
            ok = DirEntry::renameFile(part, dest);
        }
        if (ok) {
            fetched++;
        } else {
            DirEntry::removeFile(part);
            failed++;
            if (failed <= 20)
                say("  could not fetch " + it.path + ": " + why);
            if (stop && stop()) {
                error = "Stopped";
                return false;
            }
        }
        if ((fetched + failed) % 50 == 0 && fetched + failed > 0)
            say("  " + to_string(fetched) + " fetched, " + to_string(kept) + " kept, " + to_string(failed) +
                " failed so far");
    }
    out.onProgress(items.size(), items.size());
    say("  " + to_string(fetched) + " fetched, " + to_string(kept) + " already there, " + to_string(failed) +
        " failed");
    return true;
}
