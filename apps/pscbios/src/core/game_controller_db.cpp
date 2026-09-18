//
// GameControllerDb: gamecontrollerdb.txt with one mapping replaced.
//
#include "game_controller_db.h"
#include "core/main.h"

#include <ableem/engine/log.h>

#include <fstream>

using namespace std;

const char *const GameControllerDb::Marker = "#AutoBleem";

//*******************************
// GameControllerDb::parse / guidOf / platformOf
//*******************************
GameControllerDb::Line GameControllerDb::parse(const string &text) {
    Line line;
    line.text = Strings::trim(text);
    line.comment = !line.text.empty() && line.text[0] == '#';
    if (!line.comment && !line.text.empty()) {
        line.guid = guidOf(line.text);
        line.platform = platformOf(line.text);
    }
    return line;
}

string GameControllerDb::guidOf(const string &mappingLine) {
    return mappingLine.substr(0, mappingLine.find(','));
}

string GameControllerDb::platformOf(const string &mappingLine) {
    size_t pos = mappingLine.find("platform:");
    if (pos == string::npos)
        return "";
    size_t end = mappingLine.find(',', pos);
    return mappingLine.substr(pos + 9, end == string::npos ? string::npos : end - pos - 9);
}

//*******************************
// GameControllerDb::load
//*******************************
void GameControllerDb::load(const string &path) {
    lines_.clear();
    bool markerFound = false;
    ifstream in(path);
    if (!in) {
        PLOG_WARNING << "No gamecontrollerdb.txt at " << path << " - starting an empty one";
    }
    string text;
    while (getline(in, text)) {
        Line line = parse(text);
        if (line.text.empty())
            continue;
        if (line.text == Marker)
            markerFound = true;
        lines_.push_back(line);
    }
    if (!markerFound)
        lines_.push_back(parse(Marker));
    PLOG_INFO << "gamecontrollerdb.txt " << path << ": " << lines_.size() << " lines";
}

//*******************************
// GameControllerDb::replaceMapping
//*******************************
void GameControllerDb::replaceMapping(const string &guid, const string &platform, const string &mappingLine) {
    bool replaced = false;
    for (Line &line : lines_) {
        if (!line.comment && line.guid == guid && line.platform == platform) {
            line = parse(mappingLine);
            replaced = true;
        }
    }
    if (!replaced)
        lines_.push_back(parse(mappingLine));
}

//*******************************
// GameControllerDb::save
//*******************************
bool GameControllerDb::save(const string &path) const {
    ofstream out(path, ios::binary); // LF on every platform: SDL reads it, and so does the console
    if (!DirEntry::checkWritable(out, path))
        return false;
    for (const Line &line : lines_)
        out << line.text << "\n";
    return out.good();
}
