#ifndef ABPAD_PAD_LIST_H
#define ABPAD_PAD_LIST_H

// The pads abpadd sees, as `abpadd --list` prints them and the launcher's page reads them: one line per pad, fields
// separated by tabs, the name last. Read-only - the query opens each pad, looks, and closes it.
//
//     pad <TAB> index <TAB> connected <TAB> driver <TAB> mapping <TAB> buttons <TAB> axes <TAB> hats <TAB> guid <TAB> name
//
//   connected  1 when the daemon could open the pad (0: SDL lists it but it cannot be read)
//   driver     the SDL driver the pad's GUID says it came through: "hidapi", "evdev" or "?"
//   mapping    "database" when gamecontrollerdb.txt knows the pad, "guessed" when the daemon would make a mapping up
//
// Nothing else is printed on stdout, so a pad-less run is an empty output. A line that is not a pad line is ignored by
// the reader, so the format can grow a field at the end of the line's fixed part without breaking an older launcher.

#include <string>
#include <vector>

namespace abpad {

struct PadListEntry {
    int index = 0;
    bool connected = false;
    std::string driver;
    std::string mapping;
    int buttons = 0;
    int axes = 0;
    int hats = 0;
    std::string guid;
    std::string name;
};

// one line, without the newline; tabs and line breaks inside the name or guid are turned into spaces
std::string formatPadListLine(const PadListEntry &entry);
// false for a line that is not a well-formed pad line
bool parsePadListLine(const std::string &line, PadListEntry &out);
// every pad line of the lines, in order
std::vector<PadListEntry> parsePadList(const std::vector<std::string> &lines);

} // namespace abpad

#endif
