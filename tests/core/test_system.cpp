//
// System: the OS helpers that can be exercised without a console - the disk space and the shell.
//
#include "doctest/doctest.h"

#include "../support/env_fixture.h"
#include "../support/temp_dir.h"

#include "core/services/system.h"

#include <cstdint>
#include <string>

using std::string;

TEST_CASE("diskSpace reports the filesystem a path is on, and nothing for a path that is not there") {
    TempDir tmp("disk_space");
    uint64_t freeBytes = 0, totalBytes = 0;
    REQUIRE(System::diskSpace(tmp.path(), freeBytes, totalBytes));
    CHECK(totalBytes > 0);
    CHECK(freeBytes <= totalBytes);

    CHECK_FALSE(System::diskSpace(tmp.at("no/such/dir"), freeBytes, totalBytes));
}

TEST_CASE("getAvailableSpace is the USB root's filesystem as 'N GB / M GB (P%)'") {
    TempDir tmp("available_space");
    EnvFixture env;
    ableem::Environment::setUsbRoot(tmp.path());
    const string text = System::getAvailableSpace();
    CHECK(text.find(" GB / ") != string::npos);
    CHECK(text.find("%)") != string::npos);

    ableem::Environment::setUsbRoot(tmp.at("gone"));
    CHECK(System::getAvailableSpace().empty());
}

TEST_CASE("runShellCommand runs a line through the shell and returns its exit status") {
    TempDir tmp("shell");
#ifdef _WIN32
    const string touch = "echo hi > \"" + tmp.at("made.txt") + "\"";
#else
    const string touch = "echo hi > '" + tmp.at("made.txt") + "'";
#endif
    CHECK(System::runShellCommand(touch) == 0);
    CHECK(ableem::DirEntry::exists(tmp.at("made.txt")));
    CHECK(System::runShellCommand("exit 3") == 3);
}
