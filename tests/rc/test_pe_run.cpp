//
// rc/pe_run.sh and rc/pe_env.sh: the start of a PE App - the environment built for the mod's own launch.sh, the
// dialog stand-ins' answers, the compat list's refusal, the clean-up after a normal end and after a TERM. Runs the
// real scripts with the machine's sh on a fake mod in a scratch folder (every place pe_env.sh writes is a
// variable). The bind mount needs root: unprivileged, pe_env.sh falls back to PROJECT_ERIS_PATH=<the RAM tree>,
// which is what is checked here; as root the mount itself is checked as well. Without sh (a Windows shell with no
// MSYS2 on PATH) the tests are skipped, and the TERM test needs /proc.
//
#include "doctest/doctest.h"

#include "support/temp_dir.h"
#include "core/services/system.h"
#include "core/main.h"

#include <algorithm>
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

// a fake mod: what its launch.sh does is written into out.txt, one fact per line
const char *const FakeLaunch =
    "#!/bin/sh\n"
    "source \"/var/volatile/project_eris.cfg\" 2>/dev/null || source \"$PE_VOLATILE/project_eris.cfg\"\n"
    "OUT=\"$APP_OUT\"\n"
    "cd \"$PE_VOLATILE/launchtmp\" || exit 90\n"
    "echo \"pwd=$(pwd -P)\" >> $OUT\n"
    "echo \"project_eris_path=$PROJECT_ERIS_PATH\" >> $OUT\n"
    "echo \"mountpoint=$MOUNTPOINT\" >> $OUT\n"
    "echo \"log_path=$RUNTIME_LOG_PATH\" >> $OUT\n"
    "echo \"selected_theme=$SELECTED_THEME\" >> $OUT\n"
    "echo \"bin=$(ls \"$PROJECT_ERIS_PATH/bin\" | tr '\\n' ' ')\" >> $OUT\n"
    "echo \"lib=$(ls \"$PROJECT_ERIS_PATH/lib\" | tr '\\n' ' ')\" >> $OUT\n"
    "echo \"db_bytes=$(wc -c < \"$PROJECT_ERIS_PATH/etc/boot_menu/gamecontrollerdb.txt\")\" >> $OUT\n"
    "echo \"path_has_bin=$(echo \"$PATH\" | grep -c \"$PROJECT_ERIS_PATH/bin\")\" >> $OUT\n"
    "echo -n 2 > \"$PE_POWER_FLAG\"\n"
    "sdl_text_display \"Loading\" 640 120 12\n"
    "echo \"display_pid=$(cat \"$PE_RUN_DIR/sdl_display.pid\")\" >> $OUT\n"
    "sdl_input_text_display \"One d-pad?\" 640 120 12 f 255 255 255 bg XO\n"
    "echo \"answer_xo=$?\" >> $OUT\n"
    "sdl_input_text_display \"Pick\" 640 120 12 f 255 255 255 bg OST\n"
    "echo \"answer_ost=$?\" >> $OUT\n"
    "sdl_input_text_display \"Pick\" 640 120 12 f 255 255 255 bg TS\n"
    "echo \"answer_ts=$?\" >> $OUT\n"
    "sdl_input_text_display \"No letters\"\n"
    "echo \"answer_none=$?\" >> $OUT\n"
    "echo \"cfg_exists=$(ls \"$PE_VOLATILE/project_eris.cfg\")\" >> $OUT\n"
    "echo \"power_now=$(cat \"$PE_POWER_FLAG\")\" >> $OUT\n"
    "exit 7\n";

struct PeRun {
    PeRun() : tmp("pe_run") {
        tmp.makeSubDir("Apps/pe-demo");
        tmp.writeFile(
            "Apps/pe-demo/launcher.cfg",
            "launcher_filename=\"demo\"\r\nlauncher_title=\"Demo Game\"\r\nlauncher_publisher=\"Someone\"\r\n");
        tmp.writeFile("Apps/pe-demo/app.ini", "Title=Demo Game\nExec.psc=run.sh\nStartup=run.sh\nCategory=PE\n");
        tmp.writeFile("Apps/pe-demo/launch.sh", FakeLaunch);
        // the libs pack is NOT installed on this stick (no applib): gl4es comes with the launcher, rc/pe/lib
        tmp.makeSubDir("sdl");
        tmp.writeFile("sdl/libSDL2-2.0.so.0", "sdl");
        tmp.makeSubDir("power");
        tmp.writeFile("power/disable", "1");
        tmp.makeSubDir("vol");
        // the trimmed pad table the package carries, as a stand-in beside the scripts is not needed: pe_env.sh reads
        // PE_RC_DIR/pe_gamecontrollerdb.txt, so the scripts are copied together with one
        tmp.makeSubDir("Autobleem/rc/pe/lib");
        for (const char *f :
             {"pe_run.sh", "pe_env.sh", "app_env.sh", "app_resolve.sh", "pe_compat.ini", "pe/sdl_display",
              "pe/sdl_text_display", "pe/sdl_input_text_display", "pe/lib/libGL.so.1", "pe/lib/libGLU.so.1"}) {
            REQUIRE(DirEntry::copy(string(AB_RC_DIR) + "/" + f, tmp.at(string("Autobleem/rc/") + f)));
        }
        tmp.writeFile("Autobleem/rc/pe_gamecontrollerdb.txt",
                      "030000004c050000da0c000011010000,Pad,a:b2,platform:Linux\n");
    }

