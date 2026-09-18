//
// AbFlashKit: the program - AppBase (the main GUI's theme and language) plus the Flasher and the Led the
// actions run on, and where the backup, the kernel folder and the scratch dir are.
//
#pragma once

#include "app_base.h"
#include "core/flasher.h"
#include "core/led.h"

#include <memory>
#include <string>

//******************
// AbFlashKit
//******************
class AbFlashKit : public AppBase {
public:
    AbFlashKit(std::unique_ptr<Flasher> flasher, std::unique_ptr<Led> led, std::string scratchDir);
    static AbFlashKit &get() { return static_cast<AbFlashKit &>(AppBase::get()); }

    Flasher &flasher() { return *flasher_; }
    Led &led() { return *led_; }
    std::string backupPath() const;  // <usb root>/LBOOT.EPB
    std::string validMarker() const; // <usb root>/validlboot - skips the restore's inspection
    std::string kernelDir() const;   // <app dir>/kernel - boot.img, boot.md5, recovery-*.img, install_payload.sh
    std::string scratchDir() const { return scratchDir_; }

    int run();

private:
    std::unique_ptr<Flasher> flasher_;
    std::unique_ptr<Led> led_;
    std::string scratchDir_;
};
