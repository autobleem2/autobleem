//
// System: the process and console helpers - running a program, a shell command, powering off, free space,
// random numbers. What is not portable enough to live in lib_ableem.
//
#pragma once

#include "environment.h" // for AB_DEBUG_HOST

#include <cstdint>
#include <string>
#include <vector>

//******************
// System
//******************
// Was the non-string half of Util (the string half is ableem::Strings, reached as Strings::). Stateless
// calls into the OS, so static; the one that matters for testing, runAndWait, is behind ProcessRunner.
class System {
public:
    // fork + exec 'exe' with 'args' (argv[0] is added for you) and wait, started in 'cwd' when one is given.
    // returns exit code, -1 if it could not run. This is the only fork/exec in the code base - everything
    // that starts a process goes through here (CreateProcess on Windows, where the child gets no console
    // window of its own).
    static int runAndWait(const std::string &exe, const std::vector<std::string> &args, const std::string &cwd = "");

    static std::string execUnixCommand(const char *cmd); // run a shell command, return its stdout ("" on failure)
    // runs a command line through the shell and waits: std::system, except that on Windows the child
    // (cmd.exe and what it starts - curl) gets no console window, so nothing flashes over the launcher.
    // returns the exit status (0 = success), -1 when the shell could not be started. What OnlineAssets and
    // UpdateService run their download commands with.
    static int runShellCommand(const std::string &commandLine);
    // the same, one entry per non-empty line of stdout, trimmed - for a command that lists things
    static std::vector<std::string> execUnixCommandLines(const std::string &cmd);

    static void powerOff(); // halts the console; exits the app on a debug host

    // drops the calling thread (not the process) to the OS's lowest scheduling priority - idle-priority
    // where the platform has it (SCHED_IDLE / THREAD_PRIORITY_IDLE: runs at full speed when nothing else
    // wants the CPU, yields almost completely once something else does), a plain nice(19) otherwise. Used
    // by ScanService's worker thread so a background scan never competes with a running emulator for CPU.
    static void lowerCurrentThreadPriority();

    // the free and total bytes of the filesystem 'path' is on (statvfs / GetDiskFreeSpaceEx); false when
    // the path is not there
    static bool diskSpace(const std::string &path, uint64_t &freeBytes, uint64_t &totalBytes);
    static std::string getAvailableSpace(); // "N GB / M GB (P%)" for the status bar, of the USB root's filesystem

    static unsigned int getRandomNumber();
    static unsigned int getRandomIndex(unsigned int size); // 0 .. size-1
};
