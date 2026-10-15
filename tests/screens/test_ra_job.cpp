//
// The RetroArch install/remove job (PLATFORM-24): evoui/ra_job.h (pure - the progress line's parser, the status
// machine, the runner's command line, the panel's rows) and RaJobService on real child processes (tools/
// fake_ra_runner.sh, and tiny scripts that end badly).
//
#include "doctest/doctest.h"

#include "ra_job.h"
#include "ra_job_service.h"
#include "support/temp_dir.h"

#include <chrono>
#include <cstdlib>
#include <thread>

#ifndef _WIN32
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

using namespace rajob;

TEST_CASE("the progress line: a phase") {
    const Line line = parseLine("phase 3/9|download-cores|1048576|20971520\n");
    REQUIRE(line.kind == Line::Kind::Phase);
    CHECK(line.progress.phase == 3);
    CHECK(line.progress.phases == 9);
    CHECK(line.progress.key == "download-cores");
    CHECK(line.progress.done == 1048576);
    CHECK(line.progress.total == 20971520);
    CHECK(phaseCounter(line.progress) == "3/9");
}

TEST_CASE("the progress line: the old 4-field form has no unit, item or package count") {
    const Line line = parseLine("phase 3/9|download-cores|10|20\n");
    REQUIRE(line.kind == Line::Kind::Phase);
    CHECK(line.progress.unit == "");
    CHECK(line.progress.item == "");
    CHECK(line.progress.itemIndex == 0);
    CHECK(line.progress.itemCount == 0);
}

TEST_CASE("the progress line: the 8-field form carries unit, package name and package i of n") {
    const Line line = parseLine("phase 3/9|download-cores|1048576|20971520|bytes|cores-psc-20261003.tar.gz|2|5\n");
    REQUIRE(line.kind == Line::Kind::Phase);
    CHECK(line.progress.phase == 3);
    CHECK(line.progress.phases == 9);
    CHECK(line.progress.key == "download-cores");
    CHECK(line.progress.done == 1048576);
    CHECK(line.progress.total == 20971520);
    CHECK(line.progress.unit == "bytes");
    CHECK(line.progress.item == "cores-psc-20261003.tar.gz");
    CHECK(line.progress.itemIndex == 2);
    CHECK(line.progress.itemCount == 5);
    // an empty item and 0/0 are fine
    const Line bare = parseLine("phase 9/9|install-binary|0|2|items||0|0");
    REQUIRE(bare.kind == Line::Kind::Phase);
    CHECK(bare.progress.unit == "items");
    CHECK(bare.progress.item == "");
    CHECK(bare.progress.itemCount == 0);
    CHECK(parseLine("phase 7/9|unpack|3|900|files|cheats.zip|11|11").kind == Line::Kind::Phase);
}

TEST_CASE("the progress line: a bad unit, a package index past the count or a wrong field count is garbled") {
    CHECK(parseLine("phase 3/9|unpack|1|2|kilobytes|x|1|1").kind == Line::Kind::Garbled);
    CHECK(parseLine("phase 3/9|unpack|1|2||x|1|1").kind == Line::Kind::Garbled);
    CHECK(parseLine("phase 3/9|unpack|1|2|bytes|x|3|2").kind == Line::Kind::Garbled);
    CHECK(parseLine("phase 3/9|unpack|1|2|bytes|x|a|2").kind == Line::Kind::Garbled);
    CHECK(parseLine("phase 3/9|unpack|1|2|bytes|x|1").kind == Line::Kind::Garbled);
    CHECK(parseLine("phase 3/9|unpack|1|2|bytes|x|1|2|9").kind == Line::Kind::Garbled);
}

TEST_CASE("the progress line: CRLF and a missing newline are fine, only the first line counts") {
    CHECK(parseLine("phase 1/2|check-space|0|0\r\n").kind == Line::Kind::Phase);
    CHECK(parseLine("phase 1/2|check-space|0|0").kind == Line::Kind::Phase);
    CHECK(parseLine("end|ok\nphase 1/2|unpack|0|0\n").end == EndKind::Ok);
}

TEST_CASE("the progress line: total 0 is an indeterminate bar, done past total is a full one") {
    CHECK(fraction(parseLine("phase 1/2|unpack|5|0").progress) == -1);
    CHECK(fraction(parseLine("phase 1/2|unpack|5|10").progress) == doctest::Approx(0.5));
    CHECK(fraction(parseLine("phase 1/2|unpack|50|10").progress) == 1.0);
}

TEST_CASE("the progress line: the end lines") {
    CHECK(parseLine("end|ok").end == EndKind::Ok);
    CHECK(parseLine("end|stopped\n").end == EndKind::Stopped);
    const Line error = parseLine("end|error|no-space\n");
    CHECK(error.kind == Line::Kind::End);
    CHECK(error.end == EndKind::Error);
    CHECK(error.code == "no-space");
}

