//
// theme_convert: converts every old-layout theme folder under a themes directory to the theme.json
// layout, the same way autobleem-gui does on first load (core/services/theme_converter.h).
//
//     theme_convert <themesDir>        every sub-folder that needs it
//     theme_convert --one <themeDir>   just that theme
//
#include "core/services/theme_converter.h"

#include <iostream>

using namespace std;

int main(int argc, char *argv[]) {
    if (argc == 3 && string(argv[1]) == "--one") {
        const string dir = DirEntry::removeSeparatorFromEndOfPath(argv[2]);
        if (!ThemeConverter::needsConversion(dir)) {
            cout << dir << ": nothing to do" << endl;
            return 0;
        }
        return ThemeConverter::convert(dir) ? 0 : 1;
    }
    if (argc != 2) {
        cout << "usage: theme_convert <themesDir> | --one <themeDir>" << endl;
        return 2;
    }

    const string themesDir = DirEntry::removeSeparatorFromEndOfPath(argv[1]);
    if (!DirEntry::isDirectory(themesDir)) {
        cout << themesDir << " is not a directory" << endl;
        return 2;
    }

    int failures = 0;
    for (const DirEntry &entry : DirEntry::diru_DirsOnly(themesDir)) {
        const string dir = themesDir + sep + entry.name;
        if (!ThemeConverter::needsConversion(dir)) {
            cout << entry.name << ": nothing to do" << endl;
            continue;
        }
        if (!ThemeConverter::convert(dir)) failures++;
    }
    return failures == 0 ? 0 : 1;
}