    // the shell prelude: every place of the run moved into the scratch folder
    string env() const {
        const string r = slashes(tmp.path());
        return "export AB_ROOT='" + r + "' AB_RUNTIME_DIR='" + r + "/rt' AB_LOG_DIR='" + r + "/rt/logs'\n" +
               "export PE_TREE='" + r + "/pe' PE_VOLATILE='" + r + "/vol' PE_POWER_FLAG='" + r +
               "/power/disable' PE_MOUNT_POINT='" + r + "/media/project_eris' PE_APPLIB='" + r +
               "/applib' PE_SDL_DIR='" + r + "/sdl'\n" + "export AB_APP_VIRTUAL_PAD=0 APP_OUT='" + r + "/out.txt'\n" +
               "export HOME='" + r + "/home'\n";
    }

    vector<string> run(const string &body) {
        tmp.writeFile("driver.sh", env() + body);
        return System::execUnixCommandLines("sh \"" + slashes(tmp.at("driver.sh")) + "\"");
    }

    string out(const string &key) const {
        string text = "\n" + tmp.readFile("out.txt");
        string::size_type at = text.find("\n" + key + "=");
        if (at == string::npos)
            return "<missing>";
        string::size_type from = at + key.size() + 2;
        return text.substr(from, text.find('\n', from) - from);
    }

    TempDir tmp;
};

} // namespace

TEST_CASE("pe_run.sh builds the environment, runs the mod's launch.sh unchanged, answers its dialogs and cleans up") {
    if (!haveSh()) {
        MESSAGE("no sh on this machine - pe_run.sh is not run");
        return;
    }
    PeRun pe;
    const string root = slashes(pe.tmp.path());
    const string before = pe.tmp.readFile("Apps/pe-demo/launch.sh");
    vector<string> lines = pe.run("sh \"$AB_ROOT/Autobleem/rc/pe_run.sh\" \"$AB_ROOT/Apps/pe-demo\"\necho rc=$?\n");
    REQUIRE_FALSE(lines.empty());
    CHECK(lines.back() == "rc=7"); // the mod's own exit status

    // the mod saw its world
    const string path = pe.out("project_eris_path");
    CHECK((path == root + "/media/project_eris" || path == root + "/pe/project_eris"));
    CHECK(pe.out("pwd") == slashes(pe.tmp.at("Apps/pe-demo"))); // launchtmp is the App's folder
    CHECK(pe.out("mountpoint") == "/media");
    CHECK(pe.out("log_path") == root + "/rt/logs/pe");
    CHECK(pe.out("selected_theme") == "modmyclassic");
    CHECK(pe.out("bin") == "sdl_display sdl_input_text_display sdl_text_display ");
    CHECK(pe.out("lib") == "libGL.so.1 libGLU.so.1 libSDL2-2.0.so.0 "); // gl4es without the libs pack, and our SDL2
    CHECK(pe.out("db_bytes") != "<missing>");
    CHECK(pe.out("path_has_bin") == "1");
    CHECK(pe.out("power_now") == "2"); // the mod's own write is the mod's
    // the dialog stand-ins: fixed answers, the first allowed letter
    CHECK(pe.out("answer_xo") == "100");
    CHECK(pe.out("answer_ost") == "101");
    CHECK(pe.out("answer_ts") == "103");
    CHECK(pe.out("answer_none") == "100");
    CHECK(pe.out("display_pid") != "<missing>");

    // the mod's own file is the byte-identical one, and nothing is left behind
    CHECK(pe.tmp.readFile("Apps/pe-demo/launch.sh") == before);
    CHECK_FALSE(DirEntry::exists(pe.tmp.at("vol/launchtmp")));
    CHECK_FALSE(DirEntry::exists(pe.tmp.at("vol/project_eris.cfg")));
    CHECK_FALSE(DirEntry::exists(pe.tmp.at("pe")));
    CHECK(pe.tmp.readFile("power/disable") == "1"); // as before the run
    CHECK(DirEntry::exists(pe.tmp.at("Apps/pe-demo/app.ini")));
    string log = pe.tmp.readFile("rt/logs/pe/pe_run.log");
    CHECK(log.find("starting demo") != string::npos);
    CHECK(log.find("no libGL") == string::npos); // gl4es was found with no libs pack
    // the pad trail: written while it is true, it outlives the RAM tree
    CHECK(log.find("pad: LD_PRELOAD=") != string::npos);
    CHECK(log.find("pad: AB_PAD_DEFAULTS=") != string::npos);
    CHECK(log.find("pad: abpad.state: ") != string::npos);
    CHECK(log.find("pad: remap files bound over by abpad: none") != string::npos);
    CHECK(pe.tmp.readFile("rt/logs/pe/dialogs.log").find("sdl_text_display: Loading") != string::npos);
}

