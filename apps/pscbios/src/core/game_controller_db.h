//
// GameControllerDb: a gamecontrollerdb.txt as the mapping wizard edits it - every line kept as it was,
// one mapping replaced (or appended under the "#AutoBleem" marker) and the file written back. A mapping
// line is "<guid>,<name>,<k:v,...>,platform:<os>,"; the (platform, guid) pair identifies it.
//
#pragma once

#include <string>
#include <vector>

//******************
// GameControllerDb
//******************
class GameControllerDb {
public:
    struct Line {
        std::string text;
        std::string guid;     // "" for a comment or a line without one
        std::string platform; // the platform:<os> value, "" when the line has none
        bool comment = false;
    };

    // reads `path`; an unreadable file gives an empty db, which save() then creates. The "#AutoBleem"
    // marker is appended when the file has none.
    void load(const std::string &path);
    // replaces the line for (platform, guid), else appends `mappingLine` at the end (the AutoBleem section)
    void replaceMapping(const std::string &guid, const std::string &platform, const std::string &mappingLine);
    bool save(const std::string &path) const;

    const std::vector<Line> &lines() const { return lines_; }
    static Line parse(const std::string &text);
    // the guid and platform of a mapping line - "" when the line has none
    static std::string guidOf(const std::string &mappingLine);
    static std::string platformOf(const std::string &mappingLine);

    static const char *const Marker; // "#AutoBleem"

private:
    std::vector<Line> lines_;
};
