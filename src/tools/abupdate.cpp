// abupdate - the console's own update: what the launcher downloaded, laid over the stick - the stick package,
// and/or RetroArch's zip.
//
//   abupdate ROOT [--download "COMMAND %u %o"] [--repo URL]
//
// The launcher's online update (UpdateService - only on a console with a network, which the AutoBleem
// kernel's WiFi gives it) fetches the channel's autobleem-psc-<version>.tar.gz (and, when the stick has RetroArch and
// the site has a newer build, retroarch-psc-<version>.zip) into ROOT/System/Updates/,
// checks it against the site's sha256 and writes pending.json there, then leaves with MENU_OPTION_UPDATE.
// rc/selection.sh copies this program to tmpfs - the update replaces Autobleem/bin/autobleem, where it
// lives - and runs it with ROOT=/media. It is the PC installer's own update, autobleem-core's InstallerJob
// with the package given: what the package ships is replaced (the launcher, the emulators, the rc scripts,
// the console tools, the shipped themes, Docs/), everything of the user's stays (games, saves, memory cards,
// config.ini's settings, RetroArch and its cores, the cover databases). UpdateRoms for the stick's PC side
// comes from the site through the download command - abfetch, the launcher's own downloader, from the
// folder this program runs in (selection.sh copies both to tmpfs; nothing of the kernel payload's). Its record is
// ROOT/System/Logs/installer.log; stdout goes to update.log (selection.sh). RetroArch's zip is the same job
// again with InstallOptions::retroarchZip: the binary and its docs, the theme over RetroArch/bin/assets, the
// wallpaper, retroarch.cfg's merge, the theme's keys and the VERSION stamp - what the PC installer does for a
// stick that has RetroArch; the cores, saves, playlists and the user's own assets stay. It is not touched while
// RetroArch runs, and a zip that cannot be laid leaves the old RetroArch as it was. Exit 0 when the stick is updated
// (System/Updates is removed then), 1 when not - the stick then keeps what it had, apart from a failure
// half way through the unpacking, which the next update or the PC installer repairs.
#include "installer/command_downloader.h"
#include "installer/installer_job.h"

#include <ableem/engine/filesystem.h>
#include <ableem/engine/log.h>
#include <ableem/engine/update_catalog.h>

#include <cstdio>
#include <cstring>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <iostream>
#include <string>
#ifndef _WIN32
#include <dirent.h>
#endif

using namespace std;
using ableem::DirEntry;
using ableem::PendingUpdate;

namespace {

// the folder this program is in, with a trailing '/' ("" when it cannot be told - then abfetch is looked
// for in the current directory, which selection.sh makes /tmp)
string ownDir() {
#ifdef _WIN32
    return "";
#else
    char buffer[4096];
    ssize_t length = readlink("/proc/self/exe", buffer, sizeof(buffer) - 1);
    if (length <= 0)
        return "";
    string path(buffer, static_cast<size_t>(length));
    return path.substr(0, path.rfind('/') + 1);
#endif
}

// whether a RetroArch process is running (busybox's /proc/<pid>/comm names it "retroarch"): its binary is
// not replaced under it
bool retroArchRunning() {
#ifdef _WIN32
    return false;
#else
    DIR *proc = opendir("/proc");
    if (proc == nullptr)
        return false;
    bool found = false;
    while (dirent *entry = readdir(proc)) {
        if (entry->d_name[0] < '0' || entry->d_name[0] > '9')
            continue;
        FILE *comm = fopen((string("/proc/") + entry->d_name + "/comm").c_str(), "r");
        if (comm == nullptr)
            continue;
        char name[64] = {0};
        if (fgets(name, sizeof(name), comm) != nullptr && strncmp(name, "retroarch", 9) == 0)
            found = true;
        fclose(comm);
        if (found)
            break;
    }
    closedir(proc);
    return found;
#endif
}

class Printer : public InstallListener {
public:
    void onPhase(int index, int total, const string &title) override {
        cout << "[" << index << "/" << total << "] " << title << endl;
    }
    void onProgress(uint64_t, uint64_t) override {}
    void onLine(const string &line) override { cout << line << endl; }
};

int usage() {
    cerr << "usage: abupdate ROOT [--download \"COMMAND %u %o\"] [--repo URL]" << endl;
    return 2;
}

} // namespace

