//
// Flasher: everything ABFlashKit does to the console - the recovery backup, the validation, the recovery
// flag, the kernel image, the payload, the reboot. ConsoleFlasher is the real thing (dd, the firmware's
// image_verify_tool, systemctl), FakeFlasher a dev host where every step takes a moment and succeeds
// without touching a device. FlashKitActions sequences them; main() picks one.
//
#pragma once

#include "lboot_backup.h"

#include <functional>
#include <string>
#include <vector>

//******************
// Flasher
//******************
class Flasher {
public:
    virtual ~Flasher() = default;
    using Progress = std::function<void(const std::string &)>; // a status line per step

    // another custom firmware's service is installed (BleemSync, Project Eris): flashing over it is refused
    virtual bool hasForeignFirmware() = 0;

    // the backup: the partitions zipped to `path`, then the recovery trailer
    virtual bool createBackup(const std::vector<LbootBackup::Partition> &partitions, const std::string &path,
                              const Progress &progress) = 0;
    // Sony's image_verify_tool accepts the backup
    virtual bool validateBackup(const std::string &path) = 0;
    // the backup unpacked for LbootBackup::inspect(); false when it could not be
    virtual bool extractBackup(const std::string &path, const std::string &dir) = 0;

    // <kernelDir>/boot.img is there and its md5 is the one in <kernelDir>/boot.md5
    virtual bool validateKernel(const std::string &kernelDir) = 0;
    // the bootloader's recovery flag, <kernelDir>/recovery-{on,off}.img into the MISC partition
    virtual void setRecoveryMode(bool on, const std::string &kernelDir) = 0;
    // <kernelDir>/boot.img into the BOOTIMG1 partition
    virtual void flashKernel(const std::string &kernelDir) = 0;
    // <kernelDir>/install_payload.sh - the AutoBleem rootfs overlay onto the data partition
    virtual void installPayload(const std::string &kernelDir) = 0;

    virtual void sync() = 0;
    // stops the launcher and the compositor and reboots; the tool does not return from it on the console
    virtual void reboot() = 0;
};

//******************
// ConsoleFlasher
//******************
class ConsoleFlasher : public Flasher {
public:
    bool hasForeignFirmware() override;
    bool createBackup(const std::vector<LbootBackup::Partition> &partitions, const std::string &path,
                      const Progress &progress) override;
    bool validateBackup(const std::string &path) override;
    bool extractBackup(const std::string &path, const std::string &dir) override;
    bool validateKernel(const std::string &kernelDir) override;
    void setRecoveryMode(bool on, const std::string &kernelDir) override;
    void flashKernel(const std::string &kernelDir) override;
    void installPayload(const std::string &kernelDir) override;
    void sync() override;
    void reboot() override;

    static const char *const MiscPartition; // /dev/disk/by-partlabel/MISC
    static const char *const BootPartition; // /dev/disk/by-partlabel/BOOTIMG1
};

//******************
// FakeFlasher
//******************
// a dev host: no firmware in the way, a backup written for real (so the trailer and the md5 checks run)
// but from the files named in `partitions` - main() points those at small files under the app dir -
// the validation always passing, the flash/payload/reboot steps logged and delayed `stepDelayMs`
class FakeFlasher : public Flasher {
public:
    explicit FakeFlasher(int stepDelayMs = 1500) : stepDelayMs_(stepDelayMs) {}
    bool hasForeignFirmware() override { return false; }
    bool createBackup(const std::vector<LbootBackup::Partition> &partitions, const std::string &path,
                      const Progress &progress) override;
    bool validateBackup(const std::string &path) override;
    bool extractBackup(const std::string &path, const std::string &dir) override;
    bool validateKernel(const std::string &kernelDir) override;
    void setRecoveryMode(bool on, const std::string &kernelDir) override;
    void flashKernel(const std::string &kernelDir) override;
    void installPayload(const std::string &kernelDir) override;
    void sync() override {}
    void reboot() override;

    std::vector<std::string> log; // what it would have done, in order
    bool rebooted = false;
    bool recoveryMode = false;
    // when set, a partition's device is stood in for by <dir>/<entry>, a small file made on the spot -
    // what lets the dev host's tool write a backup of "partitions" it does not have
    std::string fakePartitionsDir;

private:
    void step(const std::string &what);
    int stepDelayMs_;
};
