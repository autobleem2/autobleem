//
// System: the process and console helpers - running a program, a shell command, powering off, free space,
// random numbers. What is not portable enough to live in lib_ableem.
//
#pragma once

#include "environment.h"    // for AB_DEBUG_HOST

#include <string>
#include <vector>

//******************
// System
//******************
// Was the non-string half of Util (the string half is ableem::Strings, reached as Strings::). Stateless
// calls into the OS, so static; the one that matters for testing, runAndWait, is behind ProcessRunner.
class System {
public:
    // fork + exec 'exe' with 'args' (argv[0] is added for you) and wait. returns exit code, -1 if it could not
    // run. This is the only fork/exec in the code base - everything that starts a process goes through here.
    static int runAndWait(const std::string &exe, const std::vector<std::string> &args);

    static std::string execUnixCommand(const char *cmd);   // run a shell command, return its stdout ("" on failure)

    static void powerOff();                                // halts the console; exits the app on a debug host

    // drops the calling thread (not the process) to the OS's lowest scheduling priority - idle-priority
    // where the platform has it (SCHED_IDLE / THREAD_PRIORITY_IDLE: runs at full speed when nothing else
    // wants the CPU, yields almost completely once something else does), a plain nice(19) otherwise. Used
    // by ScanService's worker thread so a background scan never competes with a running emulator for CPU.
    static void lowerCurrentThreadPriority();

    static std::string getAvailableSpace();                // "N GB / M GB (P%)" for the status bar, from df on /media

    static unsigned int getRandomNumber();
    static unsigned int getRandomIndex(unsigned int size); // 0 .. size-1
};
