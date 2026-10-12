// abupdate (src/tools/abupdate.cpp), the built program run against a scratch root: the argument rules of the
// --retroarch mode, its progress file's last line, and the old pending.json mode that stays as it was. The download
// command is a fake (`true`); nothing here reaches a network.
#include "doctest/doctest.h"

#include "support/temp_dir.h"

#include <cstdlib>
#include <string>
#ifndef _WIN32
#include <sys/wait.h>
#endif

using namespace std;

namespace {

// abupdate with `args`, its output into <tmp>/out.txt; the exit code
int run(const TempDir &tmp, const string &args) {
    const string command = "\"" AB_ABUPDATE_PATH "\" " + args + " > \"" + tmp.at("out.txt") + "\" 2>&1";
    const int status = system(command.c_str());
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

string lastLine(const string &text) {
    string trimmed = text;
    while (!trimmed.empty() && (trimmed.back() == '\n' || trimmed.back() == '\r'))
        trimmed.pop_back();
    const size_t cut = trimmed.rfind('\n');
    return cut == string::npos ? trimmed : trimmed.substr(cut + 1);
}

string common(const TempDir &tmp) {
    return "\"" + tmp.at("root") + "\" --download true --progress \"" + tmp.at("progress.txt") + "\" ";
}

} // namespace

TEST_CASE("abupdate --retroarch: the arguments") {
    TempDir tmp("abupdate_args");
    tmp.makeSubDir("root");
    const string root = "\"" + tmp.at("root") + "\" ";
    const string progress = "--progress \"" + tmp.at("progress.txt") + "\" ";

    CHECK(run(tmp, "") == 2);
    CHECK(run(tmp, root + "--retroarch") == 2);                         // no verb
    CHECK(run(tmp, root + "--retroarch frobnicate " + progress) == 2);  // unknown verb
    CHECK(run(tmp, root + "--retroarch remove") == 2);                  // --progress is required
    CHECK(run(tmp, root + "--retroarch remove --progress") == 2);       // ... with its file
    CHECK(run(tmp, root + "--retroarch remove --nonsense " + progress) == 2);
    CHECK(run(tmp, root + "--bios --retroarch remove") == 2);           // still no --progress
    CHECK(run(tmp, root + "--bios") == 2);                              // --bios is a --retroarch flag only
}

TEST_CASE("abupdate --retroarch remove on an empty root ends ok, flags in any order") {
    TempDir tmp("abupdate_remove");
    tmp.makeSubDir("root");
    CHECK(run(tmp, common(tmp) + "--retroarch remove") == 0);
    CHECK(lastLine(tmp.readFile("progress.txt")) == "end|ok");

    CHECK(run(tmp, "\"" + tmp.at("root") + "\" --retroarch remove --bios --progress \"" + tmp.at("progress.txt") +
                       "\" --repo http://localhost:1/") == 0);
    CHECK(lastLine(tmp.readFile("progress.txt")) == "end|ok");
}

TEST_CASE("abupdate --retroarch: a missing root is end|error|other") {
    TempDir tmp("abupdate_noroot");
    const string args = "\"" + tmp.at("no/such/root") + "\" --download true --progress \"" + tmp.at("progress.txt") +
                        "\" --retroarch ";
    CHECK(run(tmp, args + "remove") == 1);
    CHECK(lastLine(tmp.readFile("progress.txt")) == "end|error|other");
    CHECK(run(tmp, args + "install") == 1);
    CHECK(lastLine(tmp.readFile("progress.txt")) == "end|error|other");
}

TEST_CASE("abupdate --retroarch leaves System/Updates alone and needs no pending.json") {
    TempDir tmp("abupdate_updates");
    tmp.makeSubDir("root/System/Updates");
    tmp.writeFile("root/System/Updates/pending.json", "{\"autobleem_file\":\"x.tar.gz\"}");
    CHECK(run(tmp, common(tmp) + "--retroarch remove") == 0);
    CHECK(tmp.readFile("root/System/Updates/pending.json") == "{\"autobleem_file\":\"x.tar.gz\"}");
}

TEST_CASE("abupdate without --retroarch: the pending.json mode is as it was") {
    TempDir tmp("abupdate_pending");
    tmp.makeSubDir("root/System/Updates");
    const string root = "\"" + tmp.at("root") + "\" ";

    CHECK(run(tmp, root + "--download true") == 1); // no pending.json
    CHECK(tmp.readFile("out.txt").find("nothing to install") != string::npos);

    tmp.writeFile("root/System/Updates/pending.json", "{\"autobleem_file\":\"missing.tar.gz\"}");
    CHECK(run(tmp, root + "--download true --repo http://localhost:1/") == 1); // the package is not there
    CHECK(tmp.readFile("out.txt").find("is not there") != string::npos);
    CHECK(!tmp.readFile("root/System/Updates/pending.json").empty()); // System/Updates stays

    CHECK(run(tmp, root + "--download") == 2);
    CHECK(run(tmp, root + "--progress x") == 2); // the new flags are not the old mode's
}
