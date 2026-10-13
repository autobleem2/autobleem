//
// rc/ab_memory.sh (AUTOBLEEM-31): overcommit_memory=1 and one zram swap per CPU, set once - a second run (the boot
// loop's next round) must not add swap twice. The real script runs with the machine's sh on a fake /proc, /sys and
// /dev in a scratch folder, with mkswap and swapon stood in for by scripts that leave a trace. Without sh (a
// Windows shell with no MSYS2 on PATH) the tests are skipped.
//
#include "doctest/doctest.h"

#include "support/temp_dir.h"
#include "core/services/system.h"
#include "core/main.h"

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

struct MemWorld {
    explicit MemWorld(bool zram) : tmp("ab_memory") {
        tmp.makeSubDir("proc/sys/vm");
        tmp.writeFile("proc/sys/vm/overcommit_memory", "0\n");
        tmp.writeFile("proc/cpuinfo",
                      "processor\t: 0\nx\n\nprocessor\t: 1\nx\n\nprocessor\t: 2\nx\n\nprocessor\t: 3\nx\n");
        tmp.writeFile("proc/meminfo", "MemTotal:        1024000 kB\nMemFree:          500000 kB\n");
        tmp.writeFile("proc/swaps", "Filename\t\t\t\tType\t\tSize\tUsed\tPriority\n");
        tmp.makeSubDir("dev");
        tmp.makeSubDir("bin");
        tmp.makeSubDir("logs");
        if (zram) {
            tmp.makeSubDir("sys/class/zram-control");
            tmp.writeFile("sys/class/zram-control/hot_add", "0\n");
            for (int i = 0; i < 4; i++) {
                tmp.makeSubDir("sys/block/zram" + to_string(i));
                tmp.writeFile("sys/block/zram" + to_string(i) + "/disksize", "0\n");
            }
        }
        // mkswap marks the device made, swapon lists it in /proc/swaps like the kernel does
        tmp.writeFile("bin/mkswap", "#!/bin/sh\necho \"mkswap $*\" >> \"$FAKE_LOG\"\n");
        string swaponBody = "#!/bin/sh\necho \"swapon $*\" >> \"$FAKE_LOG\"\n";
        swaponBody += "for last; do :; done\n";
        swaponBody += "echo \"$last partition 256000 0 100\" >> \"$AB_MEM_PROC/swaps\"\n";
        tmp.writeFile("bin/swapon", swaponBody);
        const string mkswap = slashes(tmp.at("bin/mkswap"));
        const string swapon = slashes(tmp.at("bin/swapon"));
        const string chmod = "chmod +x \"" + mkswap + "\" \"" + swapon + "\"";
        System::execUnixCommand(chmod.c_str());
    }

    vector<string> run(int times) {
        const string r = slashes(tmp.path());
        string body = "export AB_MEM_PROC='" + r + "/proc' AB_MEM_SYS='" + r + "/sys' AB_MEM_DEV='" + r + "/dev'\n";
        body += "export AB_LOG_DIR='" + r + "/logs' FAKE_LOG='" + r + "/fake.log' PATH='" + r + "/bin':$PATH\n";
        body += ". '" + string(AB_RC_DIR) + "/ab_memory.sh'\n";
        for (int i = 0; i < times; i++)
            body += "ab_memory_setup\n";
        tmp.writeFile("driver.sh", body);
        return System::execUnixCommandLines("sh \"" + slashes(tmp.at("driver.sh")) + "\"");
    }

    int count(const string &file, const string &what) const {
        int n = 0;
        string text = tmp.readFile(file);
        for (string::size_type at = text.find(what); at != string::npos; at = text.find(what, at + 1))
            n++;
        return n;
    }

    TempDir tmp;
};

} // namespace

TEST_CASE("ab_memory.sh sets overcommit 1 and one zram swap per CPU, half of MemTotal in all") {
    if (!haveSh()) {
        MESSAGE("no sh on this machine - ab_memory.sh is not run");
        return;
    }
    MemWorld w(true);
    w.run(1);
    CHECK(Strings::trim(w.tmp.readFile("proc/sys/vm/overcommit_memory")) == "1");
    // 1024000 kB * 1024 / 2 / 4 CPUs
    CHECK(Strings::trim(w.tmp.readFile("sys/block/zram0/disksize")) == "131072000");
    CHECK(Strings::trim(w.tmp.readFile("sys/block/zram3/disksize")) == "131072000");
    CHECK(w.count("fake.log", "swapon -p 100") == 4);
    CHECK(w.count("fake.log", "mkswap -L zram2 ") == 1);
    CHECK(w.count("proc/swaps", "/dev/zram") == 4);
    string log = w.tmp.readFile("logs/boot.log");
    CHECK(log.find("overcommit_memory=1") != string::npos);
    CHECK(log.find("4 of 4 devices, 125 MiB each") != string::npos);
}

TEST_CASE("ab_memory.sh run again adds no swap") {
    if (!haveSh())
        return;
    MemWorld w(true);
    w.run(3);
    CHECK(w.count("fake.log", "swapon") == 4);
    CHECK(w.count("fake.log", "mkswap") == 4);
    CHECK(w.count("proc/swaps", "/dev/zram") == 4);
    CHECK(Strings::trim(w.tmp.readFile("proc/sys/vm/overcommit_memory")) == "1");
}

TEST_CASE("ab_memory.sh on a kernel without zram sets overcommit and skips the swap") {
    if (!haveSh())
        return;
    MemWorld w(false);
    w.run(1);
    CHECK(Strings::trim(w.tmp.readFile("proc/sys/vm/overcommit_memory")) == "1");
    CHECK(w.tmp.readFile("fake.log").empty());
    CHECK(w.tmp.readFile("logs/boot.log").find("no zram") != string::npos);
}
