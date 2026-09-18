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

#ifndef AB_DEBUG_HOST
namespace {
string floatToString(float value, int precision) {
    ostringstream oss;
    oss << fixed << setprecision(precision) << value;
    return oss.str();
}
}
#endif

//*******************************
// System::powerOff
//*******************************
// The one way the app powers the console off: the launcher's L2+R2, the classic menu's L2+R2 and the
// console's power button all come here. sync() first, so the last log lines and any ini just written
// reach the USB stick before the halt.
void System::powerOff()
{
#ifdef AB_DEBUG_HOST
    exit(0);
#else
    System::execUnixCommand("shutdown -h now");
    sync();
    exit(0);
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
    param.sched_priority = 0;   // SCHED_IDLE requires 0
    if (sched_setscheduler(0, SCHED_IDLE, &param) != 0)
        nice(19);   // SCHED_IDLE refused (needs a capability some setups don't grant) - a plain nice bump instead
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
string System::getAvailableSpace(){
#ifdef AB_DEBUG_HOST
    return "x86 - does not care about free space - Does not work on mac";
    #else
    // execUnixCommand returns "" when df fails or nothing under /media is mounted - Strings::toInt makes that
    // a 0 instead of a thrown exception (this branch had never been compiled before the Pi port)
    int gb = 1024 * 1024;
    float freeSpace = (float) Strings::toInt(execUnixCommand("df | grep \"media\" | head -1 | awk '{print $4}'")) / gb;
    float totalSpace = (float) Strings::toInt(execUnixCommand("df | grep \"media\" | head -1 | awk '{print $2}'")) / gb;
    int freeSpacePerc = totalSpace > 0 ? (int) ((freeSpace / totalSpace) * 100) : 0;
    return floatToString(freeSpace, 2) + " GB / " + floatToString(totalSpace, 2) + " GB (" + to_string(freeSpacePerc) + "%)";
#endif
}

//*******************************
// System::execUnixCommand
//*******************************
/*
 * Execute a shell command and return output
 */
string System::execUnixCommand(const char* cmd){
    array<char, 128> buffer;
    string result;
    PLOG_INFO << "Exec:" << cmd;
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        PLOG_WARNING << "popen() failed for: " << cmd;
        return result;  // never throw: there is no handler anywhere and an abort() takes the whole UI down
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
int System::runAndWait(const string &exe, const vector<string> &args) {
    string line = "CMD line to execute: '" + exe + "'";
    for (const string &arg : args) {
        line += " '" + arg + "'";
    }
    PLOG_INFO << line;

#ifdef _WIN32
    PLOG_INFO << "runAndWait is not supported on Windows";
    return -1;
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
