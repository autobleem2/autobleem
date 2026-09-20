//
// The Windows side of the installer: the removable drives, formatting one, downloading over WinINet.
// Everything here is #ifdef _WIN32; the job (core/) knows none of it.
//
#pragma once

#include "core/installer_job.h"

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#ifdef _WIN32

//******************
// RemovableDrive
//******************
struct RemovableDrive {
    std::string root;       // "F:/"
    std::string letter;     // "F:"
    std::string label;      // the volume label, "" when none
    std::string fileSystem; // "FAT32", "exFAT", "NTFS", "" (unformatted)
    uint64_t sizeBytes = 0;
    uint64_t freeBytes = 0;
    bool ready = true; // false when the drive has no readable volume (unformatted, no media)

    std::string describe() const; // "F: SONY (FAT32, 58.4 GB, 42.5 GB free)"
};

// the removable drives only - fixed disks, network drives and optical drives are not offered
std::vector<RemovableDrive> listRemovableDrives();

// formats a removable drive with Windows' format.com ("FAT32" or "exFAT"), quick, labelled `label`.
// FAT32 above 32 GB is what format.com refuses: fat32format.exe next to the installer (Ridgecrop's
// free tool) is used for that when it is there. Every line the tool prints goes to `say`; false with the
// reason when it fails.
bool formatDrive(const std::string &letter, const std::string &fileSystem, const std::string &label,
                 const std::function<void(const std::string &)> &say, std::string &error);

// the stick's label as the console expects it: SONY. Sets it when it is anything else; false with the
// reason when Windows refuses (the label is then reported, not fatal)
bool ensureVolumeLabel(const std::string &root, const std::string &label, std::string &error);

// the directory of the running program, forward slashes, no trailing slash
std::string programDirectory();

//******************
// WinInetDownloader
//******************
class WinInetDownloader : public Downloader {
public:
    bool fetch(const std::string &url, const std::string &destFile, const Progress &progress,
               std::string &error) override;
};

// attaches to the console the program was started from, when there is one, so --quiet's lines show up
// although the exe is a GUI-subsystem program
void attachParentConsole();

#endif
