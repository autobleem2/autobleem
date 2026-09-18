//
// ThemeInstaller: <themes>/<name>.zip -> <themes>/<name>/
//

#include "theme_installer.h"
#include "theme_converter.h"

#include <iostream>
#include <ableem/engine/log.h>

using namespace std;

namespace {

bool isZipName(const string &name) {
    return DirEntry::matchExtension(name, "zip");
}

// the folders a zip tool adds that are not the theme (macOS resource forks, hidden folders)
bool isJunkFolder(const string &name) {
    return name.empty() || name[0] == '.' || name.compare(0, 2, "__") == 0;
}

} // namespace

//*******************************
// ThemeInstaller::themeName
//*******************************
string ThemeInstaller::themeName(const string &zipPath) {
    return DirEntry::getFileNameWithoutExtension(DirEntry::getFileNameFromPath(zipPath));
}

//*******************************
// ThemeInstaller::findThemeRoot
//*******************************
string ThemeInstaller::findThemeRoot(const string &dir) {
    if (ThemeConverter::isThemeFolder(dir)) return dir;

    // one folder inside, the theme's files in it
    string only;
    for (const DirEntry &entry : DirEntry::diru_DirsOnly(dir)) {
        if (isJunkFolder(entry.name)) continue;
        if (!only.empty()) return "";   // two candidates: not a theme zip we understand
        only = dir + sep + entry.name;
    }
    if (!only.empty() && ThemeConverter::isThemeFolder(only)) return only;
    return "";
}

//*******************************
// ThemeInstaller::installZip
//*******************************
bool ThemeInstaller::installZip(const string &zipPath, const string &themesDir) {
    const string name = themeName(zipPath);
    const string dest = themesDir + sep + name;
    const string staging = themesDir + sep + "." + name + ".unzip";
    PLOG_INFO << "Installing theme from " << zipPath;

    auto giveUp = [&](const string &why) {
        PLOG_INFO << "Theme " << name << " not installed: " << why;
        DirEntry::removeDirAndContents(staging);
        DirEntry::renameFile(zipPath, zipPath + ".bad");
        return false;
    };

    DirEntry::removeDirAndContents(staging);   // a previous run that did not get to the end
    if (!ZipArchive::extract(zipPath, staging))
        return giveUp("could not unpack it");

    const string root = findThemeRoot(staging);
    if (root.empty())
        return giveUp("no theme.json or theme.ini in it");

    if (DirEntry::exists(dest)) {
        PLOG_INFO << "Replacing theme folder " << dest;
        DirEntry::removeDirAndContents(dest);
    }
    if (!DirEntry::renameFile(root, dest))
        return giveUp("could not move it into place");
    if (root != staging) DirEntry::removeDirAndContents(staging);

    DirEntry::removeFile(zipPath);
    PLOG_INFO << "Theme " << name << " installed";
    return true;
}

//*******************************
// ThemeInstaller::installZips
//*******************************
vector<string> ThemeInstaller::installZips(const string &themesDir) {
    vector<string> installed;
    for (const DirEntry &entry : DirEntry::diru_FilesOnly(themesDir)) {
        if (!isZipName(entry.name)) continue;
        if (installZip(themesDir + sep + entry.name, themesDir))
            installed.push_back(themeName(entry.name));
    }
    return installed;
}
