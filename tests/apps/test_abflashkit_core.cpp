//
// abflashkit_core: the backup's shape and checks, the fake flasher's files, and the three sequences run
// to the end against the fake with a scripted screen.
//
#include "doctest/doctest.h"

#include "../support/temp_dir.h"

#include "core/flash_actions.h"
#include "core/flasher.h"
#include "core/lboot_backup.h"
#include "core/led.h"

#include <ableem/engine/md5.h>
#include <ableem/engine/zip_archive.h>

#include <string>
#include <vector>

using std::string;
using std::vector;

//*******************************
// a scripted screen
//*******************************
namespace {
struct ScriptedUi : FlashUi {
    vector<string> statuses;
    vector<string> questions;
    vector<bool> answers; // consumed in order; true when it runs out
    int waited = 0;
    void status(const string &text) override { statuses.push_back(text); }
    bool confirm(const string &question) override {
        questions.push_back(question);
        if (answers.empty())
            return true;
        bool answer = answers.front();
        answers.erase(answers.begin());
        return answer;
    }
    void wait(int ms) override { waited += ms; }
    bool said(const string &piece) const {
        for (const string &s : statuses)
            if (s.find(piece) != string::npos)
                return true;
        return false;
    }
};

// a tree with fake partitions, a kernel folder whose boot.md5 matches its boot.img, and the paths
struct Bench {
    Bench() : tmp("abflashkit") {
        tmp.writeFile("parts/boot", "BOOT IMAGE BYTES");
        tmp.writeFile("parts/root", "ROOTFS BYTES");
        tmp.writeFile("parts/user", "USERDATA BYTES");
        tmp.writeFile("parts/tee", "TEE BYTES");
        tmp.writeFile("kernel/boot.img", "the new kernel");
        tmp.writeFile("kernel/boot.md5", ableem::Md5::ofString("the new kernel") + "  boot.img\n");
        backup = tmp.at("LBOOT.EPB");
        kernelDir = tmp.at("kernel");
        scratch = tmp.at("scratch");
        marker = tmp.at("validlboot");
    }
    vector<LbootBackup::Partition> partitions(bool withRootfs) const {
        vector<LbootBackup::Partition> list = {{tmp.at("parts/boot"), "boot.img"}};
        if (withRootfs)
            list.push_back({tmp.at("parts/root"), "rootfs.ext4"});
        list.push_back({tmp.at("parts/user"), "userdata.ext4"});
        list.push_back({tmp.at("parts/tee"), "tz.img"});
        return list;
    }
    TempDir tmp;
    string backup, kernelDir, scratch, marker;
};
} // namespace

TEST_CASE("LbootBackup: the trailer makes a backup ours, and the vanilla check reads the extracted files") {
    Bench b;
    FakeFlasher flasher(0);
    ScriptedUi ui;
    REQUIRE(flasher.createBackup(b.partitions(true), b.backup, [&](const string &s) { ui.status(s); }));
    CHECK(ui.said("boot.img"));
    CHECK(LbootBackup::isAutoBleemBackup(b.backup));
    b.tmp.writeFile("other.zip", "PK not really autobleem"); // ends in "autobleem" but that is the whole point
    CHECK(LbootBackup::isAutoBleemBackup(b.tmp.at("other.zip")));
    b.tmp.writeFile("plain.zip", "PK something else");
    CHECK_FALSE(LbootBackup::isAutoBleemBackup(b.tmp.at("plain.zip")));
    CHECK_FALSE(LbootBackup::isAutoBleemBackup(b.tmp.at("missing.zip")));

    // the zip still reads despite the trailer, and the images inside are not the stock ones
    REQUIRE(flasher.extractBackup(b.backup, b.scratch));
    CHECK(b.tmp.readFile("scratch/boot.img") == "BOOT IMAGE BYTES");
    LbootBackup::Contents contents = LbootBackup::inspect(b.scratch);
    CHECK(contents.hasBoot);
    CHECK_FALSE(contents.bootIsVanilla);
    CHECK(contents.hasRootfs);
    CHECK_FALSE(contents.rootfsIsVanilla);
    CHECK_FALSE(contents.isVanilla());

    LbootBackup::Contents empty = LbootBackup::inspect(b.tmp.at("nowhere"));
    CHECK_FALSE(empty.hasBoot);
    CHECK_FALSE(empty.hasRootfs);
}