int main(int argc, char **argv) {
    if (argc < 2)
        return usage();
    string root = InstallerJob::normalizeRoot(argv[1]);
    // abfetch next to this program (fails on an HTTP error, follows redirects, gives up on a stall), with
    // its cacert.pem beside it
    string download = "\"" + ownDir() + "abfetch\" --connect-timeout 20 --stall-timeout 60 -o \"%o\" \"%u\"";
    InstallOptions options;
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--download") == 0 && i + 1 < argc)
            download = argv[++i];
        else if (strcmp(argv[i], "--repo") == 0 && i + 1 < argc)
            options.repoUrl = argv[++i];
        else
            return usage();
    }
    ableem::Log::initConsoleOnly();

    const string updates = root + (root.back() == '/' ? "" : "/") + "System/Updates";
    PendingUpdate pending;
    if (!pending.load(updates + "/pending.json") || (pending.autobleemFile.empty() && pending.retroarchFile.empty())) {
        cout << "abupdate: nothing to install - no AutoBleem package or RetroArch zip in " << updates << "/pending.json"
             << endl;
        return 1;
    }
    const auto inUpdates = [&](const string &file) { return file[0] == '/' ? file : updates + "/" + file; };
    const string package = pending.autobleemFile.empty() ? "" : inUpdates(pending.autobleemFile);
    const string retroarchZip = pending.retroarchFile.empty() ? "" : inUpdates(pending.retroarchFile);
    if ((!package.empty() && !DirEntry::exists(package)) ||
        (!retroarchZip.empty() && !DirEntry::exists(retroarchZip))) {
        cout << "abupdate: " << (!package.empty() && !DirEntry::exists(package) ? package : retroarchZip)
             << " is not there" << endl;
        return 1;
    }
    // before anything is replaced: RetroArch's binary is not swapped under a running RetroArch
    if (!retroarchZip.empty() && retroArchRunning()) {
        cout << "abupdate: RetroArch is running - nothing is installed, System/Updates stays" << endl;
        return 1;
    }

    options.root = root;
    options.channel.clear(); // the package is given - no channel to fetch it from
    options.coversJapan = options.coversUsa = options.coversPal = false; // the stick has its own
    options.retroarch = options.bios = options.samples = false;
    options.scratchDir = "/tmp/abupdate"; // not on the stick being replaced
    CommandDownloader downloader(download);
    Printer printer;
    string error;
    if (!package.empty()) {
        cout << "abupdate: AutoBleem " << pending.autobleemVersion << " from " << package << " onto " << root << endl;
        options.packageFile = package;
        if (!InstallerJob::run(options, downloader, printer, []() { return false; }, error)) {
            cout << "abupdate: FAILED: " << error << endl;
            return 1;
        }
    }
    if (!retroarchZip.empty()) {
        cout << "abupdate: RetroArch " << pending.retroarchVersion << " from " << retroarchZip << " onto " << root
             << endl;
        options.packageFile.clear();
        options.retroarchZip = retroarchZip;
        error.clear();
        if (!InstallerJob::run(options, downloader, printer, []() { return false; }, error)) {
            cout << "abupdate: FAILED: " << error << endl;
            return 1;
        }
    }
    DirEntry::removeDirAndContents(updates);
    if (!package.empty())
        cout << "abupdate: the stick is AutoBleem " << pending.autobleemVersion << " now" << endl;
    if (!retroarchZip.empty())
        cout << "abupdate: RetroArch is " << pending.retroarchVersion << " now" << endl;
    return 0;
}
