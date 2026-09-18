//
// FlashKitActions: the sequences.
//
#include "flash_actions.h"
#include "core/main.h"

#include <ableem/engine/log.h>

#include <utility>

using namespace std;

//*******************************
// FlashKitActions::FlashKitActions
//*******************************
FlashKitActions::FlashKitActions(Flasher &flasher, Led &led, FlashUi &ui, string backupPath, string kernelDir,
                                 string scratchDir, string validMarker)
    : flasher_(flasher), led_(led), ui_(ui), backupPath_(std::move(backupPath)), kernelDir_(std::move(kernelDir)),
      scratchDir_(std::move(scratchDir)), validMarker_(std::move(validMarker)) {}

const char *FlashKitActions::name(Outcome outcome) {
    switch (outcome) {
    case Outcome::Refused:
        return "refused";
    case Outcome::Cancelled:
        return "cancelled";
    case Outcome::Failed:
        return "failed";
    case Outcome::Done:
        return "done";
    case Outcome::Rebooting:
        return "rebooting";
    }
    return "?";
}

bool FlashKitActions::backupExists() const {
    return DirEntry::exists(backupPath_);
}

// the LED off, a moment for sysfs to take it, then the reboot
void FlashKitActions::rebootNow() {
    led_.setMode(LedMode::Off);
    ui_.wait(100);
    flasher_.reboot();
}

//*******************************
// FlashKitActions::flash
//*******************************
FlashKitActions::Outcome FlashKitActions::flash() {
    if (flasher_.hasForeignFirmware()) {
        ui_.status(_("This PSC has not been restored to original condition. Unable to flash."));
        ui_.wait(4000);
        return Outcome::Refused;
    }
    if (!ui_.confirm(_("Start flashing ?")))
        return Outcome::Cancelled;

    led_.setMode(LedMode::BlinkGreen);
    ui_.status(_("Creating backup...."));
    // an existing backup is kept: it is the one the recovery would restore, made before any flash
    if (!backupExists()) {
        if (!flasher_.createBackup(LbootBackup::partitionsForFlash(), backupPath_,
                                   [this](const string &text) { ui_.status(text); })) {
            ui_.status(_("Invalid backup or invalid kernel image"));
            ui_.wait(3000);
            led_.setMode(LedMode::Green);
            return Outcome::Failed;
        }
    }
    flasher_.sync();

    ui_.status(_("Validating backup...   Please wait..."));
    bool ok = flasher_.validateBackup(backupPath_) && flasher_.validateKernel(kernelDir_);
    if (ok) {
        led_.setMode(LedMode::Red);
        ui_.wait(2000);
        ui_.status(_("Setting recovery mode: on"));
        flasher_.setRecoveryMode(true, kernelDir_);
        ui_.status(_("Flashing KERNEL IMAGE"));
        flasher_.flashKernel(kernelDir_);
        ui_.status(_("Updating payload"));
        flasher_.installPayload(kernelDir_);
        led_.setMode(LedMode::Green);
        ui_.status(_("Setting recovery mode: off"));
        flasher_.setRecoveryMode(false, kernelDir_);
        ui_.wait(2000);
        ui_.status(_("All done - when the screen goes black replace power cord"));
    } else {
        ui_.status(_("Invalid backup or invalid kernel image"));
    }
    // the 2020 tool rebooted either way: a validated console is about to run its new kernel, a failed
    // one has had nothing written and comes back as it was
    ui_.wait(3000);
    rebootNow();
    return ok ? Outcome::Rebooting : Outcome::Failed;
}

//*******************************
// FlashKitActions::fullBackup
//*******************************
FlashKitActions::Outcome FlashKitActions::fullBackup() {
    // new since the port: the previous backup is what a restore would use, so overwriting it is asked
    if (backupExists() && !ui_.confirm(_("Overwrite the existing backup?")))
        return Outcome::Cancelled;
    led_.setMode(LedMode::BlinkGreen);
    ui_.status(_("Creating backup...."));
    if (backupExists())
        DirEntry::removeFile(backupPath_);
    bool ok = flasher_.createBackup(LbootBackup::partitionsForFullBackup(), backupPath_,
                                    [this](const string &text) { ui_.status(text); });
    flasher_.sync();
    led_.setMode(LedMode::Green);
    ui_.status(ok ? _("Backup complete") : _("Backup failed"));
    ui_.wait(2000);
    return ok ? Outcome::Done : Outcome::Failed;
}

//*******************************
// FlashKitActions::restore
//*******************************
FlashKitActions::Outcome FlashKitActions::restore() {
    if (!backupExists()) {
        ui_.status(_("No backup found - Recovery locked"));
        ui_.wait(3000);
        return Outcome::Refused;
    }
    if (!LbootBackup::isAutoBleemBackup(backupPath_)) {
        ui_.status(_("Non AutoBleem Backup found. Please use valid AB backup"));
        ui_.wait(3000);
        return Outcome::Refused;
    }
    // the marker file skips the inspection (a backup already known to be good)
    if (!DirEntry::exists(validMarker_)) {
        ui_.status(_("Checking LBOOT for vanilla kernel. Decompressing..."));
        ui_.wait(1000);
        if (!flasher_.extractBackup(backupPath_, scratchDir_)) {
            ui_.status(_("Recovery interrupted"));
            ui_.wait(3000);
            return Outcome::Failed;
        }
        LbootBackup::Contents contents = LbootBackup::inspect(scratchDir_);
        if (contents.hasBoot) {
            PLOG_INFO << "backup: boot " << (contents.bootIsVanilla ? "vanilla" : "modified") << ", rootfs "
                      << (contents.hasRootfs ? (contents.rootfsIsVanilla ? "vanilla" : "modified") : "absent");
            if (!contents.isVanilla() &&
                !ui_.confirm(_(
                    "The LBOOT does not contain CLEAN PSC firmware. You may SOFTBRICK your console. Are you sure?"))) {
                ui_.status(_("Recovery interrupted"));
                ui_.wait(3000);
                return Outcome::Cancelled;
            }
            if (!contents.hasRootfs &&
                !ui_.confirm(_("The LBOOT does not contain rootfs1. This may be ABFK1.0a backup. Are you sure?"))) {
                ui_.status(_("Recovery interrupted"));
                ui_.wait(3000);
                return Outcome::Cancelled;
            }
        }
    }
    // new since the port: the reboot into recovery is the point of no return, so it is asked
    if (!ui_.confirm(_("Set recovery mode and reboot now?"))) {
        ui_.status(_("Recovery interrupted"));
        ui_.wait(3000);
        return Outcome::Cancelled;
    }
    led_.setMode(LedMode::Red);
    flasher_.setRecoveryMode(true, kernelDir_);
    ui_.status(_("Recovery Mode On"));
    ui_.wait(3000);
    rebootNow();
    return Outcome::Rebooting;
}
