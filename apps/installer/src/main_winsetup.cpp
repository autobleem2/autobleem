//
// AutoBleemWinSetup: the Windows product's data tree, filled in from the download repository - what the
// NSIS installer runs after it has put the program in place, and what "AutoBleem Setup" in the Start Menu
// runs again to add RetroArch, the BIOS files or the samples later. The program folder is the NSIS
// installer's; everything under the data folder (Documents\AutoBleem by default) is this program's.
//
//   AutoBleemWinSetup.exe [--root DIR] [--program DIR]        the window, the questions started from the options
//   AutoBleemWinSetup.exe --quiet --root DIR [--program DIR]  no window, the lines on the console it came from:
//       [--covers JUP] [--retroarch] [--bios] [--samples] [--update] [--repo URL]
//       --covers names the cover databases to fetch (J, U, P - the default is all three; "" for none)
//       --update: the program was just updated - the launcher rescans once
//
// --root defaults to the registry's DataRoot (what the installer wrote), else Documents\AutoBleem;
// --program to the folder this exe is in.
//
#include "core/windows_install_job.h"
#include "core/version.h"
#include "win32_platform.h"
#include "win32_setup_window.h"

#include "core/services/environment_setup.h"
#include "core/services/windows_host.h"

#include <ableem/engine/log.h>

#include <cstdlib>
#include <iostream>
#include <string>

using namespace std;

namespace {

//******************
// QuietListener
//******************
class QuietListener : public InstallListener {
public:
    void onPhase(int index, int total, const string &title) override {
        cout << "[" << index << "/" << total << "] " << title << endl;
    }
    void onProgress(uint64_t, uint64_t) override {}
    void onLine(const string &line) override { cout << line << endl; }
};

#ifndef _WIN32
class CommandDownloader : public Downloader {
public:
    bool fetch(const string &url, const string &destFile, const Progress &, string &error) override {
        string cmd = "curl -sfL -o \"" + destFile + "\" \"" + url + "\"";
        if (system(cmd.c_str()) == 0)
            return true;
        error = "curl failed for " + url;
        return false;
    }
};
#endif

int usage() {
    cout << "USAGE: AutoBleemWinSetup [--quiet] [--root DIR] [--program DIR] [--covers JUP] [--retroarch] [--bios]\n"
            "                         [--samples] [--update] [--repo URL]"
         << endl;
    return EXIT_FAILURE;
}

} // namespace

int main(int argc, char *argv[]) {
    bool quiet = false;
    WindowsInstallOptions options;
    string covers = "JUP";
    for (int i = 1; i < argc; i++) {
        string arg = argv[i];
        auto value = [&](string &into) {
            if (i + 1 >= argc)
                return false;
            into = argv[++i];
            return true;
        };
        if (arg == "--quiet")
            quiet = true;
        else if (arg == "--retroarch")
            options.retroarch = true;
        else if (arg == "--bios")
            options.bios = true;
        else if (arg == "--samples")
            options.samples = true;
        else if (arg == "--update")
            options.update = true;
        else if (arg == "--root" && value(options.dataRoot)) {
        } else if (arg == "--program" && value(options.programDir)) {
        } else if (arg == "--covers" && value(covers)) {
        } else if (arg == "--repo" && value(options.repoUrl)) {
        } else
            return usage();
    }
    options.coversJapan = covers.find_first_of("Jj") != string::npos;
    options.coversUsa = covers.find_first_of("Uu") != string::npos;
    options.coversPal = covers.find_first_of("Pp") != string::npos;

#ifdef _WIN32
    if (quiet)
        attachParentConsole();
    // the same facts the launcher decides its layout from
    HostFacts facts = WindowsHost::facts();
    if (options.programDir.empty())
        options.programDir = facts.programDir;
    if (options.dataRoot.empty()) {
        if (!facts.registryDataRoot.empty())
            options.dataRoot = facts.registryDataRoot;
        else if (!facts.pointerFileDataRoot.empty())
            options.dataRoot = facts.pointerFileDataRoot;
        else if (!facts.documentsDir.empty())
            options.dataRoot = facts.documentsDir + "/" + EnvironmentSetup::DefaultDataFolder;
    }
#endif
    ableem::Log::initConsoleOnly();
    PLOG_INFO << "AutoBleem setup " << Version::FULL_VERSION << ", program " << options.programDir << ", data "
              << options.dataRoot;

    if (!quiet) {
#ifdef _WIN32
        return runSetupWindow(options);
#else
        cout << "no window on this platform - use --quiet --root <dir>" << endl;
        return usage();
#endif
    }
    if (options.dataRoot.empty())
        return usage();
    QuietListener listener;
    string error;
#ifdef _WIN32
    WinInetDownloader downloader;
#else
    CommandDownloader downloader;
#endif
    bool ok = WindowsInstallJob::run(options, downloader, listener, []() { return false; }, error);
    if (!ok)
        cout << "FAILED: " << error << endl;
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
