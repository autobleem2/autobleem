//
// Util: the process/console helpers that are not portable enough to live in lib_ableem.
//
#pragma once

#include <string>
#include <vector>
#include <ctime>
#include <ableem/engine/strings.h>
#include "environment.h"    // for AB_DEBUG_HOST

//******************
// Util
//******************
// The string helpers (Util::trim, Util::replaceAll, Util::toInt, ...) are ableem::Strings, inherited so the
// existing call sites keep working. What is left here is process/console specific and stays in the app.
//
// Note the two trim families: ableem::trim(s) (pulled into the global namespace by main.h) modifies s in
// place, Util::trim(s) returns a trimmed copy.
class Util : public ableem::Strings {
public:
    static std::string getAvailableSpace();                // "N GB free" for the status bar, from df on /media

    static std::string execUnixCommand(const char *cmd);   // run a shell command, return its stdout ("" on failure)

    // fork + exec 'exe' with 'args' (argv[0] is added for you) and wait. returns exit code, -1 if it could not
    // run. This is the only fork/exec in the code base - everything that starts a process goes through here.
    static int runAndWait(const std::string &exe, const std::vector<std::string> &args);

    // the old argv-array form, kept for the few remaining callers: argvNew is argv[0] .. nullptr, and it just
    // repackages them for runAndWait.
    static void execFork(const char *cmd, std::vector<const char *> argvNew);

    static void powerOff();                                // halts the console; exits the app on a debug host

    static void dumpMemory(const char *p, int count);      // hex dump to cout, for debugging memory cards

    static unsigned int getRandomNumber();
    static unsigned int getRandomIndex(unsigned int size); // 0 .. size-1
};
