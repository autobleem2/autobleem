//
// theme_convert: unpacks every <name>.zip under a themes directory and converts every old-layout theme
// folder there to the theme.json layout, the same way autobleem-gui does on first load
// (core/services/theme_installer.h, theme_converter.h).
//
//     theme_convert <themesDir>        every zip, then every sub-folder that needs it
//     theme_convert --one <themeDir>   just that theme folder
//
#include "core/services/theme_converter.h"
#include "core/services/theme_installer.h"

#include <iostream>

using namespace std;

// the colours the bridge derived (G6c2), so a theme's author can see them and hand-edit theme.json
static void printRoles(const string &themeDir) {
    const string json = themeDir + sep + "theme.json";
    ThemeSpec spec;
    if (!spec.load(json))
        return;
    const auto &c = spec.launcher.colors;
    const ableem::ThemeSheet sheet = ableem::readThemeSheet(json);
    cout << "  colours: sheet " << sheet.color.toHex() << " alpha " << sheet.alpha << ", text " << c.text.toHex()
         << ", row " << c.row.color.toHex() << ", secondary " << c.secondary.toHex() << ", heading "
         << c.heading.color.toHex() << ", edge " << c.edge.color.toHex() << ", selection band "
         << c.selectionBand.color.toHex() << endl;
}

int main(int argc, char *argv[]) {
    if (argc == 3 && string(argv[1]) == "--one") {
        const string dir = DirEntry::removeSeparatorFromEndOfPath(argv[2]);
        if (!ThemeConverter::needsConversion(dir)) {
            cout << dir << ": nothing to do" << endl;
            return 0;
        }
        if (!ThemeConverter::convert(dir))
            return 1;
        printRoles(dir);
        return 0;
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
    for (const string &name : ThemeInstaller::installZips(themesDir))
        cout << name << ": installed from zip" << endl;
    for (const DirEntry &entry : DirEntry::diru_DirsOnly(themesDir)) {
        const string dir = themesDir + sep + entry.name;
        if (!ThemeConverter::needsConversion(dir)) {
            cout << entry.name << ": nothing to do" << endl;
            continue;
        }
        if (!ThemeConverter::convert(dir)) {
            failures++;
            continue;
        }
        cout << entry.name << ": converted" << endl;
        printRoles(dir);
    }
    return failures == 0 ? 0 : 1;
}