TEST_CASE("the progress line: anything else is garbled") {
    for (const char *text : {"",
                             "\n",
                             "garbage",
                             "phase",
                             "phase 1/2|unpack|1",
                             "phase 1/2|unpack|1|2|3",
                             "phase 0/2|unpack|1|2",
                             "phase 3/2|unpack|1|2",
                             "phase a/b|unpack|1|2",
                             "phase 1/2||1|2",
                             "phase 1/2|unpack|-1|2",
                             "phase 1/2|unpack|x|2",
                             "end",
                             "end|",
                             "end|fine",
                             "end|error",
                             "end|error|",
                             "end|ok|extra",
                             "Phase 1/2|unpack|1|2",
                             "phase 1/2|unpack|99999999999999999999|2",
                             "\xff\xfe\x01"})
        CHECK(parseLine(text).kind == Line::Kind::Garbled);
}

TEST_CASE("the error codes: the contract's six, anything else is other") {
    for (const char *code : {"no-network", "no-space", "download", "checksum", "apt-provided", "other"})
        CHECK(knownCode(code) == code);
    CHECK(knownCode("disk-on-fire") == "other");
    CHECK(knownCode("") == "other");
}

TEST_CASE("the phase keys are the contract's eleven, in a run's order") {
    CHECK(phaseKeys().size() == 11);
    CHECK(phaseKeys().front() == "check-space");
    CHECK(phaseKeys()[9] == "install-binary");
    CHECK(phaseKeys().back() == "remove-files");
}

TEST_CASE("status: idle until begun, a phase line moves the progress") {
    JobState job;
    CHECK(job.state == State::Idle);
    job.apply("phase 1/3|check-space|0|0"); // not running: ignored
    CHECK(job.progress.phase == 0);
    job.begin();
    CHECK(job.state == State::Running);
    job.apply("phase 2/3|download-retroarch|10|100");
    CHECK(job.progress.phase == 2);
    CHECK(job.progress.key == "download-retroarch");
    job.apply("phase 3/3|unpack|0|0");
    CHECK(job.progress.phase == 3);
    CHECK(job.state == State::Running);
}

TEST_CASE("status: a run that skips keys (the appliance never emits download-apps / download-assets)") {
    JobState job;
    job.begin();
    // n is fixed per run: 7 phases, the counter walks 1..7 although two keys of phaseKeys() never come
    const char *lines[] = {"phase 1/7|check-space|0|0",     "phase 2/7|download-retroarch|5|10",
                           "phase 3/7|download-cores|5|10", "phase 4/7|download-libs|5|10",
                           "phase 5/7|unpack|0|0",          "phase 6/7|merge-config|0|0",
                           "phase 7/7|install-binary|0|0"};
    int expected = 1;
    for (const char *text : lines) {
        job.apply(text);
        CHECK(job.progress.phase == expected);
        CHECK(job.progress.phases == 7);
        CHECK(phaseCounter(job.progress) == std::to_string(expected) + "/7");
        expected++;
    }
    CHECK(job.progress.key == "install-binary");
    CHECK(job.state == State::Running);
    job.apply("end|ok");
    job.childExited(0);
    CHECK(job.state == State::Ok);
}

TEST_CASE("status: end|ok, end|stopped and end|error|<code> are decided when the child has gone") {
    JobState ok;
    ok.begin();
    ok.apply("end|ok");
    CHECK(ok.state == State::Running); // the child is not reaped yet
    ok.childExited(0);
    CHECK(ok.state == State::Ok);

    JobState stopped;
    stopped.begin();
    stopped.apply("end|stopped");
    stopped.childExited(3);
    CHECK(stopped.state == State::Stopped);

    for (const char *code : {"no-network", "no-space", "download", "checksum", "apt-provided", "other"}) {
        JobState error;
        error.begin();
        error.apply(std::string("end|error|") + code);
        error.childExited(1);
        CHECK(error.state == State::Error);
        CHECK(error.code == code);
    }

    JobState unknown;
    unknown.begin();
    unknown.apply("end|error|made-up");
    unknown.childExited(1);
    CHECK(unknown.code == "other");
}

TEST_CASE("status: the file's end line beats the exit code") {
    JobState job;
    job.begin();
    job.apply("end|error|checksum");
    job.childExited(0);
    CHECK(job.state == State::Error);
    CHECK(job.code == "checksum");
}

TEST_CASE("status: a missing or garbled file leaves the job running until the child is gone") {
    JobState job;
    job.begin();
    job.apply("");
    job.apply("garbage|garbage");
    job.apply("phase x/y|unpack|1|2");
    CHECK(job.state == State::Running);
    CHECK(job.progress.phase == 0);
    job.apply("phase 2/4|unpack|1|2");
    job.apply("phase !!!"); // a bad line does not erase the good one
    CHECK(job.progress.phase == 2);
}

