/*
 * File:   util.h
 * Author: screemer
 *
 * Created on 11 grudnia 2018, 23:12
 */

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
class Util : public ableem::Strings {
public:
    static std::string getAvailableSpace();
    static std::string execUnixCommand(const char* cmd);   // run a shell command, return its stdout ("" on failure)
    // fork + exec 'exe' with 'args' (argv[0] is added for you) and wait. returns exit code, -1 if it could not run.
    static int runAndWait(const std::string& exe, const std::vector<std::string>& args);
    static void execFork(const char* cmd,  std::vector<const char *> argvNew);

    static void powerOff();

    static void dumpMemory(const char *p, int count);

    static unsigned int getRandomNumber();
    static unsigned int getRandomIndex(unsigned int size);
};
