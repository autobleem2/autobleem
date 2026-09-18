//
// SystemInfoService: the parsers behind the Hardware Information screen, fed the text of the files they read.
//
#include "doctest/doctest.h"

#include "../support/env_fixture.h"
#include "../support/temp_dir.h"

#include "core/services/system_info.h"

#include <string>

using std::string;

TEST_CASE("prettyNameFromOsRelease takes the quoted PRETTY_NAME and nothing else") {
    const string osRelease = "NAME=\"Raspbian GNU/Linux\"\n"
                             "VERSION_ID=\"13\"\n"
                             "PRETTY_NAME=\"Raspbian GNU/Linux 13 (trixie)\"\n"
                             "ID=raspbian\n";
    CHECK(SystemInfoService::prettyNameFromOsRelease(osRelease) == "Raspbian GNU/Linux 13 (trixie)");
    CHECK(SystemInfoService::prettyNameFromOsRelease("PRETTY_NAME=Plain\n") == "Plain");
    CHECK(SystemInfoService::prettyNameFromOsRelease("NAME=\"x\"\n") == "");
    CHECK(SystemInfoService::prettyNameFromOsRelease("") == "");
}

TEST_CASE("parseMeminfo prefers MemAvailable and falls back to MemFree") {
    SystemInfoService::Memory m = SystemInfoService::parseMeminfo("MemTotal:        3884356 kB\n"
                                                                  "MemFree:          200000 kB\n"
                                                                  "MemAvailable:    3000000 kB\n"
                                                                  "Buffers:           12345 kB\n");
    CHECK(m.totalKb == 3884356);
    CHECK(m.availableKb == 3000000);

    m = SystemInfoService::parseMeminfo("MemTotal:  1024 kB\nMemFree:   512 kB\n");
    CHECK(m.totalKb == 1024);
    CHECK(m.availableKb == 512);

    m = SystemInfoService::parseMeminfo("");
    CHECK(m.totalKb == 0);
    CHECK(m.availableKb == 0);
}

TEST_CASE("parseCpuinfo counts processors and keeps the first model name and the Hardware line") {
    // a Pi 4's layout: per-core blocks, then the board block at the end
    const string pi = "processor\t: 0\n"
                      "model name\t: ARMv7 Processor rev 3 (v7l)\n"
                      "BogoMIPS\t: 108.00\n"
                      "\n"
                      "processor\t: 1\n"
                      "model name\t: ARMv7 Processor rev 3 (v7l)\n"
                      "\n"
                      "processor\t: 2\n"
                      "processor\t: 3\n"
                      "\n"
                      "Hardware\t: BCM2711\n"
                      "Revision\t: c03130\n"
                      "Model\t\t: Raspberry Pi 400 Rev 1.0\n";
    SystemInfoService::Cpu cpu = SystemInfoService::parseCpuinfo(pi);
    CHECK(cpu.cores == 4);
    CHECK(cpu.model == "ARMv7 Processor rev 3 (v7l)");
    CHECK(cpu.hardware == "BCM2711");

    // a 64-bit ARM kernel (the Pi 400's): no model name, no Hardware - the core comes from its part id
    const string pi64 = "processor\t: 0\nBogoMIPS\t: 108.00\nCPU implementer\t: 0x41\nCPU architecture: 8\n"
                        "CPU part\t: 0xd08\n\nprocessor\t: 1\nCPU implementer\t: 0x41\nCPU part\t: 0xd08\n\n"
                        "Revision\t: c03130\nModel\t\t: Raspberry Pi 400 Rev 1.0\n";
    cpu = SystemInfoService::parseCpuinfo(pi64);
    CHECK(cpu.cores == 2);
    CHECK(cpu.model == "ARM Cortex-A72");
    CHECK(cpu.hardware.empty());
    CHECK(SystemInfoService::armCoreName("0x41", "0xD04") == "ARM Cortex-A35"); // the console's MT8167
    CHECK(SystemInfoService::armCoreName("0x41", "0xfff") == "ARM (part 0xfff)");
    CHECK(SystemInfoService::armCoreName("0x51", "0x801") == "ARM implementer 0x51, part 0x801");

    // an x86 kernel: no Hardware line, a colon inside the value
    cpu = SystemInfoService::parseCpuinfo("processor\t: 0\nmodel name\t: Intel(R) Core(TM) i7 CPU @ 2.60GHz\n");
    CHECK(cpu.cores == 1);
    CHECK(cpu.model == "Intel(R) Core(TM) i7 CPU @ 2.60GHz");
    CHECK(cpu.hardware.empty());
}