TEST_CASE("status: a runner that dies without an end line is an error 'other'") {
    JobState killed;
    killed.begin();
    killed.apply("phase 2/4|download-cores|5|10");
    killed.childExited(-1); // a signal
    CHECK(killed.state == State::Error);
    CHECK(killed.code == "other");

    JobState crashed;
    crashed.begin();
    crashed.childExited(139);
    CHECK(crashed.state == State::Error);
    CHECK(crashed.code == "other");

    JobState noExec;
    noExec.begin();
    noExec.childExited(127);
    CHECK(noExec.state == State::Error);
    CHECK(noExec.code == "other");
}

TEST_CASE("status: exit 0 without a proper end|ok is an error 'other', never Done") {
    for (const char *text : {"", "garbage|garbage", "phase 9/9|install-binary|0|0", "end|", "end|error|"}) {
        JobState job; // the file missing is the same as a file the parser rejects
        job.begin();
        job.apply(text);
        job.childExited(0);
        CHECK(job.state == State::Error);
        CHECK(job.code == "other");
    }
    JobState missing;
    missing.begin();
    missing.childExited(0);
    CHECK(missing.state == State::Error);
    CHECK(missing.code == "other");

    JobState ok;
    ok.begin();
    ok.apply("phase 1/1|unpack|1|2");
    ok.apply("end|ok");
    ok.childExited(0);
    CHECK(ok.state == State::Ok);
}

TEST_CASE("the bar is per phase: total 0 is a pulse, never a carried-over fraction") {
    const Progress known = parseLine("phase 8/9|merge-config|6|10").progress;
    const Bar a = barFor(known, 500);
    CHECK_FALSE(a.indeterminate);
    CHECK(a.value == doctest::Approx(0.6));

    const Progress zero = parseLine("phase 9/9|install-binary|0|0").progress;
    for (unsigned age : {0u, 300u, 1000u, 1700u, 2300u}) {
        const Bar b = barFor(zero, age);
        CHECK(b.indeterminate);
        CHECK(b.value >= 0);
        CHECK(b.value <= 1);
    }
    CHECK(barFor(zero, 0).value == 0.0); // a new phase starts the sweep from the start
    CHECK(barFor(zero, 1000).value == 1.0);
    CHECK(barFor(zero, 1500).value == doctest::Approx(0.5));

    const Bar none = barFor(Progress(), 700); // no phase seen yet
    CHECK_FALSE(none.indeterminate);
    CHECK(none.value == 0.0);
}

TEST_CASE("the end of a run is a plain message box; no run ended shows none") {
    CHECK(boxFor(State::Ok) == EndBox::Message);
    CHECK(boxFor(State::Stopped) == EndBox::Message);
    CHECK(boxFor(State::Error) == EndBox::Message);
    CHECK(boxFor(State::Idle) == EndBox::None);
    CHECK(boxFor(State::Running) == EndBox::None);
}

TEST_CASE("status: without an end line only exit 3 / our SIGTERM say stopped") {
    JobState stopped;
    stopped.begin();
    stopped.childExited(3);
    CHECK(stopped.state == State::Stopped);
    JobState term; // our SIGTERM took it before it could write
    term.begin();
    term.stopAsked = true;
    term.childExited(-1);
    CHECK(term.state == State::Stopped);
}

TEST_CASE("status: an outcome is final") {
    JobState job;
    job.begin();
    job.apply("end|ok");
    job.childExited(0);
    job.apply("phase 1/2|unpack|0|0");
    job.childExited(1);
    CHECK(job.state == State::Ok);
}

TEST_CASE("the runner's command line follows the contract") {
    RunnerEnv psc;
    psc.psc = true;
    psc.root = "/media";
    psc.abupdate = "/media/Autobleem/bin/autobleem/abupdate";
    CHECK(runnerCommand(Op::Install, false, "/media/System/Install/p", psc) ==
          std::vector<std::string>{"/media/Autobleem/bin/autobleem/abupdate", "/media", "--retroarch", "install",
                                   "--progress", "/media/System/Install/p"});
    CHECK(runnerCommand(Op::Update, true, "p", psc) ==
          std::vector<std::string>{"/media/Autobleem/bin/autobleem/abupdate", "/media", "--retroarch", "install",
                                   "--bios", "--progress", "p"});
    CHECK(runnerCommand(Op::Remove, true, "p", psc).at(3) == "remove");

    RunnerEnv pi;
    pi.appliance = true;
    pi.applianceRunner = "autobleem-retroarch";
    CHECK(runnerCommand(Op::Remove, false, "/tmp/p", pi) ==
          std::vector<std::string>{"autobleem-retroarch", "remove", "--progress", "/tmp/p"});
    CHECK(runnerCommand(Op::Install, true, "/tmp/p", pi) ==
          std::vector<std::string>{"autobleem-retroarch", "install", "--bios", "--progress", "/tmp/p"});
}