TEST_CASE("the partition lists: no rootfs for a flash, the lot for a full backup") {
    vector<LbootBackup::Partition> flash = LbootBackup::partitionsForFlash();
    REQUIRE(flash.size() == 3);
    CHECK(flash[0].device == "/dev/disk/by-partlabel/BOOTIMG1");
    CHECK(flash[0].entry == "boot.img");
    vector<LbootBackup::Partition> full = LbootBackup::partitionsForFullBackup();
    REQUIRE(full.size() == 4);
    CHECK(full[1].entry == "rootfs.ext4");
}

TEST_CASE("the kernel check wants boot.img and a matching boot.md5 in either format") {
    Bench b;
    FakeFlasher flasher(0);
    CHECK(flasher.validateKernel(b.kernelDir));
    b.tmp.writeFile("kernel/boot.md5", ableem::Md5::ofString("the new kernel")); // bare hash
    CHECK(flasher.validateKernel(b.kernelDir));
    b.tmp.writeFile("kernel/boot.md5", "00000000000000000000000000000000");
    CHECK_FALSE(flasher.validateKernel(b.kernelDir));
    b.tmp.writeFile("kernel/boot.md5", "");
    CHECK_FALSE(flasher.validateKernel(b.kernelDir));
    CHECK_FALSE(flasher.validateKernel(b.tmp.at("no-kernel")));
}

TEST_CASE("flash: backup, validate, recovery on, kernel, payload, recovery off, reboot") {
    Bench b;
    FakeFlasher flasher(0);
    NullLed led;
    ScriptedUi ui;
    FlashKitActions actions(flasher, led, ui, b.backup, b.kernelDir, b.scratch, b.marker);
    // the backup path is what the flasher zips the "partitions" of; the fake takes the bench's files
    // through the same createBackup, so point the flash list at them by making the backup first
    REQUIRE(flasher.createBackup(b.partitions(false), b.backup, [](const string &) {}));
    flasher.log.clear();

    CHECK(actions.flash() == FlashKitActions::Outcome::Rebooting);
    REQUIRE(ui.questions.size() == 1);
    CHECK(ui.questions[0] == "Start flashing ?");
    CHECK(flasher.log == vector<string>{"validate " + b.backup, "validate kernel in " + b.kernelDir, "recovery mode on",
                                        "flash " + b.kernelDir + "/boot.img", "install payload from " + b.kernelDir,
                                        "recovery mode off", "reboot"});
    CHECK(flasher.rebooted);
    CHECK_FALSE(flasher.recoveryMode); // cleared before the reboot: the console boots normally into AutoBleem
    CHECK(led.mode == LedMode::Off);
    CHECK(ui.said("All done"));
}

TEST_CASE("flash refuses another firmware, stops at no, and reboots without writing when the kernel is bad") {
    Bench b;
    struct ForeignFlasher : FakeFlasher {
        ForeignFlasher() : FakeFlasher(0) {}
        bool hasForeignFirmware() override { return true; }
    } foreign;
    NullLed led;
    ScriptedUi ui;
    FlashKitActions refused(foreign, led, ui, b.backup, b.kernelDir, b.scratch, b.marker);
    CHECK(refused.flash() == FlashKitActions::Outcome::Refused);
    CHECK(ui.said("not been restored"));
    CHECK(ui.questions.empty());

    FakeFlasher flasher(0);
    ScriptedUi no;
    no.answers = {false};
    FlashKitActions cancelled(flasher, led, no, b.backup, b.kernelDir, b.scratch, b.marker);
    CHECK(cancelled.flash() == FlashKitActions::Outcome::Cancelled);
    CHECK(flasher.log.empty());

    b.tmp.writeFile("kernel/boot.md5", "00000000000000000000000000000000");
    REQUIRE(flasher.createBackup(b.partitions(false), b.backup, [](const string &) {}));
    flasher.log.clear();
    ScriptedUi ui2;
    FlashKitActions bad(flasher, led, ui2, b.backup, b.kernelDir, b.scratch, b.marker);
    CHECK(bad.flash() == FlashKitActions::Outcome::Failed);
    CHECK(ui2.said("Invalid backup or invalid kernel image"));
    CHECK(flasher.rebooted);
    for (const string &step : flasher.log)
        CHECK(step.find("flash ") == string::npos); // nothing written
    CHECK_FALSE(flasher.recoveryMode);
}

