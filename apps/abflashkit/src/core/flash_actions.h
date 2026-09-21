//
// FlashKitActions: the three things ABFlashKit does, step by step, as the 2020 tool did them - Flash
// Kernel (back up, validate, set the recovery flag, write the kernel and the payload, clear the flag,
// reboot), Full backup, Restore Mode (check the backup is ours and vanilla, set the recovery flag,
// reboot so that Sony's recovery restores from it). The screen is behind FlashUi (status lines, the
// confirmations, the pauses), the console behind Flasher and Led, so a test can run all three to the end.
//
#pragma once

#include "flasher.h"
#include "led.h"

#include <string>

//******************
// FlashUi
//******************
class FlashUi {
public:
    virtual ~FlashUi() = default;
    virtual void status(const std::string &text) = 0; // a line on the screen
    virtual bool confirm(const std::string &question) = 0;
    virtual void wait(int ms) = 0; // a pause the user is meant to read the status in
    // how far the action is, done of total steps (the backup's partitions count one each); total 0 = no bar
    virtual void progress(int /*done*/, int /*total*/) {}
};

//******************
// FlashKitActions
//******************
class FlashKitActions {
public:
    enum class Outcome {
        Refused,   // the console is not in a state for it (another firmware, no backup, not our backup)
        Cancelled, // the user said no at a confirmation
        Failed,    // a step failed; nothing irreversible happened
        Done,      // finished, the tool goes on (a backup)
        Rebooting  // finished with the reboot issued (a flash, a restore)
    };

    // `backupPath` is <usb root>/LBOOT.EPB, `kernelDir` the tool's kernel/ folder, `scratchDir` where a
    // backup is unpacked for inspection (/tmp/lbootzip on the console), `validMarker` the file whose
    // presence skips that inspection (<usb root>/validlboot)
    FlashKitActions(Flasher &flasher, Led &led, FlashUi &ui, std::string backupPath, std::string kernelDir,
                    std::string scratchDir, std::string validMarker);

    Outcome flash();
    Outcome fullBackup();
    Outcome restore();

    static const char *name(Outcome outcome);

private:
    bool backupExists() const;
    void rebootNow();

    Flasher &flasher_;
    Led &led_;
    FlashUi &ui_;
    std::string backupPath_, kernelDir_, scratchDir_, validMarker_;
};
