//
// System: the process and console helpers.
//
#include "system.h"
#include "../main.h"

#include <array>
#include <cerrno>
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
#include <sys/wait.h>
#include <sched.h>
#endif
#include <unistd.h>

using namespace std;

namespace {
#ifndef AB_DEBUG_HOST
string floatToString(float value, int precision) {
    ostringstream oss;
    oss << fixed << setprecision(precision) << value;
    return oss.str();
}
#endif
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
#ifdef AB_DEBUG_HOST
    return "x86 - does not care about free space - Does not work on mac";
#else
    // the filesystem the USB root (the data partition, on a Pi) is on; execUnixCommand returns "" when df
    // fails - Strings::toInt makes that a 0 instead of a thrown exception
    int gb = 1024 * 1024;
    string root = "'" + Env::getPathToUSBRoot() + "'";
    string freeCmd = "df -P " + root + " | tail -1 | awk '{print $4}'";
    string totalCmd = "df -P " + root + " | tail -1 | awk '{print $2}'";
    float freeSpace = (float)Strings::toInt(execUnixCommand(freeCmd.c_str())) / gb;
    float totalSpace = (float)Strings::toInt(execUnixCommand(totalCmd.c_str())) / gb;
    int freeSpacePerc = totalSpace > 0 ? (int)((freeSpace / totalSpace) * 100) : 0;
    return floatToString(freeSpace, 2) + " GB / " + floatToString(totalSpace, 2) + " GB (" + to_string(freeSpacePerc) +
           "%)";
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
int System::runAndWait(const string &exe, const vector<string> &args, const string &cwd) {
    string line = "CMD line to execute: '" + exe + "'";
    for (const string &arg : args) {
        line += " '" + arg + "'";
    }
    if (!cwd.empty()) {
        line += " (in " + cwd + ")";
    }
    PLOG_INFO << line;

#ifdef _WIN32
    // one command line, quoted by the rules CommandLineToArgvW / the CRT undo: an argument with a space,
    // a tab or a quote goes in quotes, backslashes before a quote (or the closing one) are doubled
    wstring cmd;
    auto quoted = [](const string &arg) {
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
    };
    cmd = quoted(exe);
    for (const string &arg : args) {
        cmd += L" " + quoted(arg);
    }
    vector<wchar_t> buffer(cmd.begin(), cmd.end());
    buffer.push_back(L'\0');
    wstring dir = wide(cwd);

    STARTUPINFOW si;
    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi;
    memset(&pi, 0, sizeof(pi));
    // CREATE_NO_WINDOW: a console program (a script through cmd, curl) gets no console window; a GUI
    // program is unaffected
    if (!CreateProcessW(nullptr, buffer.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW | CREATE_UNICODE_ENVIRONMENT,
                        nullptr, dir.empty() ? nullptr : dir.c_str(), &si, &pi)) {
        PLOG_WARNING << "could not start " << exe << ": error " << GetLastError();
        return -1;
    }
    CloseHandle(pi.hThread);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    PLOG_INFO << exe << " exited with " << code;
    return static_cast<int>(code);
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
    if (waitpid(pid, &status, 0) == -1) {
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
