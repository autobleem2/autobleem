#include "core/pad_list.h"

#include <cstdlib>

using namespace std;

namespace abpad {

namespace {

const char *const kPrefix = "pad";
constexpr size_t FixedFields = 9; // everything before the name

string oneLine(const string &text) {
    string result = text;
    for (char &c : result) {
        if (c == '\t' || c == '\n' || c == '\r') {
            c = ' ';
        }
    }
    return result;
}

bool wholeNumber(const string &text, int &out) {
    if (text.empty() || text.size() > 9) {
        return false;
    }
    for (char c : text) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    out = atoi(text.c_str());
    return true;
}

} // namespace

//*******************************
// formatPadListLine
//*******************************
string formatPadListLine(const PadListEntry &entry) {
    string line = kPrefix;
    line += '\t' + to_string(entry.index);
    line += '\t' + string(entry.connected ? "1" : "0");
    line += '\t' + oneLine(entry.driver);
    line += '\t' + oneLine(entry.mapping);
    line += '\t' + to_string(entry.buttons);
    line += '\t' + to_string(entry.axes);
    line += '\t' + to_string(entry.hats);
    line += '\t' + oneLine(entry.guid);
    line += '\t' + oneLine(entry.name);
    return line;
}

//*******************************
// parsePadListLine
//*******************************
bool parsePadListLine(const string &raw, PadListEntry &out) {
    string line = raw;
    while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) {
        line.erase(line.size() - 1);
    }
    vector<string> fields;
    size_t start = 0;
    while (fields.size() < FixedFields) { // the name is the rest, whatever it holds
        size_t tab = line.find('\t', start);
        if (tab == string::npos) {
            return false;
        }
        fields.push_back(line.substr(start, tab - start));
        start = tab + 1;
    }
    fields.push_back(line.substr(start));
    if (fields[0] != kPrefix) {
        return false;
    }

    PadListEntry entry;
    int connected = 0;
    if (!wholeNumber(fields[1], entry.index) || !wholeNumber(fields[2], connected) ||
        !wholeNumber(fields[5], entry.buttons) || !wholeNumber(fields[6], entry.axes) ||
        !wholeNumber(fields[7], entry.hats)) {
        return false;
    }
    entry.connected = connected != 0;
    entry.driver = fields[3];
    entry.mapping = fields[4];
    entry.guid = fields[8];
    entry.name = fields[9];
    out = entry;
    return true;
}

//*******************************
// parsePadList
//*******************************
vector<PadListEntry> parsePadList(const vector<string> &lines) {
    vector<PadListEntry> pads;
    for (const string &line : lines) {
        PadListEntry entry;
        if (parsePadListLine(line, entry)) {
            pads.push_back(entry);
        }
    }
    return pads;
}

} // namespace abpad
