// lib_ableem - engine: where a game's cover and screenshot are on disk when RetroArch's libretro-thumbnails
// packs are around. Filesystem only - no game or playlist knowledge - which is what keeps it testable.
//
//   <retroarch>/thumbnails/<db name>/Named_Boxarts/<escaped name>.{png,jpg,jpeg}
//   <retroarch>/thumbnails/<db name>/Named_Titles/...       (the title screen)
//   <retroarch>/thumbnails/<db name>/Named_Snaps/...        (an in-game shot)
//   <retroarch>/screenshots/<rom base>(-YYMMDD-HHMMSS)?.{png,jpg,jpeg}   the user's own
//   <retroarch>/states/<rom base>.state.auto.png                          the auto save state's picture
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace ableem {

//******************
// ThumbnailLookup
//******************
// Ported from AutoBleem-NG (autobleem/code/launcher/thumbnail_lookup.*). One instance per user: the
// directory listings it caches (Named_Boxarts alone has ~8000 files on a USB stick) belong to the
// instance, not to a global, because the scan worker and the launcher each want their own.
class ThumbnailLookup {
public:
    static const char PlayStationDbName[]; // "Sony - PlayStation" - the PS1 pack's folder and .rdb stem

    // the roots come from Environment (getPathToRetroarchThumbnailsDir/ScreenshotsDir/StatesDir)
    ThumbnailLookup();

    // what libretro-thumbnails does to a name before it becomes a file name: &*/:`<>?\| -> _
    static std::string escapeName(const std::string &name);

    // The file among `names` (a folder's listing, or libretro's server index) for a game: recordName (the
    // rdb's canonical name, when known) is tried before title, because that is what the file is named
    // after; each has its trailing " (...)" tags peeled off one at a time ("Persona.jpg" for "Persona
    // (USA)"). Last, the fuzzy match: any "<bare name> (...)" file, the one sharing most of the original
    // tags, then USA > Europe > World, then alphabetical. Case does not matter anywhere - libretro's
    // thumbnails follow newer No-Intro spellings than the rdb ("Sonic The Hedgehog" vs "Sonic the
    // Hedgehog"). Returns the name as it is in the list, "" for no match.
    static std::string pickName(const std::vector<std::string> &names, const std::string &title,
                                const std::string &recordName = "");

    // <thumbnails>/<dbName>/<thumbnailDir>/ + pickName() over that folder, or ""
    std::string findThumbnail(const std::string &dbName, const std::string &title, const std::string &thumbnailDir,
                              const std::string &recordName = "");
    // Named_Boxarts, else Named_Titles, else Named_Snaps
    std::string findBoxArt(const std::string &dbName, const std::string &title, const std::string &recordName = "");
    // the newest of the user's screenshots for this rom, else the auto save state's picture, else ""
    std::string findLocalScreenshot(const std::string &romPath);
    // findLocalScreenshot, else Named_Snaps
    std::string findSnap(const std::string &dbName, const std::string &title, const std::string &romPath = "",
                         const std::string &recordName = "");

    // forget the directory listings - after the thumbnails tree changed under us
    void clearCache() { dirCache_.clear(); }

private:
    const std::vector<std::string> &listDir(const std::string &dir);

    std::string thumbnailsDir_, screenshotsDir_, statesDir_;
    std::unordered_map<std::string, std::vector<std::string>> dirCache_;
};

} // namespace ableem