TEST_CASE("pe_run.sh puts the power flag back to what it was, even when the mod left a 2") {
    if (!haveSh()) {
        MESSAGE("no sh on this machine - pe_run.sh is not run");
        return;
    }
    PeRun pe;
    pe.tmp.writeFile("power/disable", "0");
    pe.run("sh \"$AB_ROOT/Autobleem/rc/pe_run.sh\" \"$AB_ROOT/Apps/pe-demo\"\n");
    CHECK(pe.tmp.readFile("power/disable") == "0");
    pe.tmp.writeFile("power/disable", "2"); // left by a run that was killed: not "before"
    pe.run("sh \"$AB_ROOT/Autobleem/rc/pe_run.sh\" \"$AB_ROOT/Apps/pe-demo\"\n");
    CHECK(pe.tmp.readFile("power/disable") == "1");
}

TEST_CASE("pe_run.sh: pad output = the user's choice, else the App's PadMode, else pe_compat.ini, else psc-kernel") {
    if (!haveSh()) {
        MESSAGE("no sh on this machine - pe_run.sh is not run");
        return;
    }
    const string run = "sh \"$AB_ROOT/Autobleem/rc/pe_run.sh\" \"$AB_ROOT/Apps/pe-demo\"\n";
    auto trail = [](PeRun &pe) {
        string log = pe.tmp.readFile("rt/logs/pe/pe_run.log");
        size_t at = log.rfind("pad: mode: ");
        return (at == string::npos) ? string("<none>") : log.substr(at, log.find('\n', at) - at);
    };

    SUBCASE("nothing says: the console's own pad as a real device") {
        PeRun pe;
        pe.run(run);
        CHECK(trail(pe).find("AB_APP_PAD_MODE=psc-kernel ") != string::npos);
        CHECK(trail(pe).find("AB_PAD_KERNEL=psc") != string::npos);
    }
    SUBCASE("the shim is still a choice") {
        PeRun pe;
        pe.tmp.writeFile("Apps/pe-demo/app.ini", "PadMode=psc\n");
        pe.run(run);
        CHECK(trail(pe).find("AB_APP_PAD_MODE=psc ") != string::npos);
        const string line = trail(pe);
        const string shim = "AB_PAD_VIRTUAL=psc AB_PAD_KERNEL="; // the shim's answer, no kernel pad
        CHECK(line.size() >= shim.size());
        CHECK(line.compare(line.size() - min(line.size(), shim.size()), string::npos, shim) == 0);
    }
    SUBCASE("the App's PadMode") {
        PeRun pe;
        pe.tmp.writeFile("Apps/pe-demo/app.ini", "PadMode=x360\n");
        pe.run(run);
        CHECK(trail(pe).find("AB_APP_PAD_MODE=x360 ") != string::npos);
    }
    SUBCASE("the launcher's value wins over the App's") {
        PeRun pe;
        pe.tmp.writeFile("Apps/pe-demo/app.ini", "PadMode=x360\n");
        pe.run("AB_APP_PAD_MODE=psc\nexport AB_APP_PAD_MODE\n" + run);
        CHECK(trail(pe).find("AB_APP_PAD_MODE=psc ") != string::npos);
    }
    SUBCASE("a kernel mode asks abpadd for a device and gives the shim no answer") {
        PeRun pe;
        pe.run("AB_APP_PAD_MODE=x360-kernel\nexport AB_APP_PAD_MODE\n" + run);
        CHECK(trail(pe).find("AB_APP_PAD_MODE=x360-kernel ") != string::npos);
        CHECK(trail(pe).find("AB_PAD_VIRTUAL= AB_PAD_KERNEL=x360") != string::npos); // no shim answer: the kernel's
    }
}

