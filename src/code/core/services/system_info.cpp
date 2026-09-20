//
// SystemInfoService: the machine, the OS, the volumes and the network, for the Hardware Information screen.
//
#include "system.h"
#include "system_info.h"
#include "../main.h"
#include "environment.h"
#include "core/version.h" // generated into the build tree

#include <ableem/engine/log.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <iphlpapi.h>
#else
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/utsname.h>
#include <unistd.h>
#endif

using namespace std;

namespace {

//*******************************
// readTextFile
//*******************************
// the whole of a small text file, "" when it is not there - /proc and /sys files are read this way, so a
// missing one (another kernel, another platform) just leaves its row out
string readTextFile(const string &path) {
    ifstream in(path);
    if (!in)
        return "";
    stringstream buffer;
    buffer << in.rdbuf();
    return buffer.str();
}

// the first line of a file, trimmed; /sys values are one line with a newline, device-tree strings end in NUL
string readFirstLine(const string &path) {
    string text = readTextFile(path);
    size_t end = text.find_first_of("\n\0", 0, 2);
    if (end != string::npos)
        text.erase(end);
    return Strings::trim(text);
}

// "1.23" from a float - the formatting every size and temperature row uses
string fixed(double value, int decimals) {
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
    return buffer;
}

void addRow(InfoSection &section, const string &label, const string &value) {
    if (!value.empty())
        section.rows.push_back({label, value});
}

#ifdef _WIN32
// a REG_SZ under HKEY_LOCAL_MACHINE, "" when missing
string registryString(const char *key, const char *value) {
    char buffer[256];
    DWORD size = sizeof(buffer);
    if (RegGetValueA(HKEY_LOCAL_MACHINE, key, value, RRF_RT_REG_SZ, nullptr, buffer, &size) != ERROR_SUCCESS)
        return "";
    return buffer;
}
#endif

} // namespace

//*******************************
// SystemInfoService::collect
//*******************************
vector<InfoSection> SystemInfoService::collect() const {
    return {system(), hardware(), storage(), network(), software()};
}

//*******************************
// SystemInfoService::system
//*******************************
InfoSection SystemInfoService::system() const {
    InfoSection section{_("System"), {}};
#ifdef _WIN32
    const char *versionKey = "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion";
    string os = registryString(versionKey, "ProductName");
    string release = registryString(versionKey, "DisplayVersion");
    string build = registryString(versionKey, "CurrentBuildNumber");
    if (!release.empty())
        os += " " + release;
    if (!build.empty())
        os += " (" + _("build") + " " + build + ")";
    addRow(section, _("Operating system"), os);
    SYSTEM_INFO info;
    GetNativeSystemInfo(&info);
    addRow(section, _("Architecture"),
           info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64   ? "x86_64"
           : info.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_ARM64 ? "arm64"
                                                                         : "x86");
    char host[256] = "";
    DWORD hostSize = sizeof(host);
    if (GetComputerNameA(host, &hostSize))
        addRow(section, _("Hostname"), host);
    addRow(section, _("Uptime"), formatDuration(GetTickCount64() / 1000));
#else
    string os = prettyNameFromOsRelease(readTextFile("/etc/os-release"));
    utsname name{};
    if (uname(&name) == 0) {
        if (os.empty())
            os = name.sysname;
        addRow(section, _("Operating system"), os);
        addRow(section, _("Kernel"), string(name.sysname) + " " + name.release);
        addRow(section, _("Architecture"), name.machine);
        addRow(section, _("Hostname"), name.nodename);
    } else {
        addRow(section, _("Operating system"), os);
    }
    // /proc/uptime: "<seconds up> <seconds idle>"
    string uptime = readFirstLine("/proc/uptime");
    if (!uptime.empty())
        addRow(section, _("Uptime"), formatDuration(static_cast<uint64_t>(atof(uptime.c_str()))));
    // /proc/loadavg: "0.12 0.08 0.05 1/234 5678" - the three averages are what is worth showing
    string load = readFirstLine("/proc/loadavg");
    istringstream loadIn(load);
    string one, five, fifteen;
    if (loadIn >> one >> five >> fifteen)
        addRow(section, _("Load average"), one + "  " + five + "  " + fifteen);
#endif
    return section;
}

