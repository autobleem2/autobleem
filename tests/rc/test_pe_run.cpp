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
    "echo \"app_title=$PE_APP_TITLE\" >> $OUT\n"
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
    CHECK(pe.out("app_title") == "Demo Game"); // app.ini's Title: the text screen's title
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

// the dialog program (abdialog) stood in for by a script: it is linked into the mod's bin/ as sdl_display and
// sdl_choicedisplay, the scripts hand it the command file and the options, and its exit code is the mod's answer
const char *const FakeDialogProgram = "#!/bin/sh\n"
                                      "name=$(basename \"$0\")\n"
                                      "echo \"$name $*\" >> \"$FAKE_LOG\"\n"
                                      "case \"$name\" in\n"
                                      "sdl_choicedisplay)\n"
                                      "    while [ $# -gt 0 ]; do [ \"$1\" = -file ] && cmd=$2; shift; done\n"
                                      "    cat \"$cmd\" >> \"$FAKE_LOG.choice\"\n"
                                      "    grep -q NODISPLAY \"$cmd\" && exit 3\n"
                                      "    exit 102 ;;\n"
                                      "*)\n"
                                      "    trap 'kill $FAKE_SLEEP 2>/dev/null; exit 0' TERM INT HUP\n"
                                      "    sleep 86400 &\n"
                                      "    FAKE_SLEEP=$!\n"
                                      "    wait $FAKE_SLEEP ;;\n"
                                      "esac\n";

const char *const DialogLaunch =
    "#!/bin/sh\n"
    "source \"/var/volatile/project_eris.cfg\" 2>/dev/null || source \"$PE_VOLATILE/project_eris.cfg\"\n"
    "OUT=\"$APP_OUT\"\n"
    "echo \"bin=$(ls \"$PROJECT_ERIS_PATH/bin\" | tr '\\n' ' ')\" >> $OUT\n"
    "sdl_text_display 'Hello\\nWorld' 1 2 3 f 4 5 6 bg\n"
    "cp \"$PE_RUN_DIR/sdldisplaycmd\" \"$OUT.display\"\n"
    "echo \"display_rc=$?\" >> $OUT\n"
    "sdl_input_text_display ' ' 0 0 12 f 0 0 0 /x/doom_controller_select.png XO\n"
    "echo \"answer_dialog=$?\" >> $OUT\n"
    "sdl_input_text_display NODISPLAY 0 0 12 f 0 0 0 bg TS\n"
    "echo \"answer_no_display=$?\" >> $OUT\n"
    "exit 0\n";