TEST_CASE("AB_RA_RUNNER overrides the runner; no runner at all gives no command") {
    RunnerEnv dev;
    CHECK(runnerCommand(Op::Install, false, "p", dev).empty());
    dev.runnerOverride = "tools/fake_ra_runner.sh";
    CHECK(runnerCommand(Op::Install, false, "p", dev) ==
          std::vector<std::string>{"tools/fake_ra_runner.sh", "install", "--progress", "p"});
    dev.runnerOverride = "  sh   tools/fake.sh ";
    CHECK(runnerCommand(Op::Remove, false, "p", dev) ==
          std::vector<std::string>{"sh", "tools/fake.sh", "remove", "--progress", "p"});
    dev.psc = true; // the override wins on a console too
    CHECK(runnerCommand(Op::Install, false, "p", dev).front() == "sh");
}

TEST_CASE("the panel's rows: Install while absent, Update while installed, offline greys both") {
    auto enabled = [](const std::vector<Row> &rows, RowAction a) {
        for (const Row &r : rows)
            if (r.action == a)
                return r.enabled;
        FAIL("row missing");
        return false;
    };
    auto absent = panelRows(false, true, RowAction::Install);
    CHECK(enabled(absent, RowAction::Install));
    CHECK_FALSE(enabled(absent, RowAction::Update));
    CHECK(enabled(absent, RowAction::Remove));

    auto present = panelRows(true, true, RowAction::Install);
    CHECK_FALSE(enabled(present, RowAction::Install));
    CHECK(enabled(present, RowAction::Update));

    for (bool installed : {false, true}) {
        auto offline = panelRows(installed, false, RowAction::Install);
        CHECK_FALSE(enabled(offline, RowAction::Install));
        CHECK_FALSE(enabled(offline, RowAction::Update));
        CHECK(enabled(offline, RowAction::Remove)); // Remove works offline
    }
}

TEST_CASE("the BIOS row is there only while Remove is the chosen row") {
    CHECK(panelRows(true, true, RowAction::Install).size() == 3);
    CHECK(panelRows(true, true, RowAction::Update).size() == 3);
    const auto withBios = panelRows(true, true, RowAction::Remove);
    REQUIRE(withBios.size() == 4);
    CHECK(withBios.back().action == RowAction::DeleteBios);
    CHECK(panelRows(true, true, RowAction::DeleteBios).size() == 4); // the cursor on it keeps it
}

TEST_CASE("the Quick menu offers Install RetroArch only while it is absent, and where there is a runner") {
    CHECK(quickInstallRowShown(true, false));
    CHECK_FALSE(quickInstallRowShown(true, true));
    CHECK_FALSE(quickInstallRowShown(false, false));
}

TEST_CASE("sizes read as KB / MB") {
    CHECK(humanBytes(512) == "512 B");
    CHECK(humanBytes(42000) == "42 KB");
    CHECK(humanBytes(42100000) == "42.1 MB");
}

TEST_CASE("the panel's stick line: free space of the total and a thin used bar") {
    CHECK(humanSpace(18400000000ULL) == "18.4 GB");
    CHECK(humanSpace(58200000000ULL) == "58.2 GB");
    CHECK(humanSpace(512000000ULL) == "512.0 MB");
    CHECK(usedFraction(18400000000ULL, 58200000000ULL) == doctest::Approx(0.6838).epsilon(0.001));
    CHECK(usedFraction(0, 0) == 0);    // not asked / not known
    CHECK(usedFraction(100, 50) == 0); // free above total: nonsense, an empty bar
    CHECK(usedFraction(0, 1000) == 1.0);
}

TEST_CASE("the progress panel's numbers: MB, files or items, never raw bytes") {
    Amount a = amountFor(3604512, 6295495, "bytes");
    CHECK(a.kind == Amount::Kind::MB);
    CHECK(a.done == "3.6");
    CHECK(a.total == "6.3");
    a = amountFor(128000000, 289000000, "bytes");
    CHECK(a.kind == Amount::Kind::MB);
    CHECK(a.done == "128");
    CHECK(a.total == "289");
    a = amountFor(120000, 640000, "bytes");
    CHECK(a.kind == Amount::Kind::KB);
    CHECK(a.done == "120");
    CHECK(a.total == "640");
    a = amountFor(1200, 5400, "files");
    CHECK(a.kind == Amount::Kind::Files);
    CHECK(a.done == "1200");
    CHECK(a.total == "5400");
    a = amountFor(2, 4, "items");
    CHECK(a.kind == Amount::Kind::Items);
    CHECK(a.total == "4");
}

