//
// Flasher: the console's commands, and the dev host's stand-in.
//
#include "flasher.h"
#include "core/main.h"
#include "core/services/system.h"

#include <ableem/engine/log.h>
#include <ableem/engine/md5.h>
#include <ableem/engine/zip_archive.h>
#include <ableem/engine/zip_writer.h>

#include <chrono>
#include <fstream>
#include <thread>

using namespace std;

const char *const ConsoleFlasher::MiscPartition = "/dev/disk/by-partlabel/MISC";
const char *const ConsoleFlasher::BootPartition = "/dev/disk/by-partlabel/BOOTIMG1";

namespace {
// the backup as both flashers make it: every partition streamed into the zip, then the trailer
bool writeBackup(const vector<LbootBackup::Partition> &partitions, const string &path,
                 const Flasher::Progress &progress) {
    bool ok = false;
    {
        ableem::ZipWriter zip;
        ok = zip.open(path);
        for (size_t i = 0; ok && i < partitions.size(); i++) {
            progress(_("Zipping:") + " " + partitions[i].entry + "   " + _("Please wait..."));
            ok = zip.addFile(partitions[i].device, partitions[i].entry);
        }
        ok = ok && zip.close();
    }
    ok = ok && LbootBackup::appendSignature(path);
    // a half-written backup must not be taken for one next time: it is what a restore would use
    if (!ok)
        DirEntry::removeFile(path);
    return ok;
}

// the first 32 characters of <kernelDir>/boot.md5 - a bare hash, or md5sum's "<hash>  boot.img"
string expectedKernelMd5(const string &kernelDir) {
    ifstream in(kernelDir + sep + "boot.md5");
    string line;
    getline(in, line);
    return line.size() >= 32 ? line.substr(0, 32) : "";
}

bool kernelIsValid(const string &kernelDir) {
    string image = kernelDir + sep + "boot.img";
    if (!DirEntry::exists(image))
        return false;
    string expected = expectedKernelMd5(kernelDir);
    if (expected.empty())
        return false;
    string actual = ableem::Md5::ofFile(image);
    if (actual != expected) {
        PLOG_WARNING << "Kernel image md5 " << actual << " is not the expected " << expected;
    }
    return actual == expected;
}
} // namespace

//*******************************
// ConsoleFlasher
//*******************************
bool ConsoleFlasher::hasForeignFirmware() {
    return DirEntry::exists("/usr/bin/bleemsync_service") ||
           DirEntry::exists("/etc/systemd/system/project_eris.service");
}

bool ConsoleFlasher::createBackup(const vector<LbootBackup::Partition> &partitions, const string &path,
                                  const Progress &progress) {
    return writeBackup(partitions, path, progress);
}

bool ConsoleFlasher::validateBackup(const string &path) {
    // the firmware's own checker; anything with "fail" in its output is a no
    string result = System::execUnixCommand(("image_verify_tool " + path + " /tmp/validateResult.txt").c_str());
    return result.find("fail") == string::npos;
}

bool ConsoleFlasher::extractBackup(const string &path, const string &dir) {
    DirEntry::removeDirAndContents(dir);
    DirEntry::createDir(dir);
    return ableem::ZipArchive::extract(path, dir);
}

bool ConsoleFlasher::validateKernel(const string &kernelDir) {
    return kernelIsValid(kernelDir);
}

void ConsoleFlasher::setRecoveryMode(bool on, const string &kernelDir) {
    string image = kernelDir + sep + (on ? "recovery-on.img" : "recovery-off.img");
    PLOG_INFO << "Recovery mode " << (on ? "on" : "off");
    System::execUnixCommand(("dd if=" + image + " of=" + MiscPartition).c_str());
}

void ConsoleFlasher::flashKernel(const string &kernelDir) {
    PLOG_INFO << "Flashing the kernel image";
    System::execUnixCommand(("dd if=" + kernelDir + sep + "boot.img of=" + BootPartition).c_str());
}

void ConsoleFlasher::installPayload(const string &kernelDir) {
    PLOG_INFO << "Installing the payload";
    System::execUnixCommand(("bash " + kernelDir + sep + "install_payload.sh").c_str());
}

void ConsoleFlasher::sync() {
    System::execUnixCommand("sync");
}

void ConsoleFlasher::reboot() {
    PLOG_INFO << "Rebooting";
    System::execUnixCommand("killall -9 autobleem-gui");
    System::execUnixCommand("systemctl stop weston");
    System::execUnixCommand("systemctl reboot");
}

//*******************************
// FakeFlasher
//*******************************
void FakeFlasher::step(const string &what) {
    PLOG_INFO << "fake flasher: " << what;
    log.push_back(what);
    if (stepDelayMs_ > 0)
        this_thread::sleep_for(chrono::milliseconds(stepDelayMs_));
}

bool FakeFlasher::createBackup(const vector<LbootBackup::Partition> &partitions, const string &path,
                               const Progress &progress) {
    step("backup to " + path);
    if (fakePartitionsDir.empty())
        return writeBackup(partitions, path, progress);
    DirEntry::createDir(DirEntry::getDirNameFromPath(fakePartitionsDir));
    DirEntry::createDir(fakePartitionsDir);
    vector<LbootBackup::Partition> standIns;
    for (const LbootBackup::Partition &partition : partitions) {
        string file = fakePartitionsDir + sep + partition.entry;
        if (!DirEntry::exists(file)) {
            ofstream out(file, ios::binary);
            out << "fake " << partition.entry << " standing in for " << partition.device << "\n";
        }
        standIns.push_back({file, partition.entry});
    }
    return writeBackup(standIns, path, progress);
}

bool FakeFlasher::validateBackup(const string &path) {
    step("validate " + path);
    return true;
}

bool FakeFlasher::extractBackup(const string &path, const string &dir) {
    step("extract " + path);
    DirEntry::removeDirAndContents(dir);
    DirEntry::createDir(dir);
    return ableem::ZipArchive::extract(path, dir);
}

bool FakeFlasher::validateKernel(const string &kernelDir) {
    step("validate kernel in " + kernelDir);
    return kernelIsValid(kernelDir);
}

void FakeFlasher::setRecoveryMode(bool on, const string &) {
    step(string("recovery mode ") + (on ? "on" : "off"));
    recoveryMode = on;
}

void FakeFlasher::flashKernel(const string &kernelDir) {
    step("flash " + kernelDir + sep + "boot.img");
}

void FakeFlasher::installPayload(const string &kernelDir) {
    step("install payload from " + kernelDir);
}

void FakeFlasher::reboot() {
    step("reboot");
    rebooted = true;
}
