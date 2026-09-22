//
// System: the process and console helpers.
//
#include "system.h"
#include "../main.h"

#include <array>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <ableem/engine/log.h>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <sched.h>
#endif
#include <unistd.h>

using namespace std;

namespace {
string floatToString(float value, int precision) {
    ostringstream oss;
    oss << fixed << setprecision(precision) << value;
    return oss.str();
}
#ifdef _WIN32
wstring wide(const string &s) {
    if (s.empty()) {
        return L"";
    }
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
    wstring w(n > 0 ? n - 1 : 0, L'\0');
    if (n > 1) {
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
    }
    return w;
}

// one command line, quoted by the rules CommandLineToArgvW / the CRT undo: an argument with a space, a
// tab or a quote goes in quotes, backslashes before a quote (or the closing one) are doubled
wstring quotedArgument(const string &arg) {
    wstring w = wide(arg);
    if (!w.empty() && w.find_first_of(L" \t\"") == wstring::npos) {
        return w;
    }
    wstring out = L"\"";
    size_t backslashes = 0;
    for (wchar_t c : w) {
        if (c == L'\\') {
            ++backslashes;
            continue;
        }
        if (c == L'"') {
            out.append(backslashes * 2 + 1, L'\\');
        } else {
            out.append(backslashes, L'\\');
        }
        backslashes = 0;
        out += c;
    }
    out.append(backslashes * 2, L'\\');
    out += L'"';
    return out;
}

wstring commandLineFor(const string &exe, const vector<string> &args) {
    wstring cmd = quotedArgument(exe);
    for (const string &arg : args) {
        cmd += L" " + quotedArgument(arg);
    }
    return cmd;
}

// the one CreateProcess: a ready command line (the program first), the directory to start in ("" = ours),
// waited for unless `wait` is false; the exit code (0 when not waited for), -1 when it could not start.
// CREATE_NO_WINDOW: a console program (cmd, curl) gets no console window of its own; a GUI program is
// unaffected.
int createProcessAndWait(const wstring &commandLine, const wstring &dir, const string &what, bool wait = true,
                         const function<void()> &whileWaiting = {}) {
    vector<wchar_t> buffer(commandLine.begin(), commandLine.end());
    buffer.push_back(L'\0');

    STARTUPINFOW si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));
    if (!CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
                        nullptr, dir.empty() ? nullptr : dir.c_str(), &si, &pi)) {
        PLOG_WARNING << "could not start " << what << ": error " << GetLastError();
        return -1;
    }
    CloseHandle(pi.hThread);
    if (!wait) {
        CloseHandle(pi.hProcess);
        PLOG_INFO << what << " started";
        return 0;
    }
    if (whileWaiting) {
        while (WaitForSingleObject(pi.hProcess, 100) == WAIT_TIMEOUT) {
            whileWaiting();
        }
    } else {
        WaitForSingleObject(pi.hProcess, INFINITE);
    }
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    PLOG_INFO << what << " exited with " << code;
    return static_cast<int>(code);
}
#endif
} // namespace

//*******************************
// System::powerOff
//*******************************
// The one way the app powers the console off: the launcher's L2+R2, the classic menu's L2+R2 and the
// console's power button all come here. sync() first, so the last log lines and any ini just written
// reach the USB stick before the halt.
void System::powerOff() {
#if defined(AB_PLATFORM_PSC) || defined(AB_APPLIANCE)
    System::execUnixCommand("shutdown -h now");
    sync();
    exit(0);
#else
    exit(0); // a dev host or a Windows PC: leaving the launcher is all "power off" means
#endif
}

//*******************************
// System::lowerCurrentThreadPriority
//*******************************
void System::lowerCurrentThreadPriority() {
#ifdef _WIN32
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_IDLE);
#elif defined(SCHED_IDLE)
    struct sched_param param;
    param.sched_priority = 0; // SCHED_IDLE requires 0
    if (sched_setscheduler(0, SCHED_IDLE, &param) != 0)
        nice(19); // SCHED_IDLE refused (needs a capability some setups don't grant) - a plain nice bump instead
#else
    nice(19);
#endif
}

//*******************************
// System::getAvailableSpace
//*******************************
/*
 * Return the available space of a usb device
 */