TEST_CASE("an empty unit (an old 4-field line): a download is bytes, anything else items - never a raw byte count") {
    const Progress dl = parseLine("phase 3/9|download-cores|128000000|289000000").progress;
    CHECK(effectiveUnit(dl) == "bytes");
    Amount a = amountFor(dl);
    CHECK(a.kind == Amount::Kind::MB);
    CHECK(a.done == "128");
    CHECK(a.total == "289");
    CHECK(a.done.find("000000") == std::string::npos);
    const Progress un = parseLine("phase 8/9|unpack|1200|5400").progress;
    CHECK(effectiveUnit(un) == "items");
    a = amountFor(un);
    CHECK(a.kind == Amount::Kind::Items);
    CHECK(a.done == "1200");
    // a unit the runner wrote wins over the key
    CHECK(effectiveUnit(parseLine("phase 8/9|unpack|1|2|files||0|0").progress) == "files");
    CHECK(effectiveUnit(parseLine("phase 3/9|download-cores|1|2|items|x|1|1").progress) == "items");
}

TEST_CASE("an item's '_' is shown as a space, a file name is replaced by the phase's description") {
    CHECK(displayName("Retroarch_themes") == "Retroarch themes");
    CHECK(displayName("cheats") == "cheats");
    CHECK(displayName("") == "");
    CHECK(packageWhat("Downloading emulator cores", "Retroarch_themes") == "Retroarch themes");
    CHECK(packageWhat("Downloading emulator cores", "cores-psc-20261003.tar.gz") == "Downloading emulator cores");
    CHECK(packageWhat("Downloading emulator cores", "") == "Downloading emulator cores");
}

TEST_CASE("an item that is the phase's own key is no name: the phase's description is shown") {
    CHECK(packageWhat("Checking free space", "check-space", "check-space") == "Checking free space");
    CHECK(packageWhat("Removing files", "remove-files", "remove-files") == "Removing files");
    CHECK(packageWhat("Downloading emulator cores", "cheats", "download-cores") == "cheats");
    CHECK(packageWhat("Unpacking", "check-space") == "check-space"); // no key given: the name stays
}

TEST_CASE("a size is written with the language's decimal sign") {
    CHECK(withDecimal(humanSpace(30300000000ULL), ',') == "30,3 GB");
    CHECK(withDecimal(humanSpace(30300000000ULL), '.') == "30.3 GB");
    CHECK(withDecimal(humanSpace(512000000ULL), ',') == "512,0 MB");
    CHECK(withDecimal(amountFor(3800000, 8000000, "bytes").done, ',') == "3,8");
    CHECK(withDecimal("128", ',') == "128");
    CHECK(withDecimal("", ',') == "");
}

TEST_CASE("a why line wraps to at most two lines and is cut with an ellipsis only when it must") {
    const auto measure = [](const std::string &s) { return static_cast<int>(s.size()); }; // one unit a letter
    const std::string why = "We keep your retroarch.cfg, add this version's keys and the AutoBleem theme.";
    CHECK(wrapLines("", 40, 2, measure).empty());
    CHECK(wrapLines("Short line.", 40, 2, measure) == std::vector<std::string>{"Short line."});
    const std::vector<std::string> two = wrapLines(why, 52, 2, measure);
    REQUIRE(two.size() == 2);
    CHECK(two[0] == "We keep your retroarch.cfg, add this version's keys");
    CHECK(two[1] == "and the AutoBleem theme.");
    // too long for two lines: the second ends with "..." and still fits
    const std::vector<std::string> cut = wrapLines(why, 30, 2, measure);
    REQUIRE(cut.size() == 2);
    CHECK(cut[1].size() <= 30);
    CHECK(cut[1].substr(cut[1].size() - 3) == "...");
    CHECK(wrapLines(why, 30, 3, measure).size() == 3);
    // a word wider than a line is cut in letters
    const std::vector<std::string> word = wrapLines("abcdefghijkl", 5, 3, measure);
    REQUIRE(word.size() == 3);
    CHECK(word[0] == "abcde");
    CHECK(word[2] == "kl");
}

TEST_CASE("a bar that stood still for more than a second sweeps over its real value") {
    const Progress p = parseLine("phase 3/9|download-cores|0|1|items|cores|1|9").progress;
    Bar b = barWithStall(p, 5000, 900);
    CHECK_FALSE(b.indeterminate);
    CHECK_FALSE(b.overReal);
    CHECK(b.value == 0.0);
    b = barWithStall(p, 5000, 1000); // exactly 1 s: not more
    CHECK_FALSE(b.indeterminate);
    b = barWithStall(p, 5000, 1100);
    CHECK(b.indeterminate);
    CHECK(b.overReal);
    CHECK(b.value == 0.0); // the real value stays
    CHECK(b.sweep >= 0);
    CHECK(b.sweep <= 1);
    const Progress half = parseLine("phase 3/9|download-cores|5|10|items|cores|1|9").progress;
    b = barWithStall(half, 0, 4000);
    CHECK(b.value == doctest::Approx(0.5));
    CHECK(b.sweep == doctest::Approx(sweepPosition(3000)));
    // no phase yet: nothing to sweep
    CHECK_FALSE(barWithStall(Progress(), 0, 9000).indeterminate);
    // an unknown total sweeps as before
    CHECK(barWithStall(parseLine("phase 9/9|install-binary|0|0").progress, 500, 0).indeterminate);
}