TEST_CASE("pe_run.sh refuses a launcher the compat list blocks: a message for the launcher, a line in the log") {
    if (!haveSh()) {
        MESSAGE("no sh on this machine - pe_run.sh is not run");
        return;
    }
    PeRun pe;
    pe.tmp.writeFile("Apps/pe-demo/launcher.cfg",
                     "launcher_filename=\"backupinternallaunch\"\nlauncher_title=\"Backup Internal\"\n");
    vector<string> lines =
        pe.run("sh \"$AB_ROOT/Autobleem/rc/pe_run.sh\" \"$AB_ROOT/Apps/pe-demo\" 2>/dev/null\necho rc=$?\n");
    REQUIRE_FALSE(lines.empty());
    CHECK(lines.back() == "rc=1");
    CHECK(pe.tmp.readFile("rt/app-message.txt") == "Backup Internal\ndeletes the console's own games\n");
    CHECK(pe.tmp.readFile("rt/logs/pe/pe_run.log").find("refused backupinternallaunch") != string::npos);
    CHECK_FALSE(DirEntry::exists(pe.tmp.at("out.txt"))); // launch.sh never ran
    CHECK_FALSE(DirEntry::exists(pe.tmp.at("vol/launchtmp")));

    // a launcher the list only skips (proc_pe's business, not a refusal here), and an unknown one, run
    pe.tmp.writeFile("Apps/pe-demo/launcher.cfg", "launcher_filename=\"retroarch\"\n");
    lines = pe.run("sh \"$AB_ROOT/Autobleem/rc/pe_run.sh\" \"$AB_ROOT/Apps/pe-demo\"\necho rc=$?\n");
    CHECK(lines.back() == "rc=7");
}

#ifndef _WIN32
TEST_CASE("pe_run.sh: a TERM (the Reset button) stops the mod and its children, and the clean-up still happens") {
    if (!haveSh() || !DirEntry::exists("/proc/self/stat")) {
        MESSAGE("no sh or no /proc here - the TERM test is skipped");
        return;
    }
    PeRun pe;
    pe.tmp.writeFile("Apps/pe-demo/launch.sh",
                     "#!/bin/sh\n"
                     "echo -n 2 > \"$PE_POWER_FLAG\"\n"
                     "sleep 300 &\n" // the game is a child of the script
                     "echo $! > \"$APP_OUT.child\"\n"
                     "wait\n");
    vector<string> lines = pe.run(
        "sh \"$AB_ROOT/Autobleem/rc/pe_run.sh\" \"$AB_ROOT/Apps/pe-demo\" &\n"
        "PID=$!\n"
        "n=0; while [ ! -f \"$APP_OUT.child\" ] && [ $n -lt 100 ]; do sleep 0.1; n=$((n+1)); done\n"
        "sleep 0.3\n"
        "kill -TERM $PID\n"
        "wait $PID\n"
        "echo rc=$?\n"
        // gone = no process, or a zombie: the killed child is reparented to PID 1, and a PID 1 that does not
        // reap (a CI container's `tail -f /dev/null`) leaves it a zombie, which kill -0 still finds. Polled
        // for up to 3 s rather than read once: the signal and the exit are not instant on a loaded runner.
        "C=$(cat \"$APP_OUT.child\")\n"
        "gone() { [ ! -r /proc/$C/stat ] || [ \"$(sed 's/.*) //' /proc/$C/stat 2>/dev/null | cut -c1)\" = Z ]; }\n"
        "n=0; while ! gone && [ $n -lt 30 ]; do sleep 0.1; n=$((n+1)); done\n"
        "if gone; then echo child=gone; else echo child=alive; fi\n");
    REQUIRE(lines.size() >= 2);
    CHECK(lines[lines.size() - 2] == "rc=143");
    CHECK(lines.back() == "child=gone");
    CHECK_FALSE(DirEntry::exists(pe.tmp.at("vol/launchtmp")));
    CHECK_FALSE(DirEntry::exists(pe.tmp.at("pe")));
    CHECK(pe.tmp.readFile("power/disable") == "1");
}
#endif

TEST_CASE(
    "pe_compat.ini names the three launchers that delete the console's games as blocked, in the contract's form") {
    string ini;
    REQUIRE(DirEntry::readFile(string(AB_RC_DIR) + "/pe_compat.ini", ini));
    for (const char *name : {"backupinternallaunch", "editinternallaunch", "restoreinternallaunch"}) {
        INFO(name);
        CHECK(ini.find(string("[") + name + "]\nblock=1\nreason=deletes the console's own games\n") != string::npos);
    }
    for (const char *name : {"bootmenu", "folder", "gamemanager", "pehome", "retroarch"})
        CHECK(ini.find(string("[") + name + "]\nskip=1\n") != string::npos);
    for (const char *name : {"openbor", "doom", "amiberry"})
        CHECK(ini.find(string("[") + name + "]\nskip=1\nreason=AutoBleem has its own App\n") != string::npos);
    CHECK(ini.find('\r') == string::npos);
}
