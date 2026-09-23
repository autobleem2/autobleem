// abupdate - the console's own update: the stick package the launcher downloaded, laid over the stick.
//
//   abupdate ROOT [--download "COMMAND %u %o"] [--repo URL]
//
// The launcher's online update (UpdateService - only on a console with a network, which the AutoBleem
// kernel's WiFi gives it) fetches the channel's autobleem-psc-<version>.tar.gz into ROOT/System/Updates/,
// checks it against the site's sha256 and writes pending.json there, then leaves with MENU_OPTION_UPDATE.
// rc/selection.sh copies this program to tmpfs - the update replaces Autobleem/bin/autobleem, where it
// lives - and runs it with ROOT=/media. It is the PC installer's own update, autobleem-core's InstallerJob
// with the package given: what the package ships is replaced (the launcher, the emulators, the rc scripts,
// the console tools, the shipped themes, Docs/), everything of the user's stays (games, saves, memory cards,
// config.ini's settings, RetroArch and its cores, the cover databases). UpdateRoms for the stick's PC side
// comes from the site through the download command (curl, from the kernel payload). Its record is
// ROOT/System/Logs/installer.log; stdout goes to update.log (selection.sh). Exit 0 when the stick is updated
// (System/Updates is removed then), 1 when not - the stick then keeps what it had, apart from a failure
// half way through the unpacking, which the next update or the PC installer repairs.
#include "installer/command_downloader.h"
#include "installer/installer_job.h"

#include <ableem/engine/filesystem.h>
#include <ableem/engine/log.h>
#include <ableem/engine/update_catalog.h>

#include <cstdio>
#include <cstring>
#include <iostream>
#include <string>

using namespace std;
using ableem::DirEntry;
using ableem::PendingUpdate;

namespace {

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
    // curl from the AutoBleem kernel's payload: fail on an HTTP error, follow redirects, give up on a stall
    string download = "curl -sfL --connect-timeout 20 --speed-time 60 --speed-limit 1024 -o \"%o\" \"%u\"";
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
    if (!pending.load(updates + "/pending.json") || pending.autobleemFile.empty()) {
        cout << "abupdate: nothing to install - no AutoBleem package in " << updates << "/pending.json" << endl;
        return 1;
    }
    const string package =
        pending.autobleemFile[0] == '/' ? pending.autobleemFile : updates + "/" + pending.autobleemFile;
    if (!DirEntry::exists(package)) {
        cout << "abupdate: " << package << " is not there" << endl;
        return 1;
    }
    cout << "abupdate: AutoBleem " << pending.autobleemVersion << " from " << package << " onto " << root << endl;

    options.root = root;
    options.packageFile = package;
    options.channel.clear(); // the package is given - no channel to fetch it from
    options.coversJapan = options.coversUsa = options.coversPal = false; // the stick has its own
    options.retroarch = options.bios = options.samples = false;
    options.scratchDir = "/tmp/abupdate"; // not on the stick being replaced
    CommandDownloader downloader(download);
    Printer printer;
    string error;
    if (!InstallerJob::run(options, downloader, printer, []() { return false; }, error)) {
        cout << "abupdate: FAILED: " << error << endl;
        return 1;
    }
    DirEntry::removeDirAndContents(updates);
    cout << "abupdate: the stick is AutoBleem " << pending.autobleemVersion << " now" << endl;
    return 0;
}