TEST_CASE("the stall clock: a change of phase, package or count starts the second again") {
    StallClock clock;
    Progress p = parseLine("phase 3/9|download-cores|10|100|bytes|a.tar.gz|1|9").progress;
    clock.feed(1000, p);
    CHECK(clock.stalledMs(1000) == 0);
    clock.feed(1900, p);
    CHECK(clock.stalledMs(1900) == 900);
    CHECK(barWithStall(p, 0, clock.stalledMs(1900)).indeterminate == false);
    clock.feed(2100, p);
    CHECK(clock.stalledMs(2100) == 1100);
    CHECK(barWithStall(p, 0, clock.stalledMs(2100)).overReal);

    p.done = 11; // done moved
    clock.feed(2200, p);
    CHECK(clock.stalledMs(2200) == 0);
    clock.feed(3000, p);
    p.item = "b.tar.gz";
    p.itemIndex = 2; // another package
    clock.feed(3300, p);
    CHECK(clock.stalledMs(3300) == 0);
    clock.feed(4000, p);
    p.phase = 4; // another phase
    clock.feed(4100, p);
    CHECK(clock.stalledMs(4100) == 0);
    clock.feed(5300, p);
    CHECK(clock.stalledMs(5300) == 1200);
    clock.reset();
    CHECK(clock.stalledMs(9000) == 0);
}

TEST_CASE("the Remove confirm lists: eight deleted with the BIOS files, seven without; six stay") {
    RemoveLists lists = removeLists(false);
    CHECK(lists.deleted.size() == 7);
    CHECK(lists.stays.size() == 6);
    lists = removeLists(true);
    REQUIRE(lists.deleted.size() == 8);
    CHECK(lists.deleted.back() == RemoveItem::Bios);
    CHECK(std::count(lists.stays.begin(), lists.stays.end(), RemoveItem::Playlists) == 1);
    CHECK(std::count(lists.stays.begin(), lists.stays.end(), RemoveItem::Covers) == 1);
}

TEST_CASE("the meter's words: speed with the language's decimal, time rounded") {
    CHECK(speedText(2400000, '.') == "2.4 MB/s");
    CHECK(speedText(2400000, ',') == "2,4 MB/s");
    CHECK(speedText(350000, '.') == "350 KB/s");
    CHECK(roughDuration(60) == "1 min");
    CHECK(roughDuration(200) == "3 min");
    CHECK(roughDuration(42) == "40 s");
    CHECK(roughDuration(1) == "5 s");
    CHECK(roughDuration(-1) == "");
}

TEST_CASE("the whole job's bar: phase i of n at fraction f is (i - 1 + f) / n") {
    CHECK(jobFraction(parseLine("phase 1/4|check-space|0|4|items||0|0").progress) == 0);
    CHECK(jobFraction(parseLine("phase 3/4|unpack|50|100|files|x.zip|1|2").progress) == doctest::Approx(0.625));
    CHECK(jobFraction(parseLine("phase 2/4|unpack|0|0").progress) == doctest::Approx(0.25)); // total unknown
    CHECK(jobFraction(Progress()) == 0);
}

TEST_CASE("the download's speed is smoothed and the time left follows it") {
    RateMeter meter;
    CHECK_FALSE(meter.known());
    CHECK(meter.secondsLeft(1000) == -1);
    meter.feed(0, 0);
    meter.feed(100, 1000000); // too short: not counted
    CHECK_FALSE(meter.known());
    meter.feed(1000, 1000000); // 1 MB/s: the first sample, no number shown yet
    CHECK(meter.bytesPerSecond() == doctest::Approx(1000000));
    CHECK_FALSE(meter.known());
    meter.feed(2000, 3000000); // 2 MB/s now, but only a quarter of the weight
    CHECK_FALSE(meter.known());
    CHECK(meter.bytesPerSecond() > 1000000);
    CHECK(meter.bytesPerSecond() < 1500000);
    meter.feed(3000, 5000000); // the third sample: now it is believed
    CHECK(meter.known());
    meter.feed(4000, 5000000 - 1); // done going back (a new package): starts again
    CHECK_FALSE(meter.known());
    CHECK(humanDuration(45) == "45 s");
    CHECK(humanDuration(200) == "3 min 20 s");
    CHECK(humanDuration(-1) == "");
}

TEST_CASE("Stop and the pre-install notice belong to Install and Update; Remove has neither") {
    CHECK(stopAllowed(Op::Install));
    CHECK(stopAllowed(Op::Update));
    CHECK_FALSE(stopAllowed(Op::Remove));
    CHECK(noticeFirst(Op::Install));
    CHECK(noticeFirst(Op::Update));
    CHECK_FALSE(noticeFirst(Op::Remove));
}