string System::getAvailableSpace() {
    // the filesystem the USB root (the data partition on a Pi, the data tree's drive on Windows) is on
    uint64_t freeBytes = 0, totalBytes = 0;
    if (!diskSpace(Env::getPathToUSBRoot(), freeBytes, totalBytes)) {
        return "";
    }
    const double gb = 1024.0 * 1024.0 * 1024.0;
    int freeSpacePerc = totalBytes > 0 ? static_cast<int>(freeBytes * 100 / totalBytes) : 0;
    return floatToString(static_cast<float>(freeBytes / gb), 2) + " GB / " +
           floatToString(static_cast<float>(totalBytes / gb), 2) + " GB (" + to_string(freeSpacePerc) + "%)";
}

//*******************************
// System::diskSpace
//*******************************
bool System::diskSpace(const string &path, uint64_t &freeBytes, uint64_t &totalBytes) {
#ifdef _WIN32
    ULARGE_INTEGER freeToCaller, total;
    if (!GetDiskFreeSpaceExW(wide(path).c_str(), &freeToCaller, &total, nullptr)) {
        return false;
    }
    freeBytes = freeToCaller.QuadPart;
    totalBytes = total.QuadPart;
    return true;
#else
    struct statvfs fs{};
    if (statvfs(path.c_str(), &fs) != 0) {
        return false;
    }
    freeBytes = static_cast<uint64_t>(fs.f_bavail) * fs.f_frsize;
    totalBytes = static_cast<uint64_t>(fs.f_blocks) * fs.f_frsize;
    return true;
#endif
}

//*******************************
// System::startDetached
//*******************************
bool System::startDetached(const string &exe, const vector<string> &args) {
    string line = "Starting (not waited for): '" + exe + "'";
    for (const string &arg : args) {
        line += " '" + arg + "'";
    }
    PLOG_INFO << line;
#ifdef _WIN32
    return createProcessAndWait(commandLineFor(exe, args), L"", exe, false) == 0;
#else
    vector<const char *> argv;
    argv.push_back(exe.c_str());
    for (const string &arg : args) {
        argv.push_back(arg.c_str());
    }
    argv.push_back(nullptr);
    pid_t pid = fork();
    if (pid == -1) {
        PLOG_WARNING << "fork() failed: " << strerror(errno);
        return false;
    }
    if (pid == 0) {
        setsid();
        execvp(exe.c_str(), const_cast<char **>(argv.data()));
        _exit(127);
    }
    return true;
#endif
}

//*******************************
// System::makeDirectoryLink / removeDirectoryLink
//*******************************
bool System::makeDirectoryLink(const string &link, const string &target) {
    removeDirectoryLink(link);
#ifdef _WIN32
    string l = link, t = target;
    for (char &c : l)
        if (c == '/')
            c = '\\';
    for (char &c : t)
        if (c == '/')
            c = '\\';
    // a junction wants a directory that exists
    if (!DirEntry::isDirectory(target)) {
        return false;
    }
    return runShellCommand("mklink /J \"" + l + "\" \"" + t + "\" >nul") == 0 && DirEntry::isDirectory(link);
#else
    // a relative target would be taken relative to the link's own directory: the target as an absolute path
    char resolved[PATH_MAX];
    const char *abs = realpath(target.c_str(), resolved);
    return abs != nullptr && symlink(abs, link.c_str()) == 0;
#endif
}

void System::removeDirectoryLink(const string &link) {
#ifdef _WIN32
    RemoveDirectoryW(wide(link).c_str()); // a junction goes, its target stays; a real directory only when empty
#else
    unlink(link.c_str());
#endif
}

