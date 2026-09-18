// lib_ableem - engine: directory listing, file copy/rename/remove, path helpers and the PS1 game-file
// detection built on them. The only place in the library (and in the app) that uses dirent/stat.
#pragma once

#include <fstream>
#include <string>
#include <tuple>
#include <vector>

#include "game_types.h"

namespace ableem {

//*******************************
// separator
//*******************************
// '/' on every platform: the code base hard-codes "/" in places and Windows accepts forward slashes,
// so mixing separators on a Windows dev build would only break string comparisons.
static const char separator = '/';

//*******************************
// append separator helper
//*******************************
struct Sep { };
const Sep sep;
// "path + sep" appends the separator only if it's not already on the end of path
std::string operator + (const std::string &leftside, Sep);
// "path += sep;"
void operator += (std::string &leftside, Sep);

//******************
// DirEntry
//******************
using DirEntries = std::vector<class DirEntry>;

class DirEntry {
public:
    std::string name;
    bool isDir;

    DirEntry(std::string _name, bool dir) : name(_name), isDir(dir) { };
    static bool sortDirEntryByName(const DirEntry &i, const DirEntry &j);

    static std::string fixPath(std::string path);   // removes leading and trailing spaces and removes any trailing '/'
    static std::string removeSeparatorFromEndOfPath(const std::string &path);

    static std::string removeGamesPathFromFrontOfPath(const std::string &path);   // Environment::getPathToGamesDir()

    static std::string replaceTheseCharsWithThisChar(std::string str, const std::string &charsToReplace, char replacementChar);
    static bool fixCommaInDirOrFileName(const std::string &path, DirEntry *entry);   // renames on disk, returns true if it did

    static DirEntries dir(std::string path);   // returns directory contents including . and ..
    static DirEntries diru(std::string path);  // returns directory contents except . and ..
    static DirEntries diru_DirsOnly(std::string path);  // diru but only returns directories
    static DirEntries diru_FilesOnly(std::string path);  // diru but only returns files
    // Every name in the directory except dot files and what the OS already knows to be a subdirectory -
    // no stat() per entry, which is what makes diru() take tens of seconds on a USB stick for the ~8000
    // files of a thumbnails folder. On exFAT/FAT the kind is often unknown, so a subdirectory may be
    // listed; callers filter by extension anyway. Unsorted.
    static std::vector<std::string> listNames(const std::string &path);

    static bool copy(const std::string &source, const std::string &dest);
    // logs when an output file could not be opened. returns false in that case so the caller can bail out.
    static bool checkWritable(const std::ofstream &os, const std::string &path);
    static bool exists(const std::string &name);        // file or dir
    // st_size of a file; -1 if it does not exist or is a directory. Deliberately size only, not mtime: the
    // PSC has no battery-backed clock, so a file's stored modification time cannot be trusted to stay put
    // across a reboot (see Clock's comment) - GamesFingerprint uses this instead to notice a changed game.
    static long long fileSize(const std::string &path);
    static bool filesAreIdentical(const std::string &a, const std::string &b);   // same size and bytes; false if either is missing
    static bool createDir(const std::string &name);
    static int rmDir(std::string path);                 // recursive; 0 on success
    static bool removeDirAndContents(const std::string path);

    static bool removeFile(const std::string &path);
    static bool renameFile(const std::string &pathFrom, const std::string &pathTo);
    static bool copyFile(const std::string &pathFrom, const std::string &pathTo);

    static std::string removeDotFromExtension(const std::string &ext);
    static std::string addDotToExtension(const std::string &ext);
    static bool matchExtension(std::string path, std::string ext);  // case insensitive
    static bool isDirectory(const std::string &path);
    static DirEntries getFilesWithExtension(const std::string &path, const DirEntries &entries,
                                            const std::vector<std::string> &extensions);    // pass file extensions in lower case, no dot
    static std::string getFileNameFromPath(const std::string &path);
    static std::string getDirNameFromPath(const std::string &path);
    static std::string getFileExtension(const std::string &fileName);     // without the "."
    static std::string getFileNameWithoutExtension(const std::string &filename);
    static std::string findFirstFile(std::string ext, std::string path);
    static std::vector<std::string> cueToBinList(std::string cueFile);
    static bool isPBPFile(std::string path);
    static void generateM3UForDirectory(std::string path, std::string basename);   // only when there is more than one disc

    void print() const;
    static void print(const DirEntries &entries);

    // PS1 game files
    static bool isAGameFile(const std::string &filename);
    static ImageType getGameFileImageType(const std::string &filename);
    static bool imageTypeUsesACueFile(ImageType imageType);

    static bool thereIsAGameFile(const DirEntries &entries);
    static bool thereIsAGameFile(const std::string &dirpath) { return thereIsAGameFile(diru(dirpath)); }

    static bool thereIsASubDir(const DirEntries &entries);
    static bool thereIsASubDir(const std::string &dirpath) { return thereIsASubDir(diru(dirpath)); }

    // Note: you must know that the game file exists in the directory before calling these functions
    static std::tuple<ImageType, std::string> getGameFile(const DirEntries &entries);
    static std::tuple<ImageType, std::string> getGameFile(const std::string &dirpath) { return getGameFile(diru(dirpath)); }
};

} // namespace ableem
