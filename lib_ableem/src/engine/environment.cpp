#include "ableem/engine/environment.h"
#include "ableem/engine/filesystem.h"
#include "ableem/engine/game_types.h"

#include <climits>
#include <unistd.h>

using namespace std;

namespace ableem {

namespace {
// file-local so nothing outside the setters can touch them
string usbRoot;
string gamesDir;
string regionalDbFile;
string internalDbFile;
string workingPath;
string appDir;
string kernelConfigDir;
string sonyDataPath;
string themesDir;
string coversDbDir;
string internalGamesDir = "/gaadata";
string retroarchDir;      // empty: usb:/RetroArch/bin, the console's layout
string retroarchCoreFile; // empty: pcsx_rearmed under the retroarch dir
string retroarchRomsDir;  // empty: usb:/RetroArch/roms
string retroarchBiosDir;  // empty: usb:/RetroArch/bios
string retroarchCoreExtension = ".so";
} // namespace

//*******************************
// Environment:: setters
//*******************************
void Environment::setUsbRoot(const string &path) {
    usbRoot = path;
}
void Environment::setGamesDir(const string &path) {
    gamesDir = path;
}
void Environment::setRegionalDbFile(const string &path) {
    regionalDbFile = path;
}
void Environment::setInternalDbFile(const string &path) {
    internalDbFile = path;
}
void Environment::setAppDir(const string &path) {
    appDir = path;
}

void Environment::setKernelConfigDir(const string &path) {
    kernelConfigDir = path;
}

void Environment::setWorkingPath(const string &path) {
    workingPath = path;
}
void Environment::setSonyDataPath(const string &path) {
    sonyDataPath = path;
}
void Environment::setThemesDir(const string &path) {
    themesDir = path;
}
void Environment::setCoversDbDir(const string &path) {
    coversDbDir = path;
}
void Environment::setInternalGamesDir(const string &path) {
    internalGamesDir = path;
}
void Environment::setRetroarchDir(const string &path) {
    retroarchDir = path;
}
void Environment::setRetroarchCoreFile(const string &path) {
    retroarchCoreFile = path;
}
void Environment::setRetroarchRomsDir(const string &path) {
    retroarchRomsDir = path;
}
void Environment::setRetroarchBiosDir(const string &path) {
    retroarchBiosDir = path;
}
void Environment::setRetroarchCoreExtension(const string &ext) {
    retroarchCoreExtension = ext.empty() ? ".so" : ext[0] == '.' ? ext : "." + ext;
}
const string &Environment::getRetroarchCoreExtension() {
    return retroarchCoreExtension;
}

//*******************************
// Environment:: getters
//*******************************
string Environment::getPathToUSBRoot() {
    return usbRoot;
}
string Environment::getPathToAutobleemDir() {
    return usbRoot + sep + "Autobleem";
}
string Environment::getPathToAppsDir() {
    return usbRoot + sep + "Apps";
}
string Environment::getPathToRCDir() {
    return getPathToAutobleemDir() + sep + "rc";
}
string Environment::getPathToGamesDir() {
    return gamesDir;
}
string Environment::getPathToMemCardsDir() {
    return gamesDir + sep + MEMCARDS_DIR_NAME;
}
string Environment::getPathToSaveStatesDir() {
    return gamesDir + sep + SAVESTATES_DIR_NAME;
}
string Environment::getPathToSystemDir() {
    return usbRoot + sep + "System";
}
string Environment::getPathToLogsDir() {
    return getPathToSystemDir() + sep + "Logs";
}
string Environment::getPathToRetroarchDir() {
    return retroarchDir.empty() ? usbRoot + sep + "RetroArch" + sep + "bin" : retroarchDir;
}
string Environment::getPathToRetroarchPlaylistsDir() {
    return getPathToRetroarchDir() + sep + "playlists";
}
string Environment::getPathToRetroarchRdbDir() {
    return getPathToRetroarchDir() + sep + "database" + sep + "rdb";
}
string Environment::getPathToRetroarchThumbnailsDir() {
    return getPathToRetroarchDir() + sep + "thumbnails";
}
string Environment::getPathToRetroarchScreenshotsDir() {
    return getPathToRetroarchDir() + sep + "screenshots";
}
string Environment::getPathToRetroarchStatesDir() {
    return getPathToRetroarchDir() + sep + "states";
}
string Environment::getPathToPlayStationRdbFile() {
    return getPathToRetroarchRdbDir() + sep + "Sony - PlayStation.rdb";
}
string Environment::getPathToRetroarchCoreFile() {
    return retroarchCoreFile.empty()
               ? getPathToRetroarchDir() + sep + "cores/pcsx_rearmed_libretro" + retroarchCoreExtension
               : retroarchCoreFile;
}
bool Environment::hasRetroBoot() {
    return DirEntry::exists(getPathToRetroarchDir() + sep + "retroboot");
}
string Environment::getPathToRetroarchRomsDir() {
    return retroarchRomsDir.empty() ? usbRoot + sep + "RetroArch" + sep + "roms" : retroarchRomsDir;
}
string Environment::getPathToRetroarchBiosDir() {
    return retroarchBiosDir.empty() ? usbRoot + sep + "RetroArch" + sep + "bios" : retroarchBiosDir;
}
string Environment::getPathToRegionalDBFile() {
    return regionalDbFile;
}
string Environment::getPathToInternalDBFile() {
    return internalDbFile;
}
string Environment::getPathToInternalGamesDir() {
    return internalGamesDir;
}

//*******************************
// Environment::getWorkingPath
// the resources dir. when the application never set one (the console runs autobleem-gui from its own dir)
// it is the current directory.
//*******************************
string Environment::getWorkingPath() {
    if (!workingPath.empty())
        return workingPath;
    char temp[PATH_MAX];
    return (getcwd(temp, sizeof(temp)) ? string(temp) : string(""));
}

//*******************************
// Environment::getAppDir
//*******************************
string Environment::getAppDir() {
    if (!appDir.empty())
        return appDir;
    char temp[PATH_MAX];
    return (getcwd(temp, sizeof(temp)) ? string(temp) : string(""));
}

string Environment::getPathToAppLangDir() {
    return getAppDir() + sep + "lang";
}

string Environment::getPathToKernelConfigDir() {
    return kernelConfigDir;
}

string Environment::getPathToGameControllerDb() {
    return getWorkingPath() + sep + "gamecontrollerdb.txt";
}

string Environment::getPathToMemcardTemplateDir() {
    return getWorkingPath() + sep + "memcard";
}
string Environment::getPathToLangDir() {
    return getWorkingPath() + sep + "lang";
}

//*******************************
// Environment::getSonyPath / getSonyFontPath
//*******************************
string Environment::getSonyPath() {
    return sonyDataPath;
}
string Environment::getSonyFontPath() {
    return getSonyPath() + sep + "font";
}

//*******************************
// Environment::getPathToThemesDir / getPathToCoversDBDir
//*******************************
string Environment::getPathToThemesDir() {
    return themesDir;
}
string Environment::getPathToCoversDBDir() {
    return coversDbDir;
}

} // namespace ableem