//*******************************
// System::runShellCommand
//*******************************
int System::runShellCommand(const string &commandLine) {
    PLOG_INFO << "Shell: " << commandLine;
#ifdef _WIN32
    // cmd.exe parses its own command line, not by the CRT's rules: `/c "<line>"` - the outer quotes go
    // and everything between them is the command, its own quotes intact. ComSpec is where cmd.exe is.
    const char *comspec = getenv("ComSpec");
    wstring cmd = L"\"" + wide(comspec ? comspec : "cmd.exe") + L"\" /c \"" + wide(commandLine) + L"\"";
    return createProcessAndWait(cmd, L"", "cmd /c " + commandLine);
#else
    int status = system(commandLine.c_str());
    if (status == -1) {
        return -1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif
}

//*******************************
// System::execUnixCommandLines
//*******************************
vector<string> System::execUnixCommandLines(const string &cmd) {
    vector<string> lines;
    PLOG_INFO << "Exec:" << cmd;
    struct PipeCloser {
        void operator()(FILE *f) const { pclose(f); }
    };
    unique_ptr<FILE, PipeCloser> pipe(popen(cmd.c_str(), "r"));
    if (!pipe) {
        PLOG_WARNING << "popen() failed for: " << cmd;
        return lines;
    }
    array<char, 512> buffer;
    string output;
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
        output += buffer.data();
    istringstream in(output);
    string line;
    while (getline(in, line)) {
        line = Strings::trim(line);
        if (!line.empty())
            lines.push_back(line);
    }
    return lines;
}

//*******************************
// System::execUnixCommand
//*******************************
/*
 * Execute a shell command and return output
 */
string System::execUnixCommand(const char *cmd) {
    array<char, 128> buffer;
    string result;
    PLOG_INFO << "Exec:" << cmd;
    struct PipeCloser {
        void operator()(FILE *f) const { pclose(f); }
    }; // not decltype(&pclose): its nonnull attribute is lost on the template argument (GCC 14 warns)
    unique_ptr<FILE, PipeCloser> pipe(popen(cmd, "r"));
    if (!pipe) {
        PLOG_WARNING << "popen() failed for: " << cmd;
        return result; // never throw: there is no handler anywhere and an abort() takes the whole UI down
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    result.erase(remove(result.begin(), result.end(), '\n'), result.end());
    return result;
}

//*******************************
// System::runAndWait
//*******************************
// fork + exec the program and wait for it to finish.
// returns the exit status of the program, or -1 if it could not be started.
int System::runAndWait(const string &exe, const vector<string> &args, const string &cwd,
                       const function<void()> &whileWaiting) {
    string line = "CMD line to execute: '" + exe + "'";
    for (const string &arg : args) {
        line += " '" + arg + "'";
    }
    if (!cwd.empty()) {
        line += " (in " + cwd + ")";
    }
    PLOG_INFO << line;

#ifdef _WIN32
    return createProcessAndWait(commandLineFor(exe, args), wide(cwd), exe, true, whileWaiting);
#else
    // argv[0] is the program itself, then the args, then a null terminator
    vector<const char *> argv;
    argv.push_back(exe.c_str());
    for (const string &arg : args) {
        argv.push_back(arg.c_str());
    }
    argv.push_back(nullptr);

    pid_t pid = fork();
    if (pid == -1) {
        PLOG_WARNING << "fork() failed: " << strerror(errno);
        return -1;
    }
    if (pid == 0) {
        // child. if exec fails we must not return into the parent's code path (that would run a second GUI).
        if (!cwd.empty() && chdir(cwd.c_str()) != 0) {
            _exit(126);
        }
        execvp(exe.c_str(), const_cast<char **>(argv.data()));
        _exit(127);
    }

    int status = 0;
    if (whileWaiting) {
        // polled, so the caller's hook runs meanwhile
        pid_t done = 0;
        while ((done = waitpid(pid, &status, WNOHANG)) == 0) {
            whileWaiting();
            usleep(100 * 1000);
        }
        if (done == -1) {
            PLOG_WARNING << "waitpid() failed: " << strerror(errno);
            return -1;
        }
    } else if (waitpid(pid, &status, 0) == -1) {
        PLOG_WARNING << "waitpid() failed: " << strerror(errno);
        return -1;
    }
    if (WIFEXITED(status)) {
        int exitCode = WEXITSTATUS(status);
        if (exitCode == 127) {
            PLOG_WARNING << "could not start: " << exe;
        } else if (exitCode == 126) {
            PLOG_WARNING << "could not start " << exe << " in " << cwd;
        }
        return exitCode;
    }
    if (WIFSIGNALED(status)) {
        PLOG_INFO << exe << " was killed by signal " << WTERMSIG(status);
    }
    return -1;
#endif
}

//*******************************
// System::getRandomNumber
//*******************************
unsigned int System::getRandomNumber() {
    static bool firstTime{true};
    if (firstTime) {
        srand(time(nullptr));
        firstTime = false;
    }

    return rand();
}

//*******************************
// System::getRandomIndex
// pass 100, get a random index between 0 and 99
//*******************************
unsigned int System::getRandomIndex(unsigned int size) {
    return getRandomNumber() % size;
}