TEST_CASE("the old fake runner line and a new 8-field one move the same job") {
    JobState job;
    job.begin();
    job.apply("phase 2/3|download-cores|10|20\n");
    CHECK(job.progress.unit == "");
    job.apply("phase 2/3|download-cores|15|20|bytes|cores-psc-20261003.tar.gz|1|4\n");
    CHECK(job.progress.unit == "bytes");
    CHECK(job.progress.item == "cores-psc-20261003.tar.gz");
    job.apply("phase 2/3|download-cores|garbage\n"); // garbled: keeps what there is
    CHECK(job.progress.done == 15);
}

//
// RaJobService on real children
//
namespace {
// poll until the job leaves Running (or give up after `seconds`)
const RaJobService::Status &finish(RaJobService &job, int seconds = 20) {
    for (int i = 0; i < seconds * 100 && job.running(); i++) {
        job.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return job.poll();
}

std::string script(const TempDir &tmp, const std::string &body) {
    tmp.writeFile("runner.sh", "#!/bin/sh\n" + body);
    chmod(tmp.at("runner.sh").c_str(), 0755);
    return tmp.at("runner.sh");
}
} // namespace

#ifndef _WIN32
TEST_CASE("RaJobService: a runner that writes end|ok") {
    TempDir tmp("rajob_ok");
    const std::string file = tmp.at("p");
    const std::string runner = script(tmp, "echo 'phase 1/1|unpack|1|2' > \"$3.tmp\"; mv \"$3.tmp\" \"$3\"\nsleep 0.3\n"
                                           "echo 'end|ok' > \"$3.tmp\"; mv \"$3.tmp\" \"$3\"\nexit 0\n");
    RaJobService job;
    REQUIRE(job.startCommand({runner, "install", "--progress", file}, file));
    CHECK(job.running());
    CHECK(finish(job).state == State::Ok);
    CHECK(job.status().progress.phases == 1);
}

TEST_CASE("RaJobService: end|error|<code> and exit 1") {
    TempDir tmp("rajob_err");
    const std::string file = tmp.at("p");
    const std::string runner = script(tmp, "echo 'end|error|checksum' > \"$3\"\nexit 1\n");
    RaJobService job;
    REQUIRE(job.startCommand({runner, "install", "--progress", file}, file));
    CHECK(finish(job).state == State::Error);
    CHECK(job.status().code == "checksum");
}

TEST_CASE("RaJobService: a runner that dies without an end line, and one that is not there") {
    TempDir tmp("rajob_die");
    const std::string file = tmp.at("p");
    const std::string runner = script(tmp, "echo 'phase 1/2|download-cores|1|9' > \"$3\"\nkill -9 $$\n");
    RaJobService job;
    REQUIRE(job.startCommand({runner, "install", "--progress", file}, file));
    CHECK(finish(job).state == State::Error);
    CHECK(job.status().code == "other");
    CHECK(job.status().progress.key == "download-cores"); // what it got to

    RaJobService missing;
    REQUIRE(missing.startCommand({tmp.at("no-such-runner"), "install"}, file)); // the fork works, the exec does not
    CHECK(finish(missing).state == State::Error);
    CHECK(missing.status().code == "other");

    RaJobService none;
    CHECK_FALSE(none.startCommand({}, file));
    CHECK(none.status().state == State::Error);
}

TEST_CASE("RaJobService: the runner does not inherit the launcher's open fds") {
    TempDir tmp("rajob_fds");
    const std::string file = tmp.at("p");
    const std::string runner = script(tmp, "sleep 2\nexit 0\n");
    int fds[2];
    REQUIRE(pipe(fds) == 0); // no CLOEXEC: only the child's close keeps the write end alive
    RaJobService job;
    REQUIRE(job.startCommand({runner, "x"}, file));
    close(fds[1]);
    struct pollfd pfd = {fds[0], POLLIN, 0};
    CHECK(poll(&pfd, 1, 1000) == 1); // EOF at once; a leaked write end would hold the pipe open while the runner sleeps
    CHECK((pfd.revents & POLLHUP) != 0);
    close(fds[0]);
    job.stop();
    finish(job);
}

TEST_CASE("RaJobService: an old run's last line does not leak into the next") {
    TempDir tmp("rajob_stale");
    const std::string file = tmp.at("p");
    tmp.writeFile("p", "end|error|download\n");
    const std::string runner = script(tmp, "sleep 0.3\nexit 0\n"); // no file of its own
    RaJobService job;
    REQUIRE(job.startCommand({runner, "x"}, file));
    CHECK(finish(job).state == State::Error); // the old end|error|download is gone, and exit 0 alone is no success
    CHECK(job.status().code == "other");
}

TEST_CASE("RaJobService: exit 0 with a file that is garbled or only a phase line is an error") {
    TempDir tmp("rajob_exit0");
    const std::string file = tmp.at("p");
    for (const char *content : {"phase 9/9|install-binary|0|0", "nonsense", ""}) {
        const std::string runner = script(tmp, std::string("printf '%s\\n' '") + content + "' > \"$3\"\nexit 0\n");
        RaJobService job;
        REQUIRE(job.startCommand({runner, "install", "--progress", file}, file));
        CHECK(finish(job).state == State::Error);
        CHECK(job.status().code == "other");
    }
}

TEST_CASE("RaJobService with the fake runner: a whole install, then a remove") {
    TempDir tmp("rajob_fake");
    setenv("AB_FAKE_RA_STEPS", "2", 1);
    setenv("AB_FAKE_RA_BINARY", tmp.at("bin/retroarch").c_str(), 1);
    const std::string fake = AB_TOOLS_DIR "/fake_ra_runner.sh";
    const std::string file = tmp.at("p");

    RaJobService job;
    REQUIRE(job.startCommand({fake, "install", "--bios", "--progress", file}, file));
    CHECK(finish(job).state == State::Ok);
    CHECK(job.status().progress.phases == 10); // check-space .. install-binary with download-bios
    CHECK(job.status().progress.key == "install-binary");
    CHECK(ableem::DirEntry::exists(tmp.at("bin/retroarch")));

    job.reset();
    REQUIRE(job.startCommand({fake, "remove", "--progress", file}, file));
    CHECK(finish(job).state == State::Ok);
    CHECK(job.status().progress.key == "remove-files");
    CHECK_FALSE(ableem::DirEntry::exists(tmp.at("bin/retroarch")));
    unsetenv("AB_FAKE_RA_STEPS");
    unsetenv("AB_FAKE_RA_BINARY");
}

TEST_CASE("RaJobService with the fake runner: stop() ends with end|stopped, the next install resumes") {
    TempDir tmp("rajob_stop");
    setenv("AB_FAKE_RA_STEPS", "10", 1);
    const std::string fake = AB_TOOLS_DIR "/fake_ra_runner.sh";
    const std::string file = tmp.at("p");

    RaJobService job;
    REQUIRE(job.startCommand({fake, "install", "--progress", file}, file));
    for (int i = 0; i < 500 && job.status().progress.phase < 2;
         i++) { // let it get into the download (phase 1 takes ~2 s)
        job.poll();
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
    job.stop();
    CHECK(finish(job).state == State::Stopped);
    const int reached = job.status().progress.phase;
    CHECK(reached >= 2);

    setenv("AB_FAKE_RA_STEPS", "1", 1);
    job.reset();
    REQUIRE(job.startCommand({fake, "install", "--progress", file}, file));
    CHECK(finish(job).state == State::Ok);
    unsetenv("AB_FAKE_RA_STEPS");
}

TEST_CASE("RaJobService: destroyed while the fake runner still works (the shutdown path)") {
    TempDir tmp("rajob_shutdown");
    setenv("AB_FAKE_RA_STEPS", "10", 1);
    const std::string fake = AB_TOOLS_DIR "/fake_ra_runner.sh";
    const std::string file = tmp.at("p");
    {
        RaJobService job;
        REQUIRE(job.startCommand({fake, "install", "--progress", file}, file));
        job.poll();
        CHECK(job.running());
        // out of scope with the child alive: SIGTERM, no wait
    }
    {
        RaJobService job; // the same, after a Stop was asked and a few polls
        REQUIRE(job.startCommand({fake, "install", "--progress", file}, file));
        job.stop();
        job.poll();
    }
    {
        RaJobService idle; // never started
        CHECK_FALSE(idle.running());
        CHECK(idle.poll().state == State::Idle);
        CHECK_FALSE(idle.startCommand({}, file));
    }
    unsetenv("AB_FAKE_RA_STEPS");
}

TEST_CASE("RaJobService with the fake runner: its failure knobs") {
    TempDir tmp("rajob_knobs");
    setenv("AB_FAKE_RA_STEPS", "4", 1);
    const std::string fake = AB_TOOLS_DIR "/fake_ra_runner.sh";
    const std::string file = tmp.at("p");
    setenv("AB_FAKE_RA_FAIL", "no-space", 1);
    RaJobService job;
    REQUIRE(job.startCommand({fake, "install", "--progress", file}, file));
    CHECK(finish(job).state == State::Error);
    CHECK(job.status().code == "no-space");
    unsetenv("AB_FAKE_RA_FAIL");

    setenv("AB_FAKE_RA_DIE", "1", 1);
    job.reset();
    REQUIRE(job.startCommand({fake, "install", "--progress", file}, file));
    CHECK(finish(job).state == State::Error);
    CHECK(job.status().code == "other");
    unsetenv("AB_FAKE_RA_DIE");
    unsetenv("AB_FAKE_RA_STEPS");
}
#endif
