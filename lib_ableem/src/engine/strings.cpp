#include "ableem/engine/strings.h"

#include <cerrno>
#include <climits>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <sstream>

using namespace std;

namespace ableem {

//*******************************
// Strings::replaceAll
//*******************************
void Strings::replaceAll(string &str, const string &from, const string &to) {
    if (from.empty())
        return;
    size_t start_pos = 0;
    while ((start_pos = str.find(from, start_pos)) != string::npos) {
        str.replace(start_pos, from.length(), to);
        start_pos += to.length(); // In case 'to' contains 'from', like replacing 'x' with 'yx'
    }
}

//*******************************
// Strings::escapeCommas
//*******************************
string Strings::escapeCommas(string input) {
    replaceAll(input, "|", "||");
    replaceAll(input, ",", "|@");
    return input;
}

//*******************************
// Strings::unescapeCommas
//*******************************
string Strings::unescapeCommas(string input) {
    replaceAll(input, "|@", ",");
    replaceAll(input, "||", "|");
    return input;
}

//*******************************
// Strings::isInteger
//*******************************
bool Strings::isInteger(const char *input) {
    size_t ln = strlen(input);
    for (size_t i = 0; i < ln; i++) {
        if (!isdigit(input[i])) {
            return false;
        }
    }
    return true;
}

//*******************************
// Strings::toInt
//*******************************
// parses leading whitespace, an optional sign and digits. anything else (or an empty string) returns def.
int Strings::toInt(const string &s, int def) {
    const char *start = s.c_str();
    char *end = nullptr;
    errno = 0;
    long value = strtol(start, &end, 10);
    if (end == start || errno == ERANGE || value > INT_MAX || value < INT_MIN) {
        return def;
    }
    return (int) value;
}

//*******************************
// Strings::compareCaseInsensitive
//*******************************
bool Strings::compareCaseInsensitive(string first, string second) {
    return lcase(first) == lcase(second);
}

//*******************************
// Strings::floatToString
//*******************************
string Strings::floatToString(float f, int n) {
    ostringstream stringStream;
    stringStream << fixed << setprecision(n) << f;
    return stringStream.str();
}

//*******************************
// Strings::commaSep
//*******************************
string Strings::commaSep(const string &s, int pos) {
    vector<string> v;
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
    if (pos < (int) v.size()) {
        return v[pos];
    }
    return "";
}

//*******************************
// Strings::ltrim
//*******************************
string Strings::ltrim(const string &s) {
    size_t start = s.find_first_not_of(" \n\r\t\f\v");
    return (start == string::npos) ? "" : s.substr(start);
}

//*******************************
// Strings::rtrim
//*******************************
string Strings::rtrim(const string &s) {
    size_t end = s.find_last_not_of(" \n\r\t\f\v");
    return (end == string::npos) ? "" : s.substr(0, end + 1);
}

//*******************************
// Strings::trim
//*******************************
string Strings::trim(const string &s) {
    return rtrim(ltrim(s));
}

//*******************************
// Strings::getStringWithinChar
//*******************************
string Strings::getStringWithinChar(string s, char del) {
    int first = s.find(del);
    int last = s.find_last_of(del);
    return s.substr(first + 1, last - first - 1);
}

//*******************************
// Strings::removeCharsFromString
//*******************************
void Strings::removeCharsFromString(string &str, string charsToRemove) {
    for (char ch : charsToRemove)
        str.erase(std::remove(str.begin(), str.end(), ch), str.end());
}

//*******************************
// Strings::getlineRemoveCR
// does a getline. if it's a Windows file the CR at the end is removed.
//*******************************
istream &Strings::getlineRemoveCR(istream &is, string &str) {
    istream &ret = getline(is, str);
    if (!str.empty() && *str.rbegin() == '\r')
        str.erase(str.length() - 1, 1);
    return ret;
}

//*******************************
// Strings::removeComment
// remove "#" to end of line
//*******************************
void Strings::removeComment(string &str) {
    auto it = str.find("#");
    if (it != str.npos)
        str.erase(it);
}

//*******************************
// Strings::cleanPublisherString
// remove any trailing "." or space or " ."
//*******************************
void Strings::cleanPublisherString(string &pub) {
    if (pub.size() > 0 && pub.back() == '.')
        pub.pop_back();
    if (pub.size() > 0 && pub.back() == ' ')
        pub.pop_back();
}

//*******************************
// Strings::getTokens
//*******************************
vector<string> Strings::getTokens(const string &str, char delim) {
    istringstream ss(str);
    string token;
    vector<string> ret;
    while (getline(ss, token, delim)) {
        if (token != "")
            ret.push_back(token);
    }
    return ret;
}

} // namespace ableem
