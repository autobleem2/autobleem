//
// rc/app_resolve.sh against AppManifest: the shell copy of the multi-platform App rule (what a run.sh started
// by hand goes by) must give the launcher's answers (docs/app-format-plan.md). Runs the script with the
// machine's sh; without one (a Windows shell with no MSYS2 on PATH) the comparison is skipped, not failed.
//
#include "doctest/doctest.h"

#include "support/temp_dir.h"
#include "core/services/app_manifest.h"
#include "core/services/system.h"
#include "core/main.h"

#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using namespace std;

namespace {

bool haveSh() {
    static int have = -1;
    if (have < 0)
        have = Strings::trim(System::execUnixCommand("sh -c \"echo ok\"")) == "ok" ? 1 : 0;
    return have == 1;
}

string slashes(string path) {
    for (char &c : path)
        if (c == '\\')
            c = '/';
    return path;
}

// what the script resolves in `folder` for `keys`, as "exec|key|args|lib|<env names...>" lines
struct ShellAnswer {
    bool resolved = false;
    string exec, key, args, lib;
    map<string, string> env;
};

ShellAnswer runScript(const TempDir &tmp, const string &folder, const vector<string> &keys,
                      const vector<string> &envNames) {
    string keyList;
    for (const string &k : keys)
        keyList += (keyList.empty() ? "" : " ") + k;
    string script = "AB_APP_DIR='" + folder + "'\nAB_ROOT='" + folder + "'\nAB_PLATFORM_KEYS='" + keyList +
                    "'\nexport AB_APP_DIR AB_ROOT AB_PLATFORM_KEYS\n. '" + string(AB_RC_DIR) +
                    "/app_resolve.sh'\n"
                    "if ab_resolve_app; then\n"
                    "  printf 'exec=%s\\nkey=%s\\nargs=%s\\nlib=%s\\n' \"$AB_APP_EXEC\" \"$AB_APP_KEY\" "
                    "\"$AB_APP_ARGS\" \"$AB_APP_LIB\"\n";
    for (const string &name : envNames)
        script += "  printf 'env." + name + "=%s\\n' \"$" + name + "\"\n";
    script += "else\n  echo none\nfi\n";
    {
        ofstream out(tmp.at("driver.sh"), ios::binary);
        out << script;
    }
    ShellAnswer answer;
    for (string line : System::execUnixCommandLines("sh \"" + slashes(tmp.at("driver.sh")) + "\"")) {
        line = Strings::trim(line);
        if (line == "none")
            return answer;
        string::size_type eq = line.find('=');
        if (eq == string::npos)
            continue;
        string name = line.substr(0, eq), value = line.substr(eq + 1);
        answer.resolved = true;
        if (name == "exec")
            answer.exec = value;
        else if (name == "key")
            answer.key = value;
        else if (name == "args")
            answer.args = value;
        else if (name == "lib")
            answer.lib = value;
        else if (name.compare(0, 4, "env.") == 0)
            answer.env[name.substr(4)] = value;
    }
    return answer;
}

// the same App seen by both: the script's answer must be the manifest's
void compare(const TempDir &tmp, const string &appDir, const vector<string> &keys,
             const vector<string> &envNames = {}) {
    AppManifest::Options options; // no .exe guessing: the script is for the Linux targets
    AppManifest m = AppManifest::load(appDir, "app.ini", keys, options);
    ShellAnswer sh = runScript(tmp, appDir, keys, envNames);
    INFO("app " << appDir);
    REQUIRE(sh.resolved == m.runnable());
    if (!m.runnable())
        return;
    CHECK(sh.exec == m.program);
    CHECK(sh.key == m.key);
    CHECK(sh.args == m.args);
    CHECK(sh.lib == m.libDir);
    for (const auto &kv : m.env) {
        auto it = sh.env.find(kv.first);
        REQUIRE(it != sh.env.end());
        CHECK(it->second == kv.second);
    }
}

void app(const TempDir &tmp, const string &name, const string &ini, const vector<string> &files) {
    tmp.makeSubDir("Apps/" + name);
    tmp.writeFile("Apps/" + name + "/app.ini", ini);
    for (const string &f : files) {
        string path = "Apps/" + name + "/" + f;
        tmp.makeSubDir(path.substr(0, path.find_last_of('/')));
        tmp.writeFile(path, "x");
    }
}

} // namespace