//*******************************
// SystemInfoService::hardware
//*******************************
InfoSection SystemInfoService::hardware() const {
    InfoSection section{_("Hardware"), {}};
#ifdef _WIN32
    addRow(section, _("Processor"),
           Strings::trim(registryString("HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString")));
    SYSTEM_INFO info;
    GetNativeSystemInfo(&info);
    addRow(section, _("Cores"), to_string(info.dwNumberOfProcessors));
    DWORD mhz = 0, mhzSize = sizeof(mhz);
    if (RegGetValueA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "~MHz", RRF_RT_REG_DWORD,
                     nullptr, &mhz, &mhzSize) == ERROR_SUCCESS)
        addRow(section, _("Clock"), to_string(mhz) + " MHz");
    MEMORYSTATUSEX memory;
    memory.dwLength = sizeof(memory);
    if (GlobalMemoryStatusEx(&memory)) {
        addRow(section, _("Memory"), formatBytes(memory.ullTotalPhys));
        addRow(section, _("Memory free"), formatSpace(memory.ullAvailPhys, memory.ullTotalPhys));
    }
#else
    // the device tree names the board on a Pi ("Raspberry Pi 400 Rev 1.0") and on the console
    addRow(section, _("Model"), readFirstLine("/proc/device-tree/model"));
    Cpu cpu = parseCpuinfo(readTextFile("/proc/cpuinfo"));
    addRow(section, _("Processor"), cpu.model.empty() ? cpu.hardware : cpu.model);
    if (!cpu.model.empty() && !cpu.hardware.empty())
        addRow(section, _("SoC"), cpu.hardware);
    if (cpu.cores > 0)
        addRow(section, _("Cores"), to_string(cpu.cores));
    // cpufreq reports kHz; the max is the interesting one next to what it runs at now
    string cur = readFirstLine("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq");
    string max = readFirstLine("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq");
    if (!cur.empty()) {
        string clock = to_string(atol(cur.c_str()) / 1000) + " MHz";
        if (!max.empty())
            clock += " / " + to_string(atol(max.c_str()) / 1000) + " MHz";
        addRow(section, _("Clock"), clock);
    }
    // millidegrees; the first thermal zone is the SoC on a Pi and on the console
    string temp = readFirstLine("/sys/class/thermal/thermal_zone0/temp");
    if (!temp.empty())
        addRow(section, _("Temperature"), fixed(atol(temp.c_str()) / 1000.0, 1) + " \xC2\xB0" + "C");
    Memory memory = parseMeminfo(readTextFile("/proc/meminfo"));
    if (memory.totalKb > 0) {
        addRow(section, _("Memory"), formatBytes(memory.totalKb * 1024));
        addRow(section, _("Memory free"), formatSpace(memory.availableKb * 1024, memory.totalKb * 1024));
    }
#endif
    return section;
}

//*******************************
// SystemInfoService::storage
//*******************************
InfoSection SystemInfoService::storage() const {
    InfoSection section{_("Storage"), {}};
    // the AutoBleem data root first - the games' volume, what "free space" means to a user of this program
    uint64_t freeBytes = 0, totalBytes = 0;
    if (spaceOf(Env::getPathToUSBRoot(), freeBytes, totalBytes))
        addRow(section, _("AutoBleem data"), formatSpace(freeBytes, totalBytes));
#ifdef _WIN32
    char drives[256] = "";
    GetLogicalDriveStringsA(sizeof(drives), drives);
    for (const char *drive = drives; *drive; drive += strlen(drive) + 1) {
        UINT type = GetDriveTypeA(drive);
        if (type != DRIVE_FIXED && type != DRIVE_REMOVABLE)
            continue;
        char label[64] = "", fsName[32] = "";
        GetVolumeInformationA(drive, label, sizeof(label), nullptr, nullptr, nullptr, fsName, sizeof(fsName));
        if (!spaceOf(drive, freeBytes, totalBytes))
            continue; // a card reader with no card
        string name = string(drive, 2);
        if (*label)
            name += " " + string(label);
        if (*fsName)
            name += " (" + string(fsName) + ")";
        addRow(section, name, formatSpace(freeBytes, totalBytes));
    }
#else
    for (const Mount &mount : parseMounts(readTextFile("/proc/mounts"))) {
        if (!spaceOf(mount.mountPoint, freeBytes, totalBytes) || totalBytes == 0)
            continue;
        addRow(section, mount.mountPoint + " (" + mount.fsType + ")", formatSpace(freeBytes, totalBytes));
    }
#endif
    return section;
}

//*******************************
// SystemInfoService::network
//*******************************
InfoSection SystemInfoService::network() const {
    InfoSection section{_("Network"), {}};
#ifdef _WIN32
    ULONG size = 16 * 1024;
    vector<char> buffer(size);
    auto *adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data());
    ULONG flags = GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER;
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &size) == ERROR_BUFFER_OVERFLOW) {
        buffer.resize(size);
        adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES *>(buffer.data());
    }
    if (GetAdaptersAddresses(AF_INET, flags, nullptr, adapters, &size) == NO_ERROR) {
        for (auto *adapter = adapters; adapter; adapter = adapter->Next) {
            if (adapter->OperStatus != IfOperStatusUp || adapter->IfType == IF_TYPE_SOFTWARE_LOOPBACK)
                continue;
            for (auto *unicast = adapter->FirstUnicastAddress; unicast; unicast = unicast->Next) {
                char address[INET_ADDRSTRLEN] = "";
                auto *in = reinterpret_cast<sockaddr_in *>(unicast->Address.lpSockaddr);
                inet_ntop(AF_INET, &in->sin_addr, address, sizeof(address));
                // the friendly name is UTF-16; the characters a user names an adapter with fit in a char
                wstring wide(adapter->FriendlyName);
                addRow(section, string(wide.begin(), wide.end()), address);
            }
        }
    }
