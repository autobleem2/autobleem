//
// InstallJobBase: what the two install jobs share - the console's (InstallerJob, a stick) and the Windows
// product's (WindowsInstallJob, the user's data tree): the phase/progress/line reporting, a download with
// the progress on the bar and the stop flag honoured, a site file checked against its published sha256,
// a catalog fetch, a tarball unpacked, and the BIOS pack (a list of files by sha256 from RetroBIOS, kept
// when already right). The small text helpers every phase uses are here too.
//
#pragma once

#include "core/installer_job.h"

#include <ableem/engine/tar_archive.h>
#include <ableem/engine/update_catalog.h>

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

std::string readText(const std::string &path);
bool writeText(const std::string &path, const std::string &text);
std::string trimmed(const std::string &s);
std::string firstLine(const std::string &text);
std::string sidecarHash(const std::string &text); // "abc  name" -> "abc" (a sha256sum sidecar)
std::string humanSize(uint64_t bytes);

//******************
// InstallJobBase
//******************
class InstallJobBase {
public:
    InstallJobBase(Downloader &downloader, InstallListener &listener, const InstallerJob::ShouldStop &shouldStop,
                   const std::string &repoUrl, const std::string &scratchDir)
        : dl(downloader), out(listener), stop(shouldStop), repoUrl(repoUrl), scratch(scratchDir) {}

protected:
    bool stopped(std::string &error);
    void phase(const std::string &title);
    void say(const std::string &line);

    // a download with the progress on the phase's bar and the stop flag honoured
    bool download(const std::string &url, const std::string &dest, std::string &error);
    // a file the site publishes with its sha256: kept when the target has that very file already, fetched
    // to .part and checked otherwise
    bool downloadVerified(const ableem::UpdateFile &file, const std::string &dest, std::string &error);
    // <repo>/<rel>, a small JSON
    bool fetchCatalog(const std::string &rel, std::string &text, std::string &error);
    // a tarball's contents under `dest`, the progress on the bar
    bool untar(const std::string &tarball, const std::string &dest, std::string &error,
               const ableem::TarArchive::Filter &filter = ableem::TarArchive::Filter(), const std::string &prefix = "");
    // the BIOS pack: <catalogRel> (a PackCatalog naming the list) -> the list, one line per file
    // "<sha256> <size> <url> <path>" -> each file under `dir`, kept when size and sha256 match, a lost one
    // reported and gone past; false only on a stop or a lost list. `only`, when given, picks the files by
    // their path in the list (a PS1-only install wants the two PlayStation files, not the ~300 MB pack)
    using BiosFilter = std::function<bool(const std::string &path)>;
    bool fetchBiosPack(const std::string &catalogRel, const std::string &dir, std::string &error,
                       const BiosFilter &only = BiosFilter());
    // the two files pcsx-ab wants under System/Bios by the console's names - romw.bin (SCPH-5501, NTSC-U)
    // for every game, romJP.bin (SCPH-5500) for a Japanese one - copied from the pack in `systemDir`
    // unless the user has put their own there (the originals stay for RetroArch's cores)
    void installPs1Bios(const std::string &systemDir, const std::string &biosDir);
    static bool isPs1BiosFile(const std::string &path);

    Downloader &dl;
    InstallListener &out;
    InstallerJob::ShouldStop stop;
    std::string repoUrl, scratch;
    std::vector<std::string> phases;
    int phaseIndex = 0;
};
