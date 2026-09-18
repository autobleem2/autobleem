//
// SystemInfoService: what the Hardware Information screen shows where there is no PSC-Bios app to run (a
// Raspberry Pi, a PC) - the machine and its OS, the CPU, memory and temperature, every volume with its free
// space, the network addresses, and what AutoBleem itself is running with.
//
#pragma once

#include <cstdint>
#include <string>
#include <vector>

//******************
// InfoRow / InfoSection
//******************
// a label and its value, grouped under a section title; the screen draws them as they come
struct InfoRow {
    std::string label;
    std::string value;
};

struct InfoSection {
    std::string title;
    std::vector<InfoRow> rows;
};

//******************
// SystemInfoService
//******************
// Stateless: collect() reads a dozen small files under /proc and /sys (a few registry keys and Win32 calls on
// Windows) and does one statvfs per volume, which is cheap enough for the screen to call it once a second
// so that the temperature, the free memory and the uptime move. Everything SDL knows (the renderer, the
// display mode, the pads) is the screen's own section - this class links nothing but the engine.
class SystemInfoService {
public:
    // every section, in display order: System, Hardware, Storage, Network, AutoBleem
    std::vector<InfoSection> collect() const;

    InfoSection system() const;   // OS, kernel, architecture, hostname, uptime, load
    InfoSection hardware() const; // model, CPU, cores, clock, temperature, memory
    InfoSection storage() const;  // every real filesystem mounted, and the AutoBleem data root
    InfoSection network() const;  // each interface's IPv4 address
    InfoSection software() const; // the build, the platform, the data root, RetroArch

    //*******************************
    // the parsers, pure so the tests can feed them a file's text
    //*******************************
    // PRETTY_NAME from /etc/os-release, unquoted; "" when the key is not there
    static std::string prettyNameFromOsRelease(const std::string &text);

    struct Memory {
        uint64_t totalKb = 0;
        uint64_t availableKb = 0; // MemAvailable, else MemFree
    };
    static Memory parseMeminfo(const std::string &text);

    struct Cpu {
        std::string model;    // "model name" (x86, recent ARM kernels)
        std::string hardware; // "Hardware" (older ARM kernels: the SoC)
        int cores = 0;        // the "processor" lines
    };
    static Cpu parseCpuinfo(const std::string &text);

    struct Mount {
        std::string device;
        std::string mountPoint;
        std::string fsType;
    };
    // the mounts of /proc/mounts a user would call a volume: block filesystems only (no proc/sys/tmpfs/
    // cgroup...), mount points unescaped ("\040" is a space there), in the file's order
    static std::vector<Mount> parseMounts(const std::string &text);
    static std::string unescapeMountPath(const std::string &path);

    // "1.5 GB", "512 MB", "3.2 TB"; "0 B" for 0
    static std::string formatBytes(uint64_t bytes);
    // "3 days 04:12:33", "04:12:33"
    static std::string formatDuration(uint64_t seconds);
    // "1.5 GB free of 14.9 GB (10%)"
    static std::string formatSpace(uint64_t freeBytes, uint64_t totalBytes);

    // the free and total bytes of the filesystem a path is on; false when it cannot be told
    static bool spaceOf(const std::string &path, uint64_t &freeBytes, uint64_t &totalBytes);
};