#else
    ifaddrs *list = nullptr;
    if (getifaddrs(&list) == 0) {
        for (ifaddrs *entry = list; entry; entry = entry->ifa_next) {
            if (!entry->ifa_addr || entry->ifa_addr->sa_family != AF_INET)
                continue;
            if (entry->ifa_flags & IFF_LOOPBACK)
                continue;
            char address[INET_ADDRSTRLEN] = "";
            auto *in = reinterpret_cast<sockaddr_in *>(entry->ifa_addr);
            inet_ntop(AF_INET, &in->sin_addr, address, sizeof(address));
            addRow(section, entry->ifa_name, address);
        }
        freeifaddrs(list);
    }
#endif
    if (section.rows.empty())
        addRow(section, _("Address"), _("Not connected"));
    return section;
}

//*******************************
// SystemInfoService::software
//*******************************
InfoSection SystemInfoService::software() const {
    InfoSection section{_("AutoBleem"), {}};
    addRow(section, _("Version"), Version::FULL_VERSION);
    addRow(section, _("Built"), Version::BUILD_TIMESTAMP);
    addRow(section, _("Platform"), Env::platformName());
    addRow(section, _("Data root"), Env::getPathToUSBRoot());
    addRow(section, _("Games"), Env::getPathToGamesDir());
    addRow(section, _("RetroArch"), Env::retroArchInstalled() ? Env::getPathToRetroarchDir() : _("Not installed"));
    return section;
}

//*******************************
// SystemInfoService::prettyNameFromOsRelease
//*******************************
string SystemInfoService::prettyNameFromOsRelease(const string &text) {
    istringstream in(text);
    string line;
    while (getline(in, line)) {
        if (line.compare(0, 12, "PRETTY_NAME=") != 0)
            continue;
        string value = Strings::trim(line.substr(12));
        if (value.size() >= 2 && (value.front() == '"' || value.front() == '\'') && value.back() == value.front())
            value = value.substr(1, value.size() - 2);
        return value;
    }
    return "";
}

//*******************************
// SystemInfoService::parseMeminfo
//*******************************
// "MemTotal:        3884356 kB" lines; MemAvailable (3.14+) counts the reclaimable caches as free, which is
// what a user means by free, MemFree is the fallback for an older kernel
SystemInfoService::Memory SystemInfoService::parseMeminfo(const string &text) {
    Memory memory;
    uint64_t memFree = 0;
    bool haveAvailable = false;
    istringstream in(text);
    string line;
    while (getline(in, line)) {
        istringstream fields(line);
        string key;
        uint64_t value = 0;
        if (!(fields >> key >> value))
            continue;
        if (key == "MemTotal:")
            memory.totalKb = value;
        else if (key == "MemAvailable:") {
            memory.availableKb = value;
            haveAvailable = true;
        } else if (key == "MemFree:")
            memFree = value;
    }
    if (!haveAvailable)
        memory.availableKb = memFree;
    return memory;
}

//*******************************
// SystemInfoService::parseCpuinfo
//*******************************
// "key\t: value" lines, one block per processor; "model name" is per processor (all the same, the first is
// kept), "Hardware" is once at the end on the ARM kernels that have it
SystemInfoService::Cpu SystemInfoService::parseCpuinfo(const string &text) {
    Cpu cpu;
    string implementer, part;
    istringstream in(text);
    string line;
    while (getline(in, line)) {
        size_t colon = line.find(':');
        if (colon == string::npos)
            continue;
        string key = Strings::trim(line.substr(0, colon));
        string value = Strings::trim(line.substr(colon + 1));
        if (key == "processor")
            cpu.cores++;
        else if (key == "model name" && cpu.model.empty())
            cpu.model = value;
        else if (key == "Hardware" && cpu.hardware.empty())
            cpu.hardware = value;
        else if (key == "CPU implementer" && implementer.empty())
            implementer = value;
        else if (key == "CPU part" && part.empty())
            part = value;
    }
    // a 64-bit ARM kernel (a Pi 4/400 with the v8 kernel, the console's own) prints neither a model name
    // nor a Hardware line - only the core's implementer and part ids, which name it well enough
    if (cpu.model.empty() && !part.empty())
        cpu.model = armCoreName(implementer, part);
    return cpu;
}