TEST_CASE("the console's and the Linux payload's App scripts are one file each") {
    for (const char *name : {"app_env.sh", "app_resolve.sh", "app_run.sh", "ab_log.sh"}) {
        INFO(name);
        CHECK(DirEntry::filesAreIdentical(string(AB_RC_DIR) + "/" + name, string(AB_LINUX_RC_DIR) + "/" + name));
    }
}

TEST_CASE("rc/app_resolve.sh gives the launcher's answer for every kind of app.ini") {
    if (!haveSh()) {
        MESSAGE("no sh on this machine - the shell resolver is not compared");
        return;
    }
    TempDir tmp("app_resolve");
    const string root = slashes(tmp.path());
    auto dir = [&root](const string &name) { return root + "/Apps/" + name; };

    // the pattern, the specific key first when both are there, the generic one otherwise
    app(tmp, "Pattern", "[app]\nExec=bin/{key}/tyrian\n", {"bin/linux-arm64/tyrian", "bin/psc/tyrian"});
    compare(tmp, dir("Pattern"), {"rpi64", "linux-arm64"});
    compare(tmp, dir("Pattern"), {"psc"});
    compare(tmp, dir("Pattern"), {"win", "windows-x86_64"}); // nothing here

    // Exec.<key> beats the pattern; comments, blanks, CRLF and mixed case in the keys
    app(tmp, "Own",
        "[App]\r\n"
        "# a comment\r\n"
        "  EXEC.psc =  odd/tyrian-psc   # trailing comment\r\n"
        "Exec = bin/{key}/tyrian\r\n",
        {"odd/tyrian-psc", "bin/rpi/tyrian"});
    compare(tmp, dir("Own"), {"psc"});
    compare(tmp, dir("Own"), {"rpi", "linux-armhf"});

    // Args/Lib/Env, plain and per key
    app(tmp, "Full",
        "Exec=bin/{key}/game\n"
        "Args=--data data -f\n"
        "Args.rpi=--data data --rpi\n"
        "Lib=lib/{key}\n"
        "Env=SDL_AUDIODRIVER=alsa;GAME_KEY={key};EMPTY=\n"
        "Env.rpi=SDL_AUDIODRIVER=pulse;EXTRA=a=b\n",
        {"bin/rpi/game", "bin/pcusb/game"});
    compare(tmp, dir("Full"), {"rpi", "linux-armhf"}, {"SDL_AUDIODRIVER", "GAME_KEY", "EMPTY", "EXTRA"});
    compare(tmp, dir("Full"), {"pcusb", "linux-i386"}, {"SDL_AUDIODRIVER", "GAME_KEY", "EMPTY"});

    // an empty Exec.<key> is "nothing for this key", not a fall back to the pattern
    app(tmp, "Empty", "Exec.psc=\nExec=bin/{key}/game\n", {"bin/psc/game"});
    compare(tmp, dir("Empty"), {"psc"});

    // an App of the old kind: nothing for the script to resolve (its run.sh runs its own binary)
    app(tmp, "Old", "Title=Old\nStartup=run.sh\n", {"run.sh"});
    CHECK_FALSE(runScript(tmp, dir("Old"), {"psc"}, {}).resolved);
}

