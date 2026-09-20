//
// The AutoBleem installer for the PlayStation Classic: a stick from a PC. The stick's file system comes
// from the package next to the program (autobleem-psc-<version>.tar.gz), the rest - the cover databases,
// RetroArch and what goes with it, the BIOS files, the sample games - from the download repository, as
// asked. Run again over a stick that has AutoBleem, it updates it and leaves the user's games and data.
//
//   AutoBleemInstaller.exe                       the window: pick the stick, answer the questions, Install
//   AutoBleemInstaller.exe --quiet --drive F:    no window, the same on the console it was started from:
//       [--covers JUP] [--retroarch] [--bios] [--samples] [--package FILE] [--repo URL]
//       --covers names the cover databases to fetch (J, U, P - the default is all three; "" for none)
//
// On Windows a plain Win32 window (win32_window.cpp), statically linked: one file, nothing to install.
// Elsewhere (a Linux or macOS build) there is the --quiet path only, and a download command instead of
// WinINet.
//
#include "core/installer_job.h"
#include "core/version.h"
#include "win32_platform.h"
#include "win32_window.h"

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
//******************
// CommandDownloader
//******************
// curl, which every Linux and macOS box has
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
    cout << "USAGE: AutoBleemInstaller [--quiet --drive F: [--covers JUP] [--retroarch] [--bios] [--samples]]\n"
            "                          [--package FILE] [--repo URL]"
         << endl;
    return EXIT_FAILURE;
}

} // namespace

int main(int argc, char *argv[]) {
    bool quiet = false;
    InstallOptions options;
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
        else if (arg == "--drive" && value(options.root)) {
        } else if (arg == "--covers" && value(covers)) {
        } else if (arg == "--package" && value(options.packageFile)) {
        } else if (arg == "--repo" && value(options.repoUrl)) {
        } else
            return usage();
    }
    options.coversJapan = covers.find_first_of("Jj") != string::npos;
    options.coversUsa = covers.find_first_of("Uu") != string::npos;
    options.coversPal = covers.find_first_of("Pp") != string::npos;
    if (options.packageFile.empty()) {
#ifdef _WIN32
        options.packageFile = InstallerJob::packageNextTo(programDirectory() + "/AutoBleemInstaller.exe");
#else
        options.packageFile = InstallerJob::packageNextTo(argv[0]);
#endif
    }

#ifdef _WIN32
    if (quiet)
        attachParentConsole();
#endif
    ableem::Log::initConsoleOnly();
    PLOG_INFO << "AutoBleem installer " << Version::FULL_VERSION << ", package " << options.packageFile;

    if (!quiet) {
#ifdef _WIN32
        return runInstallerWindow(options);
#else
        cout << "no window on this platform - use --quiet --drive <root>" << endl;
        return usage();
#endif
    }
    if (options.root.empty())
        return usage();
    QuietListener listener;
    string error;
#ifdef _WIN32
    WinInetDownloader downloader;
#else
    CommandDownloader downloader;
#endif
    bool ok = InstallerJob::run(options, downloader, listener, []() { return false; }, error);
    if (!ok)
        cout << "FAILED: " << error << endl;
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
