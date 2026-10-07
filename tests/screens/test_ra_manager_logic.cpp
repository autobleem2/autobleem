//
// ra_manager_logic.h (evoui/screens/, pure): which menu rows the RetroArch manager adds on each platform and in each
// RetroArch state, the panel's rows and state lines, the questions and the progress panel's texts.
//
#include "doctest/doctest.h"

#include "ra_manager_logic.h"

#include "core/services/platform_config.h"

#include <string>

using namespace RaManager;
using Inspection = RaJobService::Inspection;
using Status = RaJobService::Status;

namespace {

Inspection installed(const std::string &version = "v1.22.2-3", const std::string &latest = "v1.22.2-3") {
    Inspection s;
    s.ready = true;
    s.installed = true;
    s.version = version;
    s.latestVersion = latest;
    s.sizeBytes = 845300000;
    return s;
}

Inspection absent() {
    Inspection s;
    s.ready = true;
    return s;
}

bool platformHasRunner(const char *name) {
    return !PlatformConfig::load(std::string(AB_PLATFORM_DIR) + "/" + name + ".ini").retroarchJobCommand.empty();
}

} // namespace

TEST_CASE("the menu rows: a runner on the platform shows them, RetroArch's presence picks the Quick menu's") {
    // the shipped platform files: the three targets that install RetroArch themselves have a runner, a dev host none
    CHECK(systemMenuRowShown(platformHasRunner("psc")));
    CHECK(systemMenuRowShown(platformHasRunner("rpi")));
    CHECK(systemMenuRowShown(platformHasRunner("pcusb")));
    CHECK_FALSE(systemMenuRowShown(platformHasRunner("pc")));

    SUBCASE("System menu: the row follows the runner, not RetroArch's presence (it removes and updates too)") {
        CHECK(systemMenuRowShown(true));
        CHECK_FALSE(systemMenuRowShown(false));
    }
    SUBCASE("Quick menu: Install RetroArch... only while RetroArch is absent, and only with a runner") {
        CHECK(quickMenuInstallRowShown(true, false));
        CHECK_FALSE(quickMenuInstallRowShown(true, true)); // where "RetroArch cores" shows instead
        CHECK_FALSE(quickMenuInstallRowShown(false, false));
        CHECK_FALSE(quickMenuInstallRowShown(false, true));
    }
}

TEST_CASE("the panel's rows") {
    SUBCASE("while the check runs there are none") {
        CHECK(panelRows(Inspection()).empty());
        CHECK(stateLines(Inspection()) == std::vector<std::string>{"Checking..."});
    }
    SUBCASE("not installed: Install, on with a network, greyed without") {
        std::vector<Row> rows = panelRows(absent());
        REQUIRE(rows.size() == 1);
        CHECK(rows[0].kind == RowKind::Install);
        CHECK(rows[0].enabled);

        Inspection offline = absent();
        offline.networkUp = false;
        rows = panelRows(offline);
        REQUIRE(rows.size() == 1);
        CHECK_FALSE(rows[0].enabled);
        CHECK(rows[0].description == "Needs a network connection");
    }
    SUBCASE("installed and current: Remove and the BIOS row, no Update") {
        const std::vector<Row> rows = panelRows(installed());
        REQUIRE(rows.size() == 2);
        CHECK(rows[0].kind == RowKind::Remove);
        CHECK(rows[1].kind == RowKind::RemoveBios);
        CHECK(rows[0].enabled);
        CHECK(rows[1].enabled);
    }
    SUBCASE("a newer version in the catalog: Update first, named") {
        const std::vector<Row> rows = panelRows(installed("v1.22.2-3", "v1.22.2-6"));
        REQUIRE(rows.size() == 3);
        CHECK(rows[0].kind == RowKind::Update);
        CHECK(rows[0].enabled);
        CHECK(rows[0].description == "Update to v1.22.2-6");
    }
    SUBCASE("a newer version but no network: Update greyed, Remove still works") {
        Inspection s = installed("v1.22.2-3", "v1.22.2-6");
        s.networkUp = false;
        const std::vector<Row> rows = panelRows(s);
        REQUIRE(rows.size() == 3);
        CHECK_FALSE(rows[0].enabled);
        CHECK(rows[1].enabled);
    }
    SUBCASE("no version on the stamp: nothing to compare, no Update") {
        const std::vector<Row> rows = panelRows(installed("", "v1.22.2-6"));
        CHECK(rows.size() == 2);
    }
    SUBCASE("a distribution's own program is not ours to remove") {
        Inspection s = installed("", "");
        s.systemPackage = true;
        const std::vector<Row> rows = panelRows(s);
        REQUIRE(rows.size() == 2);
        CHECK_FALSE(rows[0].enabled);
        CHECK_FALSE(rows[1].enabled);
        CHECK(rows[0].description.find("system") != std::string::npos);
    }
    SUBCASE("each row names its action") {
        CHECK(actionFor(RowKind::Install) == RaJobService::Action::Install);
        CHECK(actionFor(RowKind::Update) == RaJobService::Action::Update);
        CHECK(actionFor(RowKind::Remove) == RaJobService::Action::Remove);
        CHECK(actionFor(RowKind::RemoveBios) == RaJobService::Action::RemoveWithBios);
    }
}