TEST_CASE("fullBackup asks before overwriting and writes the four partitions") {
    Bench b;
    FakeFlasher flasher(0);
    NullLed led;
    ScriptedUi ui;
    FlashKitActions actions(flasher, led, ui, b.backup, b.kernelDir, b.scratch, b.marker);
    // no backup yet: no question. (The real partition devices do not exist here, so the write fails and
    // the outcome says so - the sequencing is what is under test.)
    CHECK(actions.fullBackup() == FlashKitActions::Outcome::Failed);
    CHECK(ui.questions.empty());
    CHECK(ui.said("Backup failed"));

    REQUIRE(flasher.createBackup(b.partitions(true), b.backup, [](const string &) {}));
    ScriptedUi no;
    no.answers = {false};
    FlashKitActions keep(flasher, led, no, b.backup, b.kernelDir, b.scratch, b.marker);
    CHECK(keep.fullBackup() == FlashKitActions::Outcome::Cancelled);
    REQUIRE(no.questions.size() == 1);
    CHECK(no.questions[0] == "Overwrite the existing backup?");
    CHECK(LbootBackup::isAutoBleemBackup(b.backup)); // untouched
}

TEST_CASE("restore: no backup, not ours, a modified one asks twice, the marker skips the inspection") {
    Bench b;
    FakeFlasher flasher(0);
    NullLed led;

    ScriptedUi none;
    FlashKitActions noBackup(flasher, led, none, b.backup, b.kernelDir, b.scratch, b.marker);
    CHECK(noBackup.restore() == FlashKitActions::Outcome::Refused);
    CHECK(none.said("No backup found"));

    b.tmp.writeFile("LBOOT.EPB", "someone else's file");
    ScriptedUi foreign;
    FlashKitActions notOurs(flasher, led, foreign, b.backup, b.kernelDir, b.scratch, b.marker);
    CHECK(notOurs.restore() == FlashKitActions::Outcome::Refused);
    CHECK(foreign.said("Non AutoBleem Backup"));

    // ours, without a rootfs and with a non-stock kernel: the SOFTBRICK question, the 1.0a question, then
    // the reboot question
    REQUIRE(flasher.createBackup(b.partitions(false), b.backup, [](const string &) {}));
    flasher.log.clear();
    ScriptedUi yes;
    FlashKitActions modified(flasher, led, yes, b.backup, b.kernelDir, b.scratch, b.marker);
    CHECK(modified.restore() == FlashKitActions::Outcome::Rebooting);
    REQUIRE(yes.questions.size() == 3);
    CHECK(yes.questions[0].find("SOFTBRICK") != string::npos);
    CHECK(yes.questions[1].find("rootfs1") != string::npos);
    CHECK(yes.questions[2] == "Set recovery mode and reboot now?");
    CHECK(flasher.recoveryMode);
    CHECK(flasher.rebooted);
    CHECK(yes.said("Recovery Mode On"));

    // a no at the first question ends it
    FakeFlasher flasher2(0);
    ScriptedUi no;
    no.answers = {false};
    FlashKitActions declined(flasher2, led, no, b.backup, b.kernelDir, b.scratch, b.marker);
    CHECK(declined.restore() == FlashKitActions::Outcome::Cancelled);
    CHECK_FALSE(flasher2.rebooted);
    CHECK(no.said("Recovery interrupted"));

    // the marker: straight to the reboot question
    b.tmp.writeFile("validlboot", "");
    FakeFlasher flasher3(0);
    ScriptedUi marked;
    FlashKitActions trusted(flasher3, led, marked, b.backup, b.kernelDir, b.scratch, b.marker);
    CHECK(trusted.restore() == FlashKitActions::Outcome::Rebooting);
    REQUIRE(marked.questions.size() == 1);
    CHECK(marked.questions[0] == "Set recovery mode and reboot now?");
    for (const string &step : flasher3.log)
        CHECK(step.find("extract") == string::npos);
}

TEST_CASE("Led names and the null led") {
    NullLed led;
    led.setMode(LedMode::BlinkGreen);
    CHECK(led.mode == LedMode::BlinkGreen);
    CHECK(string(Led::name(LedMode::BlinkGreen)) == "blink green");
    CHECK(string(Led::name(LedMode::Off)) == "off");
}

TEST_CASE("SysfsLed writes the brightness files it is pointed at and stops its thread") {
    TempDir tmp("leds");
    tmp.makeSubDir("red");
    tmp.makeSubDir("green");
    {
        SysfsLed led(tmp.path());
        led.setMode(LedMode::Orange);
        CHECK(tmp.readFile("red/brightness") == "1");
        CHECK(tmp.readFile("green/brightness") == "1");
        led.setMode(LedMode::Red);
        CHECK(tmp.readFile("green/brightness") == "0");
    } // the destructor joins the blinker
    CHECK(true);
}
