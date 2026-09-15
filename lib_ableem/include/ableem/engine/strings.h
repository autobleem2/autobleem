// lib_ableem - engine: string helpers. Two families, kept apart on purpose:
//   - the free functions modify their argument in place and return it (trim(s), lcase(s), ...)
//   - Strings::xxx take a const reference and return a copy (Strings::trim(s), ...)
#pragma once

#include <algorithm>
#include <cctype>
#include <istream>
#include <string>
#include <vector>

namespace ableem {

//******************
// in-place helpers
//******************

// trim from start
static inline std::string &ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(),
                                    [](unsigned char c) { return !std::isspace(c); }));
    return s;
}

// trim from end
static inline std::string &rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(),
                         [](unsigned char c) { return !std::isspace(c); }).base(), s.end());
    return s;
}

// trim from both ends
static inline std::string &trim(std::string &s) {
    return ltrim(rtrim(s));
}

// converts all, or the first nchars, of the passed string to lower case. a reference to the modified string is returned.
static inline std::string &lcase(std::string &s, int nchars = 0) {
    if (nchars == 0) nchars = s.length();
    for (int i = 0; i < nchars; i++) {
        s[i] = tolower(s[i]);
    }
    return s;
}

// converts all, or the first nchars, of the passed string to upper case. a reference to the modified string is returned.
static inline std::string &ucase(std::string &s, int nchars = 0) {
    if (nchars == 0) nchars = s.length();
    for (int i = 0; i < nchars; i++) {
        s[i] = toupper(s[i]);
    }
    return s;
}

// returns a lower case copy of the string. the passed string is not modified.
static inline std::string toLowerCopy(const std::string &s) {
    std::string temp = s;
    for (auto &c : temp) {
        c = tolower(c);
    }
    return temp;
}

// returns an upper case copy of the string. the passed string is not modified.
static inline std::string toUpperCopy(const std::string &s) {
    std::string temp = s;
    for (auto &c : temp) {
        c = toupper(c);
    }
    return temp;
}

// comparator for std::sort and friends
static inline bool lessCaseInsensitive(const std::string &left, const std::string &right) {
    return toLowerCopy(left) < toLowerCopy(right);
}

//******************
// Strings
//******************
class Strings {
public:
    // "|" -> "||", "," -> "|@" so a name can be stored in a comma separated list (Game.ini "Discs=", autobleem.list)
    static std::string escapeCommas(std::string input);
    static std::string unescapeCommas(std::string input);
    static void replaceAll(std::string &str, const std::string &from, const std::string &to);

    static bool isInteger(const char *input);
    static int toInt(const std::string &s, int def = 0);   // like stoi but returns def instead of throwing
    static bool compareCaseInsensitive(std::string first, std::string second);

    static std::string commaSep(const std::string &input, int pos);  // the pos-th comma separated field, "" if none
    static std::string ltrim(const std::string &s);
    static std::string rtrim(const std::string &s);
    static std::string trim(const std::string &s);
    static std::string getStringWithinChar(std::string s, char del);    // 'Super "GAMENAME" baby', '"' -> GAMENAME
    static void removeCharsFromString(std::string &str, std::string charsToRemove);
    static void removeCRLFFromString(std::string &str) { removeCharsFromString(str, "\r\n"); }
    static std::istream &getlineRemoveCR(std::istream &is, std::string &str);
    static void removeComment(std::string &str);    // remove "#" to end of line
    static void cleanPublisherString(std::string &pub);  // remove any trailing "." or spaces
    static std::vector<std::string> getTokens(const std::string &str, char delim);   // empty tokens are dropped
    static std::string floatToString(float f, int n);   // n decimals
};

} // namespace ableem