// C1 (autobleem-main docs/todo.md): a read-only stick used to leave every App without a pad -
// abpadd's own stdout/stderr redirection into an unwritable log dir stopped the shell from starting
// it at all - and an App with its own LD_LIBRARY_PATH handed abpadd a foreign SDL2 that does not know
// our gamecontrollerdb.txt. Both are exercised against a fake abpadd that records what it actually got.
TEST_CASE("rc/app_env.sh starts abpadd through an unwritable log dir, with the launcher's own "
          "library path and gamecontrollerdb regardless of the App's own") {
    if (!haveSh()) {
        MESSAGE("no sh on this machine - app_env.sh is not exercised");
        return;
    }
    TempDir tmp("app_env_abpad");
    const string root = slashes(tmp.path());
    const string appDir = root + "/Apps/Foreign";
    const string resourcesDb = root + "/Autobleem/bin/autobleem/gamecontrollerdb.txt";
    const string abpadDir = root + "/Autobleem/bin/abpad";
    const string resultFile = root + "/result.txt";
    const string blockedLogDir = root + "/BlockedLogDir"; // a plain file where a dir is expected
    const string foreignLib = root + "/ForeignLib";       // what abpadd must NOT run with

    tmp.makeSubDir("Apps/Foreign");
    tmp.makeSubDir("Autobleem/bin/autobleem");
    tmp.makeSubDir("Autobleem/bin/abpad");
    tmp.makeSubDir("ForeignLib");
    tmp.writeFile("Autobleem/bin/autobleem/gamecontrollerdb.txt", "fake db\n");
    tmp.writeFile("BlockedLogDir", "x"); // mkdir -p on this path must fail
    // a fake abpadd - a shell script standing in for the real binary - that just records the
    // environment it was actually started with, instead of touching any real pad
    tmp.writeFile("Autobleem/bin/abpad/abpadd", "#!/bin/sh\n"
                                                "touch /tmp/abpad.state\n"
                                                "{\n"
                                                "  echo \"LD_LIBRARY_PATH=$LD_LIBRARY_PATH\"\n"
                                                "  echo \"AB_PAD_DB=$AB_PAD_DB\"\n"
                                                "} > \"$AB_TEST_RESULT\"\n");
    tmp.writeFile("Autobleem/bin/abpad/libabpad.so", "x");
    System::execUnixCommand(("chmod +x '" + slashes(abpadDir) + "/abpadd'").c_str());
    System::execUnixCommand("rm -f /tmp/abpad.state");

    string script = "AB_APP_DIR='" + appDir +
                    "'\n"
                    "AB_ROOT='" +
                    root +
                    "'\n"
                    "AB_LOG_DIR='" +
                    blockedLogDir +
                    "'\n"
                    "AB_APP_LIB='" +
                    foreignLib +
                    "'\n"
                    "AB_TEST_RESULT='" +
                    resultFile +
                    "'\n"
                    "export AB_APP_DIR AB_ROOT AB_LOG_DIR AB_APP_LIB AB_TEST_RESULT\n"
                    ". '" +
                    string(AB_RC_DIR) +
                    "/app_env.sh'\n"
                    "ab_waited=0\n"
                    "while [ ! -f \"$AB_TEST_RESULT\" ] && [ $ab_waited -lt 50 ]; do\n"
                    "  ab_waited=$((ab_waited + 1))\n"
                    "  sleep 0.1\n"
                    "done\n";
    // app_env.sh exports LD_PRELOAD once abpadd is up, which is exactly what abpadd's own preload is
    // for - but it means nothing else must run in this same shell afterwards: our fake libabpad.so is
    // not a real shared object, and a shell that honours LD_PRELOAD (as MSYS2's does) fails every
    // process it forks from here on. So the driver does nothing but source app_env.sh and wait; the
    // result is read back from the file directly, not from this script's own stdout.
    ofstream(tmp.at("driver.sh"), ios::binary) << script;
    System::execUnixCommandLines("sh \"" + slashes(tmp.at("driver.sh")) + "\"");
    System::execUnixCommand("rm -f /tmp/abpad.state"); // a fresh process - not the driver's, LD_PRELOAD and all

    bool gotResult = DirEntry::exists(resultFile);
    string ldLibraryPath, padDb;
    if (gotResult) {
        istringstream in(tmp.readFile("result.txt"));
        string line;
        while (getline(in, line)) {
            line = Strings::trim(line);
            if (line.compare(0, 16, "LD_LIBRARY_PATH=") == 0)
                ldLibraryPath = line.substr(16);
            else if (line.compare(0, 10, "AB_PAD_DB=") == 0)
                padDb = line.substr(10);
        }
    }

    // it started at all - despite AB_LOG_DIR being unwritable
    REQUIRE(gotResult);
    // never the App's own library path
    CHECK(ldLibraryPath.find(foreignLib) == string::npos);
    // the resources dir's gamecontrollerdb.txt is there, whatever the App's own env carried
    CHECK(padDb.find(resourcesDb) != string::npos);
}