TEST_CASE("parseMounts keeps the block filesystems only, in order, with the paths unescaped") {
    const string mounts = "proc /proc proc rw,nosuid 0 0\n"
                          "/dev/mmcblk0p2 / ext4 rw,noatime 0 0\n"
                          "devtmpfs /dev devtmpfs rw 0 0\n"
                          "tmpfs /run tmpfs rw 0 0\n"
                          "/dev/mmcblk0p1 /boot/firmware vfat rw 0 0\n"
                          "/dev/mmcblk0p3 /media/pi/AUTOBLEEM\\040DATA exfat rw 0 0\n"
                          "cgroup2 /sys/fs/cgroup cgroup2 rw 0 0\n"
                          "not a mount line\n";
    std::vector<SystemInfoService::Mount> list = SystemInfoService::parseMounts(mounts);
    REQUIRE(list.size() == 3);
    CHECK(list[0].mountPoint == "/");
    CHECK(list[0].fsType == "ext4");
    CHECK(list[0].device == "/dev/mmcblk0p2");
    CHECK(list[1].mountPoint == "/boot/firmware");
    CHECK(list[2].mountPoint == "/media/pi/AUTOBLEEM DATA");
    CHECK(list[2].fsType == "exfat");
}

TEST_CASE("unescapeMountPath turns the octal escapes back and leaves everything else alone") {
    CHECK(SystemInfoService::unescapeMountPath("/a\\040b") == "/a b");
    CHECK(SystemInfoService::unescapeMountPath("/a\\011b") == "/a\tb");
    CHECK(SystemInfoService::unescapeMountPath("/plain") == "/plain");
    CHECK(SystemInfoService::unescapeMountPath("/tail\\04") == "/tail\\04"); // too short to be an escape
    CHECK(SystemInfoService::unescapeMountPath("C:\\Users") == "C:\\Users"); // not three digits
}

TEST_CASE("formatBytes picks the unit and shows a decimal from megabytes up") {
    CHECK(SystemInfoService::formatBytes(0) == "0 B");
    CHECK(SystemInfoService::formatBytes(512) == "512 B");
    CHECK(SystemInfoService::formatBytes(1024) == "1 KB");
    CHECK(SystemInfoService::formatBytes(1536) == "2 KB"); // whole kilobytes, rounded
    CHECK(SystemInfoService::formatBytes(1024ULL * 1024) == "1.0 MB");
    CHECK(SystemInfoService::formatBytes(1536ULL * 1024 * 1024) == "1.5 GB");
    CHECK(SystemInfoService::formatBytes(3ULL * 1024 * 1024 * 1024 * 1024) == "3.0 TB");
    CHECK(SystemInfoService::formatBytes(3000ULL * 1024 * 1024 * 1024 * 1024) == "3000.0 TB"); // no unit past TB
}

TEST_CASE("formatDuration is hh:mm:ss with the days in front when there are any") {
    CHECK(SystemInfoService::formatDuration(0) == "00:00:00");
    CHECK(SystemInfoService::formatDuration(59) == "00:00:59");
    CHECK(SystemInfoService::formatDuration(3600 + 120 + 3) == "01:02:03");
    CHECK(SystemInfoService::formatDuration(86400 + 5) == "1 day 00:00:05");
    CHECK(SystemInfoService::formatDuration(3 * 86400 + 4 * 3600 + 12 * 60 + 33) == "3 days 04:12:33");
}

TEST_CASE("formatSpace is free of total with the percentage, and survives a zero total") {
    CHECK(SystemInfoService::formatSpace(1536ULL * 1024 * 1024, 15ULL * 1024 * 1024 * 1024) ==
          "1.5 GB free of 15.0 GB (10%)");
    CHECK(SystemInfoService::formatSpace(0, 0) == "0 B free of 0 B (0%)");
}

TEST_CASE("spaceOf answers for a real directory and refuses a missing one") {
    TempDir tmp("system_info");
    uint64_t freeBytes = 0, totalBytes = 0;
    CHECK(SystemInfoService::spaceOf(tmp.path(), freeBytes, totalBytes));
    CHECK(totalBytes > 0);
    CHECK(freeBytes <= totalBytes);
    CHECK_FALSE(SystemInfoService::spaceOf(tmp.path() + "/does/not/exist", freeBytes, totalBytes));
}

TEST_CASE("collect gives every section a title, and the storage section the data root") {
    EnvFixture env;
    TempDir tmp("system_info_root");
    env.setUsbRoot(tmp.path());
    SystemInfoService info;
    std::vector<InfoSection> sections = info.collect();
    REQUIRE(sections.size() == 5);
    for (const InfoSection &section : sections)
        CHECK_FALSE(section.title.empty());
    // the data root is the first storage row, whatever else the machine has mounted
    const InfoSection &storage = sections[2];
    REQUIRE_FALSE(storage.rows.empty());
    CHECK(storage.rows[0].label == "AutoBleem data");
    CHECK(storage.rows[0].value.find("free of") != string::npos);
    // no row is ever shown without a value
    for (const InfoSection &section : sections)
        for (const InfoRow &row : section.rows)
            CHECK_FALSE(row.value.empty());
}
