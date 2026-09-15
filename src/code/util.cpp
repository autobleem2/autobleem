#include "util.h"
#include "main.h"

#include <fstream>
#include <array>
#include <cerrno>
#include <climits>
#include <memory>
#ifndef _WIN32
#include <sys/wait.h>
#endif
#include <unistd.h>
#include <iomanip>
#include <string.h>
#include <sstream>
#include <iostream>
#include <stdio.h>

using namespace std;

//*******************************
// Util::powerOff
//*******************************
void Util::powerOff()
{
#ifdef AB_DEBUG_HOST
    exit(0);
#else
    Util::execUnixCommand("shutdown -h now");
    exit(0);
#endif
}

//*******************************
// Util::getAvailableSpace
//*******************************
/*
 * Return the available space of a usb device
 */
string Util::getAvailableSpace(){
#ifdef AB_DEBUG_HOST
    return "x86 - does not care about free space - Does not work on mac";
    #else
    string str;
    int gb = 1024 * 1024;
    string dfResult;
    float freeSpace;
    float totalSpace;
    int freeSpacePerc;
    freeSpace = ((float)(stoi(execUnixCommand("df | grep \"media\" | head -1 | awk '{print $4}'"))))/gb;
    totalSpace = ((float)(stoi(execUnixCommand("df | grep \"media\" | head -1 | awk '{print $2}'"))))/gb;
    freeSpacePerc = (freeSpace / totalSpace) * 100;
    str = floatToString(freeSpace, 2) + " GB / " + floatToString(totalSpace,2)+ " GB (" + to_string(freeSpacePerc)+"%)";
    return str;
#endif
}

//*******************************
// Util::execUnixCommad
//*******************************
/*
 * Execute a shell command and return output
 */
string Util::execUnixCommand(const char* cmd){
    array<char, 128> buffer;
    string result;
    cout << "Exec:" << cmd << endl;
    unique_ptr<FILE, decltype(&pclose)> pipe(popen(cmd, "r"), pclose);
    if (!pipe) {
        cout << "popen() failed for: " << cmd << endl;
        return result;  // never throw: there is no handler anywhere and an abort() takes the whole UI down
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    result.erase(remove(result.begin(), result.end(), '\n'), result.end());
    return result;
}

//*******************************
// Util::runAndWait
//*******************************
// fork + exec the program and wait for it to finish.
// returns the exit status of the program, or -1 if it could not be started.
int Util::runAndWait(const string &exe, const vector<string> &args) {
    cout << "CMD line to execute: '" << exe << "'";
    for (const string &arg : args) {
        cout << " '" << arg << "'";
    }
    cout << endl;

#ifdef _WIN32
    cout << "runAndWait is not supported on Windows" << endl;
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
        cout << "fork() failed: " << strerror(errno) << endl;
        return -1;
    }
    if (pid == 0) {
        // child. if exec fails we must not return into the parent's code path (that would run a second GUI).
        execvp(exe.c_str(), const_cast<char **>(argv.data()));
        _exit(127);
    }

    int status = 0;
    if (waitpid(pid, &status, 0) == -1) {
        cout << "waitpid() failed: " << strerror(errno) << endl;
        return -1;
    }
    if (WIFEXITED(status)) {
        int exitCode = WEXITSTATUS(status);
        if (exitCode == 127) {
            cout << "could not start: " << exe << endl;
        }
        return exitCode;
    }
    if (WIFSIGNALED(status)) {
        cout << exe << " was killed by signal " << WTERMSIG(status) << endl;
    }
    return -1;
#endif
}

//*******************************
// Util::execFork
//*******************************
// kept for the pscbios launch in gui.cpp. argvNew is argv[0] ... null terminator
void Util::execFork(const char *cmd,  vector<const char *> argvNew)
{
    cout << "calling Util::execFork()" << endl;
    vector<string> args;
    for (size_t i = 1; i < argvNew.size(); i++) {
        if (argvNew[i] != nullptr) {
            args.push_back(argvNew[i]);
        }
    }
    runAndWait(cmd, args);
}

//*******************************
// Util::dumpMemory
//*******************************
void Util::dumpMemory(const  char *p, int count) {
    for (int i=0; i < count; ++i) {
        printf("%x, ", (unsigned int) *p++);
        if (i %16 == 15 || i == count-1)
            cout << endl;
    }
}

//*******************************
// Util::getRandomNumber
//*******************************
unsigned int Util::getRandomNumber() {
    static bool firstTime{true};
    if (firstTime) {
        srand(time(nullptr));
        firstTime = false;
    }

    return rand();
}

//*******************************
// Util::getRandomIndex
// pass 100, get a random index between 0 and 99
//*******************************
unsigned int Util::getRandomIndex(unsigned int size) {
    return getRandomNumber() % size;
}