//*******************************
// SystemInfoService::armCoreName
//*******************************
string SystemInfoService::armCoreName(const string &implementer, const string &part) {
    struct Core {
        const char *part;
        const char *name;
    };
    static const Core armCores[] = {{"0xc07", "Cortex-A7"},  {"0xc0f", "Cortex-A15"}, {"0xd03", "Cortex-A53"},
                                    {"0xd04", "Cortex-A35"}, {"0xd05", "Cortex-A55"}, {"0xd07", "Cortex-A57"},
                                    {"0xd08", "Cortex-A72"}, {"0xd09", "Cortex-A73"}, {"0xd0a", "Cortex-A75"},
                                    {"0xd0b", "Cortex-A76"}, {"0xd0d", "Cortex-A77"}, {"0xd41", "Cortex-A78"}};
    string id = part;
    lcase(id);
    if (implementer == "0x41") { // ARM Ltd
        for (const Core &core : armCores)
            if (id == core.part)
                return string("ARM ") + core.name;
        return "ARM (part " + part + ")";
    }
    if (implementer == "0x42" || implementer == "0x43")
        return "Broadcom/Cavium (part " + part + ")";
    return "ARM implementer " + implementer + ", part " + part;
}

//*******************************
// SystemInfoService::parseMounts
//*******************************
// "<device> <mount point> <fstype> <options> 0 0", the fields separated by spaces and a space inside a path
// written as "\040"
vector<SystemInfoService::Mount> SystemInfoService::parseMounts(const string &text) {
    static const char *const volumeTypes[] = {"ext2",    "ext3",  "ext4", "vfat", "exfat",   "ntfs", "ntfs3",
                                              "fuseblk", "btrfs", "xfs",  "f2fs", "hfsplus", "apfs", "msdos"};
    vector<Mount> mounts;
    istringstream in(text);
    string line;
    while (getline(in, line)) {
        istringstream fields(line);
        Mount mount;
        if (!(fields >> mount.device >> mount.mountPoint >> mount.fsType))
            continue;
        if (find(begin(volumeTypes), end(volumeTypes), mount.fsType) == end(volumeTypes))
            continue;
        mount.device = unescapeMountPath(mount.device);
        mount.mountPoint = unescapeMountPath(mount.mountPoint);
        mounts.push_back(mount);
    }
    return mounts;
}

//*******************************
// SystemInfoService::unescapeMountPath
//*******************************
string SystemInfoService::unescapeMountPath(const string &path) {
    string out;
    for (size_t i = 0; i < path.size(); i++) {
        if (path[i] == '\\' && i + 3 < path.size() && isdigit(static_cast<unsigned char>(path[i + 1])) &&
            isdigit(static_cast<unsigned char>(path[i + 2])) && isdigit(static_cast<unsigned char>(path[i + 3]))) {
            out += static_cast<char>(strtol(path.substr(i + 1, 3).c_str(), nullptr, 8));
            i += 3;
        } else {
            out += path[i];
        }
    }
    return out;
}

//*******************************
// SystemInfoService::formatBytes
//*******************************
string SystemInfoService::formatBytes(uint64_t bytes) {
    static const char *const units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = static_cast<double>(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        unit++;
    }
    // whole bytes and kilobytes, one decimal from megabytes up - "1.5 GB" reads better than "1.53 GB"
    return fixed(value, unit >= 2 ? 1 : 0) + " " + units[unit];
}

//*******************************
// SystemInfoService::formatDuration
//*******************************
string SystemInfoService::formatDuration(uint64_t seconds) {
    uint64_t days = seconds / 86400;
    seconds %= 86400;
    char clock[16];
    snprintf(clock, sizeof(clock), "%02d:%02d:%02d", static_cast<int>(seconds / 3600),
             static_cast<int>(seconds % 3600 / 60), static_cast<int>(seconds % 60));
    if (days == 0)
        return clock;
    return to_string(days) + " " + (days == 1 ? _("day") : _("days")) + " " + clock;
}

//*******************************
// SystemInfoService::formatSpace
//*******************************
string SystemInfoService::formatSpace(uint64_t freeBytes, uint64_t totalBytes) {
    int percent = totalBytes > 0 ? static_cast<int>(freeBytes * 100 / totalBytes) : 0;
    return formatBytes(freeBytes) + " " + _("free of") + " " + formatBytes(totalBytes) + " (" + to_string(percent) +
           "%)";
}

//*******************************
// SystemInfoService::spaceOf
//*******************************
bool SystemInfoService::spaceOf(const string &path, uint64_t &freeBytes, uint64_t &totalBytes) {
    return System::diskSpace(path, freeBytes, totalBytes);
}