TEST_CASE("the panel's state lines") {
    SUBCASE("not installed") {
        CHECK(stateLines(absent()) == std::vector<std::string>{"Not installed"});
    }
    SUBCASE("not installed, no network: says so") {
        Inspection s = absent();
        s.networkUp = false;
        CHECK(stateLines(s) == std::vector<std::string>{"Not installed", "Needs a network connection"});
    }
    SUBCASE("installed: version and size on the stick, and that it is current") {
        CHECK(stateLines(installed()) == std::vector<std::string>{"Installed v1.22.2-3  -  845.3 MB", "Up to date"});
    }
    SUBCASE("an update is announced with its version") {
        CHECK(stateLines(installed("v1.22.2-3", "v1.22.2-6")) ==
              std::vector<std::string>{"Installed v1.22.2-3  -  845.3 MB", "Update available: v1.22.2-6"});
    }
    SUBCASE("installed with no stamp and no size: just 'Installed'") {
        Inspection s = installed("", "");
        s.sizeBytes = 0;
        CHECK(stateLines(s) == std::vector<std::string>{"Installed"});
    }
}

TEST_CASE("the question before a job names what happens to the BIOS files") {
    CHECK(confirmQuestion(RowKind::Remove).find("BIOS files stay") != std::string::npos);
    CHECK(confirmQuestion(RowKind::RemoveBios).find("delete the BIOS files") != std::string::npos);
    CHECK(confirmQuestion(RowKind::Install) != confirmQuestion(RowKind::Update));
}

TEST_CASE("the progress panel") {
    Status running;
    running.phase = RaJobService::Phase::Running;
    running.action = RaJobService::Action::Install;

    SUBCASE("before the runner has said anything: preparing, no bar movement, Stop is possible") {
        ProgressView view = progressView(running);
        CHECK(view.title == "Installing RetroArch");
        CHECK(view.detail == "Preparing");
        CHECK(view.fraction == doctest::Approx(0.0));
        CHECK(view.canStop);
        CHECK_FALSE(view.over);
    }
    SUBCASE("a phase: i/n, its title and its bytes, the bar over the whole job, the log lines") {
        running.step = 3;
        running.steps = 4;
        running.title = "Downloading cores";
        running.done = 50000000;
        running.total = 100000000;
        running.log = {"a", "b"};
        const ProgressView view = progressView(running);
        CHECK(view.detail == "(3/4) Downloading cores  50.0 MB / 100.0 MB");
        CHECK(view.fraction == doctest::Approx(0.625));
        CHECK(view.log.size() == 2);
    }
    SUBCASE("a title the launcher does not know is shown as it is") {
        CHECK(phaseTitle("Unpacking the moon") == "Unpacking the moon");
        CHECK(phaseTitle("Finishing") == "Finishing");
    }
    SUBCASE("the title follows the action") {
        running.action = RaJobService::Action::Update;
        CHECK(progressView(running).title == "Updating RetroArch");
        running.action = RaJobService::Action::RemoveWithBios;
        CHECK(progressView(running).title == "Removing RetroArch");
    }
    SUBCASE("a stop was asked: it says so and Stop is no longer offered") {
        running.stopping = true;
        const ProgressView view = progressView(running);
        CHECK(view.detail == "Stopping...");
        CHECK_FALSE(view.canStop);
    }
    SUBCASE("done") {
        Status done = running;
        done.phase = RaJobService::Phase::Succeeded;
        ProgressView view = progressView(done);
        CHECK(view.over);
        CHECK(view.title == "RetroArch is installed");
        done.action = RaJobService::Action::Remove;
        CHECK(progressView(done).title == "RetroArch is removed");
        done.action = RaJobService::Action::Update;
        CHECK(progressView(done).title == "RetroArch is updated");
    }
    SUBCASE("stopped: it can be started again") {
        Status stopped = running;
        stopped.phase = RaJobService::Phase::Stopped;
        const ProgressView view = progressView(stopped);
        CHECK(view.over);
        CHECK(view.title == "Stopped");
        CHECK(view.detail.find("again") != std::string::npos);
    }
    SUBCASE("failed: why, from the exit code, else from the runner's last line") {
        Status failed = running;
        failed.phase = RaJobService::Phase::Failed;
        failed.exitCode = 4;
        failed.failure = RaJobService::Failure::NoSpace;
        ProgressView view = progressView(failed);
        CHECK(view.over);
        CHECK(view.title == "The installation failed");
        CHECK(view.detail == "Not enough free space on the stick");

        failed.exitCode = 3;
        failed.failure = RaJobService::Failure::NoNetwork;
        CHECK(progressView(failed).detail == "No network connection");

        failed.exitCode = 1;
        failed.failure = RaJobService::Failure::Other;
        failed.log = {"fetching", "the catalog is unreadable"};
        CHECK(progressView(failed).detail == "the catalog is unreadable");
        failed.log.clear();
        CHECK(progressView(failed).detail == "No reason given");

        failed.action = RaJobService::Action::RemoveWithBios;
        CHECK(progressView(failed).title == "The removal failed");
    }
}