TEST_CASE("pe_run.sh: the dialogs are abdialog under the 2020 names, and without a display the answer is fixed") {
    if (!haveSh()) {
        MESSAGE("no sh on this machine - pe_run.sh is not run");
        return;
    }
    PeRun pe;
    pe.tmp.writeFile("Apps/pe-demo/launch.sh", DialogLaunch);
    pe.tmp.writeFile("fake_abdialog", FakeDialogProgram);
    pe.run("chmod +x \"$AB_ROOT/fake_abdialog\"\n"
           "export PE_DIALOG_BIN=\"$AB_ROOT/fake_abdialog\" FAKE_LOG=\"$AB_ROOT/fake.log\"\n"
           "sh \"$AB_ROOT/Autobleem/rc/pe_run.sh\" \"$AB_ROOT/Apps/pe-demo\"\n");

    CHECK(pe.out("bin") == "sdl_choicedisplay sdl_display sdl_input_text_display sdl_text_display ");
    const string log = pe.tmp.readFile("fake.log");
    CHECK(log.find("sdl_display -file ") != string::npos); // the text screen, started once
    CHECK(log.find("sdl_choicedisplay -controller-db ") != string::npos);
    CHECK(log.find(" -only XO -file ") != string::npos);
    // the texts and the picture as the 2020 script's records
    const string display = pe.tmp.readFile("out.txt.display");
    CHECK(display.find("IMAGE\t640\t360\tbg\n") != string::npos);
    CHECK(display.find("FTEXT\t1\t2\t3\tf\t4\t5\t6\tHello\\nWorld\n") != string::npos);
    const string choice = pe.tmp.readFile("fake.log.choice");
    CHECK(choice.find("doom_controller_select.png") != string::npos);
    CHECK(choice.find("FTEXT\t0\t0\t12\tf\t0\t0\t0\t \n") != string::npos);
    // the program's exit code is the answer; its "no display" (3) is the fixed rule, the first allowed letter
    CHECK(pe.out("answer_dialog") == "102");
    CHECK(pe.out("answer_no_display") == "103");
    CHECK(pe.tmp.readFile("rt/logs/pe/dialogs.log").find("no dialog (3)") != string::npos);
    CHECK_FALSE(DirEntry::exists(pe.tmp.at("pe"))); // and the text screen was taken down with the rest
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

namespace {
// the driver's tail: whether the pid in FILE still runs once the runner has returned - read at once, not polled (the
// runner must not come back before it is gone); a zombie counts as gone (see the TERM test above)
string goneCheck(const string &file) {
    return "C=$(cat \"" + file +
           "\")\n"
           "if [ ! -r /proc/$C/stat ] || [ \"$(sed 's/.*) //' /proc/$C/stat 2>/dev/null | cut -c1)\" = Z ]; then "
           "echo child=gone; else echo child=alive; kill -KILL $C; fi\n";
}
} // namespace

// OpenJazz on the console, 2026-10-05: the mod's launch.sh ended while the game it started ran on, so the launcher
// came back under a program that held the screen, and Reset (abpadd watches the runner, which was gone) did nothing
TEST_CASE("pe_run.sh: Reset stops a child that ignores TERM (TERM, then KILL), and the clean-up still happens") {
    if (!haveSh() || !DirEntry::exists("/proc/self/stat")) {
        MESSAGE("no sh or no /proc here - the test is skipped");
        return;
    }
    PeRun pe;
    pe.tmp.writeFile("Apps/pe-demo/launch.sh", "#!/bin/sh\n"
                                               "(trap '' TERM; exec sleep 300) &\n" // TERM is ignored across the exec
                                               "echo $! > \"$APP_OUT.child\"\n"
                                               "wait\n");
    vector<string> lines =
        pe.run("sh \"$AB_ROOT/Autobleem/rc/pe_run.sh\" \"$AB_ROOT/Apps/pe-demo\" &\n"
               "PID=$!\n"
               "n=0; while [ ! -f \"$APP_OUT.child\" ] && [ $n -lt 100 ]; do sleep 0.1; n=$((n+1)); done\n"
               "sleep 0.3\n"
               "kill -TERM $PID\n"
               "wait $PID\n"
               "echo rc=$?\n" +
               goneCheck("$APP_OUT.child"));
    REQUIRE(lines.size() >= 2);
    CHECK(lines[lines.size() - 2] == "rc=143");
    CHECK(lines.back() == "child=gone");
    const string log = pe.tmp.readFile("rt/logs/pe/pe_run.log");
    CHECK(log.find("told to stop - TERM to ") != string::npos);
    CHECK(log.find("still running a second after the TERM - KILL to ") != string::npos);
    CHECK(log.find("the mod ended with 143") != string::npos);
    CHECK_FALSE(DirEntry::exists(pe.tmp.at("vol/launchtmp")));
    CHECK_FALSE(DirEntry::exists(pe.tmp.at("pe")));
    CHECK(pe.tmp.readFile("power/disable") == "1");
}

TEST_CASE("pe_run.sh: what the mod leaves running when its launch.sh ends is stopped before the runner returns") {
    if (!haveSh() || !DirEntry::exists("/proc/self/stat")) {
        MESSAGE("no sh or no /proc here - the test is skipped");
        return;
    }
    PeRun pe;
    // the game's parent goes at once (a subshell that only starts it), so the game is nobody's below the mod any more;
    // it ignores TERM as well, and launch.sh then ends normally
    pe.tmp.writeFile("Apps/pe-demo/launch.sh", "#!/bin/sh\n"
                                               "(trap '' TERM; sleep 300 & echo $! > \"$APP_OUT.child\")\n"
                                               "exit 0\n");
    vector<string> lines = pe.run("sh \"$AB_ROOT/Autobleem/rc/pe_run.sh\" \"$AB_ROOT/Apps/pe-demo\"\necho rc=$?\n" +
                                  goneCheck("$APP_OUT.child"));
    REQUIRE(lines.size() >= 2);
    CHECK(lines[lines.size() - 2] == "rc=0"); // the mod's own status: it was not told to stop
    CHECK(lines.back() == "child=gone");
    const string log = pe.tmp.readFile("rt/logs/pe/pe_run.log");
    CHECK(log.find("the program ended (0) and left these running - TERM to ") != string::npos);
    CHECK(log.find("sleep 300") != string::npos); // named in the log
    CHECK(log.find("KILL to ") != string::npos);
    CHECK_FALSE(DirEntry::exists(pe.tmp.at("pe")));
}

TEST_CASE("app_run.sh: the same rule - a TERM stops the App and what it started, and what it leaves is stopped") {
    if (!haveSh() || !DirEntry::exists("/proc/self/stat")) {
        MESSAGE("no sh or no /proc here - the test is skipped");
        return;
    }
    PeRun pe;
    REQUIRE(DirEntry::copy(string(AB_RC_DIR) + "/app_run.sh", pe.tmp.at("Autobleem/rc/app_run.sh")));
    pe.tmp.makeSubDir("Apps/demo");
    // the App: a child that ignores TERM, written down, then the App waits (TERM case) or ends at once (left case)
    pe.tmp.writeFile("Apps/demo/game", "#!/bin/sh\n"
                                       "(trap '' TERM; sleep 300 & echo $! > \"$APP_OUT.child\")\n"
                                       "[ \"$1\" = wait ] && sleep 300\n"
                                       "exit 5\n");
    const string start = "chmod +x \"$AB_ROOT/Apps/demo/game\"\n"
                         "export AB_APP_DIR=\"$AB_ROOT/Apps/demo\" AB_APP_EXEC=\"$AB_ROOT/Apps/demo/game\"\n";

    SUBCASE("the App ends by itself") {
        vector<string> lines = pe.run(start +
                                      "export AB_APP_ARGS=now\n"
                                      "sh \"$AB_ROOT/Autobleem/rc/app_run.sh\" 2>/dev/null\necho rc=$?\n" +
                                      goneCheck("$APP_OUT.child"));
        REQUIRE(lines.size() >= 2);
        CHECK(lines[lines.size() - 2] == "rc=5");
        CHECK(lines.back() == "child=gone");
    }
    SUBCASE("Reset") {
        vector<string> lines =
            pe.run(start +
                   "export AB_APP_ARGS=wait\n"
                   "sh \"$AB_ROOT/Autobleem/rc/app_run.sh\" 2>/dev/null &\n"
                   "PID=$!\n"
                   "n=0; while [ ! -f \"$APP_OUT.child\" ] && [ $n -lt 100 ]; do sleep 0.1; n=$((n+1)); done\n"
                   "sleep 0.3\n"
                   "kill -TERM $PID\n"
                   "wait $PID\n"
                   "echo rc=$?\n" +
                   goneCheck("$APP_OUT.child"));
        REQUIRE(lines.size() >= 2);
        CHECK(lines[lines.size() - 2] == "rc=143");
        CHECK(lines.back() == "child=gone");
    }
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

TEST_CASE("pe_compat.ini sets no pad for any mod: every .mod gets the 2020 console pad, the original packages too") {
    // the round-4 pad plan: one model for every App; a port that wants another layout changes its own config. A pad=
    // line here would also reach the original 2020 package of the same launcher name (D2 of the plan).
    string ini;
    REQUIRE(DirEntry::readFile(string(AB_RC_DIR) + "/pe_compat.ini", ini));
    for (const char *key : {"\npad=", "\ndpad2analog=", "\nanalog2dpad="}) {
        INFO(key);
        CHECK(ini.find(key) == string::npos);
    }
    // the one documented quirk: DraStic's own remap library, which the shim stands in for when the user picks psc
    CHECK(ini.find("[drastic]\nremap=drastic_sdl_remap.so\n") != string::npos);
}

TEST_CASE("no cursor from a pad's touchpad: boot.sh puts the seat rule into /run and announces a connected pad again") {
    string rule;
    REQUIRE(DirEntry::readFile(string(AB_RC_DIR) + "/99-autobleem-pad-seat.rules", rule));
    // every name hid-sony and hid-playstation give a pad's touchpad and motion-sensors node, and the seat libinput
    // 1.4.1 reads (it skips a device whose ID_SEAT is not seat0)
    for (const char *name : {"*Wireless Controller Touchpad", "*DualSense*Touchpad", "*DualShock*Touchpad",
                             "*Wireless Controller Motion Sensors", "*DualSense*Motion Sensors"}) {
        INFO(name);
        CHECK(rule.find(name) != string::npos);
    }
    CHECK(rule.find("ENV{ID_SEAT}=\"seat-autobleem-none\"") != string::npos);
    CHECK(rule.find('\r') == string::npos);
    string boot;
    REQUIRE(DirEntry::readFile(string(AB_RC_DIR) + "/boot.sh", boot));
    CHECK(boot.find("sh $RC/pad_seat.sh") != string::npos);

    if (!haveSh()) {
        MESSAGE("no sh on this machine - pad_seat.sh is not run");
        return;
    }
    // a fake sysfs: a DualSense's buttons, motion sensors and touchpad, and a mouse
    TempDir tmp("pad_seat");
    tmp.makeSubDir("rc");
    for (const char *f : {"pad_seat.sh", "99-autobleem-pad-seat.rules"}) {
        REQUIRE(DirEntry::copy(string(AB_RC_DIR) + "/" + f, tmp.at(string("rc/") + f)));
    }
    const pair<const char *, const char *> nodes[] = {{"event1", "DualSense Wireless Controller"},
                                                      {"event2", "DualSense Wireless Controller Motion Sensors"},
                                                      {"event3", "DualSense Wireless Controller Touchpad"},
                                                      {"event4", "Logitech USB Optical Mouse"}};
    for (const auto &node : nodes) {
        tmp.makeSubDir(string("sys/") + node.first + "/device");
        tmp.writeFile(string("sys/") + node.first + "/device/name", string(node.second) + "\n");
        tmp.writeFile(string("sys/") + node.first + "/uevent", "");
    }
    tmp.writeFile("udevadm", "#!/bin/sh\necho \"$*\" >> \"$(dirname \"$0\")/udevadm.log\"\n");
    const string r = slashes(tmp.path());
    tmp.writeFile("driver.sh", "export AB_PAD_SEAT_RULES_DIR='" + r + "/run/udev/rules.d' AB_PAD_SEAT_SYSFS='" + r +
                                   "/sys' AB_PAD_SEAT_UDEVADM='sh " + r + "/udevadm'\nsh '" + r + "/rc/pad_seat.sh'\n");
    vector<string> lines = System::execUnixCommandLines("sh \"" + slashes(tmp.at("driver.sh")) + "\"");
    CHECK(tmp.readFile("run/udev/rules.d/99-autobleem-pad-seat.rules") == rule);
    CHECK(Strings::trim(tmp.readFile("udevadm.log")) == "control --reload-rules");
    // the touchpad and the motion sensors are announced again ("remove", then "add" - the last write is what stays
    // in the fake file); the pad's buttons and the mouse are not touched
    CHECK(Strings::trim(tmp.readFile("sys/event2/uevent")) == "add");
    CHECK(Strings::trim(tmp.readFile("sys/event3/uevent")) == "add");
    CHECK(tmp.readFile("sys/event1/uevent").empty());
    CHECK(tmp.readFile("sys/event4/uevent").empty());
    CHECK(lines.size() == 2);
}

// a launch.sh that only reports the audio driver it was given
const char *const AudioLaunch = "#!/bin/sh\n"
                                "echo \"audio=$SDL_AUDIODRIVER\" >> \"$APP_OUT\"\n"
                                "exit 0\n";

TEST_CASE("pe_run.sh: no ALSA card lets SDL use the dummy audio driver, a card or a chosen driver is left alone") {
    if (!haveSh()) {
        MESSAGE("no sh on this machine - pe_run.sh is not run");
        return;
    }
    const string run = "sh \"$AB_ROOT/Autobleem/rc/pe_run.sh\" \"$AB_ROOT/Apps/pe-demo\"\n";
    struct Case {
        const char *cards;  // /proc/asound/cards as the machine has it; nullptr = no such file
        const char *preset; // SDL_AUDIODRIVER the caller already has
        const char *expect; // what the mod sees
        bool logged;
    };
    const Case cases[] = {
        {"--- no soundcards ---\n", "", "dummy", true},                       // the module is there, no card
        {nullptr, "", "dummy", true},                                         // no ALSA at all
        {" 0 [PCH            ]: HDA-Intel - HDA Intel PCH\n", "", "", false}, // a PC with sound
        {"--- no soundcards ---\n", "alsa", "alsa", false},                   // a driver was chosen: not ours to change
        {" 0 [PCH            ]: HDA-Intel - HDA Intel PCH\n                      HDA Intel PCH at 0xf7 irq 31\n", "",
         "", false},
    };
    for (const Case &c : cases) {
        PeRun pe;
        pe.tmp.writeFile("Apps/pe-demo/launch.sh", AudioLaunch);
        string body = "unset SDL_AUDIODRIVER\n";
        if (c.cards) {
            pe.tmp.writeFile("cards.txt", c.cards);
            body += "export PE_ASOUND_CARDS=\"$AB_ROOT/cards.txt\"\n";
        } else {
            body += "export PE_ASOUND_CARDS=\"$AB_ROOT/no-such-cards\"\n";
        }
        if (*c.preset)
            body += string("export SDL_AUDIODRIVER=") + c.preset + "\n";
        pe.run(body + run);
        CHECK(pe.out("audio") == string(c.expect));
        const string log = pe.tmp.readFile("rt/logs/pe/pe_run.log");
        CHECK((log.find("SDL_AUDIODRIVER=dummy") != string::npos) == c.logged);
    }
}

// a program of an App as the loader sees one: an ELF header and the names it asks for
const char *const ElfNamingGl = "\x7f"
                                "ELF....libGL.so.1....libSDL2-2.0.so.0";

TEST_CASE("pe_run.sh: the missing-libGL line is said only for an App that names libGL and brings none") {
    if (!haveSh()) {
        MESSAGE("no sh on this machine - pe_run.sh is not run");
        return;
    }
    const string run = "sh \"$AB_ROOT/Autobleem/rc/pe_run.sh\" \"$AB_ROOT/Apps/pe-demo\"\n";
    // this stick has no gl4es in rc/pe/lib and no libs pack
    auto noGl4es = [](PeRun &pe) {
        pe.run("rm -f \"$AB_ROOT/Autobleem/rc/pe/lib/libGL.so.1\" \"$AB_ROOT/Autobleem/rc/pe/lib/libGLU.so.1\"\n");
    };
    {
        PeRun pe; // an App with no GL at all (a 2D engine): silent
        noGl4es(pe);
        pe.tmp.writeFile("Apps/pe-demo/game", "\x7f"
                                              "ELF....libSDL2-2.0.so.0");
        pe.run(run);
        CHECK(pe.tmp.readFile("rt/logs/pe/pe_run.log").find("no libGL") == string::npos);
    }
    {
        PeRun pe; // an App that asks for libGL.so.1 and has none: the line, once, naming the library
        noGl4es(pe);
        pe.tmp.writeFile("Apps/pe-demo/game", ElfNamingGl);
        pe.run(run);
        const string log = pe.tmp.readFile("rt/logs/pe/pe_run.log");
        CHECK(log.find("no libGL.so.1 (neither") != string::npos);
        CHECK(log.find("the App brings none") != string::npos);
        CHECK(log.find("no libGLU.so.1") == string::npos); // it does not name libGLU
    }
    {
        PeRun pe; // an App that carries its own gl4es (ioquake3): silent
        noGl4es(pe);
        pe.tmp.writeFile("Apps/pe-demo/game", ElfNamingGl);
        pe.tmp.makeSubDir("Apps/pe-demo/lib");
        pe.tmp.writeFile("Apps/pe-demo/lib/libGL.so.1", "gl4es");
        pe.run(run);
        CHECK(pe.tmp.readFile("rt/logs/pe/pe_run.log").find("no libGL.so.1") == string::npos);
    }
}
