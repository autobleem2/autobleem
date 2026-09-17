//
// ThemeConverter: an old theme folder (theme.ini + a copy of the console's data tree) becomes a new one
// (theme.json + only the files the UI uses).
//
#pragma once

#include "../main.h"

#include <string>
#include <vector>

//******************
// ThemeConverter
//******************
// A theme used to be theme.ini next to a full copy of /usr/sony/share/data (images/, sounds/, font/ - some
// 330 files) of which autobleem-gui reads 17 images, 2 fonts and 5 sounds by hard-coded PSC name. The
// new folder is theme.json (ThemeSpec) naming every file by its role, the launcher images renamed to
// those roles, and nothing else under images/, sounds/ and font/.
//
// convert() runs in an order that never leaves the folder unreadable: theme.json is written first (naming
// the role files), then the launcher images are renamed, then theme.ini, colors.ini and every file under
// images/, sounds/ and font/ that theme.json does not name are deleted, empty directories with them.
// Files at the theme's root that theme.json does not name (credit.txt, a readme) are left alone.
//
// Theme::load() calls it on a theme whose folder still has the old layout; tools/theme_convert does it
// for a whole themes/ directory ahead of time.
class ThemeConverter {
public:
    // a folder with no theme.json that has a theme.ini, or launcher images under the old names
    static bool needsConversion(const std::string &themeDir);

    // converts in place. False when theme.json could not be written - the folder is then untouched.
    static bool convert(const std::string &themeDir);

    // the theme.json contents an old folder gives, without touching it: an existing theme.json, then
    // theme.ini and colors.ini over it, then whichever launcher images, fonts and sounds are on disk. Files
    // are named by their new role names whether or not the rename has happened (convert() does both).
    static ThemeSpec specFor(const std::string &themeDir);

    // the old theme.ini's keys as a ThemeSpec (the classic UI's values and the music); nothing about files
    // on disk. A group of keys (a rect, a font) is taken only when every key of the group is there.
    static ThemeSpec specFromIni(const IniFile &ini);

    // one launcher image role: the old names tried in order, and the new name
    struct Role {
        std::vector<std::string> oldNames;   // relative to <theme>/images, first one found wins
        std::string newName;                 // relative to <theme>/images
        std::string *(*field)(ThemeSpec &);  // the ThemeSpec field the role names
    };
    static const std::vector<Role> &launcherRoles();
};
