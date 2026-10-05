// abdialog's pure part. See the header.
#include "dialog_cmd.h"

#include <algorithm>
#include <cctype>

using namespace std;

namespace abdialog {

namespace {

string baseName(const string &path) {
    const size_t cut = path.find_last_of("/\\");
    return cut == string::npos ? path : path.substr(cut + 1);
}

string lower(string text) {
    transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(tolower(c)); });
    return text;
}

bool blank(const string &text) {
    return text.find_first_not_of(" \t\r\n") == string::npos;
}

// splits at tabs into at most `fields` pieces; the last piece keeps any tab in it
vector<string> splitTabs(const string &line, size_t fields) {
    vector<string> parts;
    size_t from = 0;
    while (parts.size() + 1 < fields) {
        const size_t tab = line.find('\t', from);
        if (tab == string::npos)
            break;
        parts.push_back(line.substr(from, tab - from));
        from = tab + 1;
    }
    parts.push_back(line.substr(from));
    return parts;
}

void addText(Command &command, const string &text) {
    // the two characters \n are a line break, as the echo of the 2020 script made them
    string line;
    for (size_t i = 0; i < text.size(); i++) {
        if (text[i] == '\\' && i + 1 < text.size() && text[i + 1] == 'n') {
            command.lines.push_back(line);
            line.clear();
            i++;
        } else if (text[i] != '\r') {
            line += text[i];
        }
    }
    command.lines.push_back(line);
}

} // namespace

//*******************************
// detectMode
//*******************************
bool detectMode(const string &argv0, vector<string> &args, Mode &mode) {
    const string name = lower(baseName(argv0));
    if (name == "sdl_display") {
        mode = Mode::Display;
        return true;
    }
    if (name == "sdl_choicedisplay") {
        mode = Mode::Choice;
        return true;
    }
    if (!args.empty() && (args[0] == "text" || args[0] == "choice")) {
        mode = args[0] == "text" ? Mode::Display : Mode::Choice;
        args.erase(args.begin());
        return true;
    }
    return false;
}

//*******************************
// parseArgs
//*******************************
bool parseArgs(const vector<string> &args, Args &out, string &error) {
    out = Args();
    for (size_t i = 0; i < args.size(); i++) {
        string *target = nullptr;
        if (args[i] == "-file")
            target = &out.file;
        else if (args[i] == "-controller-db")
            target = &out.controllerDb;
        else if (args[i] == "-only")
            target = &out.only;
        else if (args[i] == "-img")
            target = &out.image;
        else {
            error = "unknown option " + args[i];
            return false;
        }
        if (i + 1 >= args.size()) {
            error = args[i] + " needs a value";
            return false;
        }
        *target = args[++i];
    }
    if (out.file.empty()) {
        error = "-file is missing";
        return false;
    }
    return true;
}

//*******************************
// parseCommandFile
//*******************************
Command parseCommandFile(const string &content) {
    Command command;
    bool inText = false; // the last record was a text: a line with no record name continues it
    size_t start = 0;
    while (start <= content.size()) {
        size_t end = content.find('\n', start);
        if (end == string::npos)
            end = content.size();
        string line = content.substr(start, end - start);
        start = end + 1;
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        const string verb = line.substr(0, line.find('\t'));
        if (verb == "IMAGE") {
            const vector<string> parts = splitTabs(line, 4);
            if (parts.size() == 4)
                command.image = parts[3];
            inText = false;
        } else if (verb == "FTEXT" || verb == "FTEXTWBG") {
            const vector<string> parts = splitTabs(line, verb == "FTEXT" ? 9 : 10);
            const size_t want = verb == "FTEXT" ? 9 : 10;
            if (parts.size() == want)
                addText(command, parts[want - 1]);
            inText = parts.size() == want;
        } else if (inText) {
            command.lines.push_back(line); // the text went on over the line break
        }
    }
    // a blank text adds nothing, the ends hold no blank lines
    while (!command.lines.empty() && blank(command.lines.back()))
        command.lines.pop_back();
    size_t first = 0;
    while (first < command.lines.size() && blank(command.lines[first]))
        first++;
    command.lines.erase(command.lines.begin(), command.lines.begin() + static_cast<long>(first));
    for (string &line : command.lines)
        if (blank(line))
            line.clear();
    return command;
}

//*******************************
// the answer
//*******************************
const char *answerLetters() {
    return "XOST";
}

string allowedLetters(const string &only) {
    string result;
    for (const char *letter = answerLetters(); *letter; letter++) {
        for (char c : only) {
            if (toupper(static_cast<unsigned char>(c)) == *letter) {
                result += *letter;
                break;
            }
        }
    }
    return result.empty() ? string(answerLetters()) : result;
}

int answerCode(char letter) {
    const char upper = static_cast<char>(toupper(static_cast<unsigned char>(letter)));
    for (int i = 0; answerLetters()[i]; i++)
        if (answerLetters()[i] == upper)
            return AnswerBase + i;
    return -1;
}

char answerLetter(int code) {
    return code >= AnswerBase && code < AnswerBase + 4 ? answerLetters()[code - AnswerBase] : 0;
}

//*******************************
// presetFor
//*******************************
Preset presetFor(const string &imagePath) {
    const string name = lower(baseName(imagePath));
    if (name.find("controller_select") != string::npos)
        return Preset::Controller;
    if (name.find("win311_select") != string::npos)
        return Preset::Windows;
    return Preset::None;
}

} // namespace abdialog
