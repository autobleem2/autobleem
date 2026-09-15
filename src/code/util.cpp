#include "util.h"
#include "main.h"

#include <fstream>
#include <array>
#include <cerrno>
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
// Util::replaceAll
//*******************************
void Util::replaceAll(string &str, const string &from, const string &to) {
    if (from.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}

//*******************************
// Util::escape
//*******************************
string Util::escape(string input) {
    replaceAll(input, "|", "||");
    replaceAll(input, ",", "|@");
    return input;
}

//*******************************
// Util::decode
//*******************************
string Util::decode(string input) {
    replaceAll(input, "|@", ",");
    replaceAll(input, "||", "|");
    return input;
}

//*******************************
// Util::isInteger
//*******************************
bool Util::isInteger(const char *input) {
    size_t ln = strlen(input);
    for (size_t i = 0; i < ln; i++) {
        if (!isdigit(input[i])) {
            return false;
        }
    }
    return true;
}

//*******************************
// Util::compareCaseInsensitive
//*******************************
bool Util::compareCaseInsensitive(string first, string second) {
    return lcase(first) == lcase(second);
}


//*******************************
// Util::readChar
//*******************************
unsigned char Util::readChar(ifstream *stream) {
    unsigned char c;
    stream->read((char *) &c, 1);
    return c;
}

//*******************************
// Util::readString
//*******************************
string Util::readString(int size, ifstream *stream) {
    char str[size + 1];
    str[size] = 0;
    stream->read(str, size);
    return str;
}

//*******************************
// Util::skipZeros
//*******************************
void Util::skipZeros(ifstream *stream) {
    char c = readChar(stream);
    while (!stream->eof() && !stream->fail() && c == 00) {
        c = readChar(stream);
    }
    stream->seekg(-1, ios::cur);
}

//*******************************
// Util::readString
//*******************************
string Util::readString(ifstream *stream) {
    string str;
    char c = readChar(stream);
    while (!stream->eof() && !stream->fail() && c != 00) {
        str = str + c;
        c = readChar(stream);
    }
    return str;
}

//*******************************
// Util::readDword
//*******************************
unsigned long Util::readDword(ifstream *stream) {
    unsigned long res = 0;
    unsigned long c;
    c = readChar(stream);
    res += c;
    c = readChar(stream);
    res += c << (1 * 8);
    c = readChar(stream);
    res += c << (2 * 8);
    c = readChar(stream);
    res += c << (3 * 8);
    return res;
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
// Util::floatToString
//*******************************
/*
 * Convert a float f to a string with precision of n
 */
string Util::floatToString(float f, int n){
    ostringstream stringStream;
    stringStream << fixed << setprecision(n) << f;
    return stringStream.str();
}

//*******************************
// Util::commaSep
//*******************************
string Util::commaSep(const string& s, int pos) {
    vector<string> v;
    v.clear();
    char c = ',';
    int i = 0;
    int j = s.find(c);

    while (j >= 0) {
        v.push_back(s.substr(i, j - i));
        i = ++j;
        j = s.find(c, j);

        if (j < 0) {
            v.push_back(s.substr(i, s.length()));
        }
    }
    if (pos<v.size())
    {
        return v[pos];
    }
    return "";
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
// Util::ltrim
//*******************************
// Left trimming
string Util::ltrim(const string& s){
    size_t start = s.find_first_not_of(" \n\r\t\f\v");
    return (start == string::npos) ? "" : s.substr(start);
}

//*******************************
// Util::rtrim
//*******************************
// Right trimming
string Util::rtrim(const string& s){
    size_t end = s.find_last_not_of(" \n\r\t\f\v");
    return (end == string::npos) ? "" : s.substr(0, end + 1);
}

//*******************************
// Util::trim
//*******************************
// Trimming both left and right
string Util::trim(const string &s) {
    return rtrim(ltrim(s));
}

//*******************************
// Util::getStringWithinChar
//*******************************
/*
 * Return a char between separator like :
 * Super "GAMENAME" baby
 * Will return
 * GAMENAME
 */
string Util::getStringWithinChar(string s, char del) {
    int first = s.find(del);
    int last = s.find_last_of(del);
    return s.substr(first+1, last-first-1);
}

//*******************************
// Util::removeCharsFromString
//*******************************
void Util::removeCharsFromString(string& str, string charsToRemove) {
    for (char ch : charsToRemove)
        str.erase( std::remove(str.begin(), str.end(), ch), str.end() );
}

//*******************************
// Util::getlineRemoveCR
// does a getline.  if it's a Windows file the CR at the end is removed.
//*******************************
istream& Util::getlineRemoveCR(std::istream& is, std::string& str) {
    istream& ret = getline(is, str);
    if (!str.empty() && *str.rbegin() == '\r')
        str.erase(str.length()-1, 1);
    return ret;
}

//*******************************
// Util::removeComment
// remove "#" to end of line
//*******************************
void Util::removeComment(std::string& str) {
    auto it = str.find("#");
    if (it != str.npos)
        str.erase(it);
}

//*******************************
// Util::cleanPublisherString
// remove any trailing "." or space or " ."
//*******************************
void Util::cleanPublisherString(std::string & pub)
{
    if (pub.size() > 0 && pub.back() == '.')
        pub.pop_back();
    if (pub.size() > 0 && pub.back() == ' ')
        pub.pop_back();
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
// Util::getTokens
//*******************************
vector<string> Util::getTokens(const string& str, char delim) {
    istringstream ss(str);
    string token;
    vector<string> ret;
    while (getline(ss, token, delim)) {
        if (token != "")
            ret.push_back(token);
    }

    return ret;
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
