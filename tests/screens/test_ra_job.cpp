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
