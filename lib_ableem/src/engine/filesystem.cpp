#include "ableem/engine/filesystem.h"
#include "ableem/engine/environment.h"
#include "ableem/engine/strings.h"

#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <cerrno>
#include <dirent.h>
#include <libgen.h>
#include <fstream>
#include <iostream>
#include <algorithm>
#include "ableem/engine/log.h"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

using namespace std;

namespace ableem {

// 0.5MB
#define FILE_BUFFER_SIZE 524288

//*******************************
// DirEntry::sortDirEntryByName
//*******************************
bool DirEntry::sortDirEntryByName(const DirEntry &i, const DirEntry &j) {
    return lessCaseInsensitive(i.name, j.name);
}

//*******************************
// append separator helper function
//*******************************
// to use "operator +" below, "path + sep" will append the separator only if it's not already on the end of path
std::string operator+(const std::string &leftside, Sep) {
    string ret = leftside;
    ret += sep;

    return ret;
}

//*******************************
// append separator helper function
//*******************************
// to use "operator +" below, "path + sep" will append the separator only if it's not already on the end of path
void operator+=(std::string &leftside, Sep) {
    if (leftside.size() > 0) {
        char lastChar = leftside.back();
        if (lastChar != separator)
            leftside += separator; // add slash at end
    }
}

//*******************************
// DirEntry::isPBPFile
//*******************************
bool DirEntry::isPBPFile(std::string path) {
    if (path.length() < 4)
        return false;
    string last_four = path.substr(path.length() - 4);
    lcase(last_four);
    return last_four == ".pbp";
}

//*******************************
// DirEntry::generateM3UForDirectory
//*******************************
// <path>/<basename>.m3u listing every disc image in the folder (cue, pbp, chd) when there is more than
// one - what RetroArch opens for a multi-disc game. basename is the first disc's name however the
// scanner spells it (a cue's base, or a PBP's / CHD's whole file name), so any image extension is
// stripped first; earlier versions kept the *last four* characters of a PBP name instead of dropping
// them, and knew nothing of CHD. Stale .m3u files go first, so a rename does not leave two behind.
void DirEntry::generateM3UForDirectory(std::string path, std::string basename) {
    string ext = getFileExtension(basename);
    if (Strings::compareCaseInsensitive(ext, "pbp") || Strings::compareCaseInsensitive(ext, "chd") ||
        Strings::compareCaseInsensitive(ext, "cue") || Strings::compareCaseInsensitive(ext, "bin") ||
        Strings::compareCaseInsensitive(ext, "img")) {
        basename = getFileNameWithoutExtension(basename);
    }
    vector<string> files;
    DirEntries filesInPath = DirEntry::diru_FilesOnly(path);
    for (const DirEntry &entry : filesInPath) {
        ext = DirEntry::getFileExtension(entry.name);
        if (Strings::compareCaseInsensitive(ext, "pbp") || Strings::compareCaseInsensitive(ext, "cue") ||
            Strings::compareCaseInsensitive(ext, "chd"))
            files.push_back(entry.name);
    }
    if (files.size() <= 1)
        return;

    sort(files.begin(), files.end());
    for (const DirEntry &entry : filesInPath) {
        if (Strings::compareCaseInsensitive(DirEntry::getFileExtension(entry.name), "m3u"))
            removeFile(fixPath(path) + sep + entry.name);
    }
    string m3uName = DirEntry::fixPath(path) + sep + basename + ".m3u";
    ofstream os(m3uName);
    if (!checkWritable(os, m3uName))
        return;
    for (const string &file : files) {
        os << file << endl;
    }
    os.close();
}

//*******************************
// DirEntry::fixPath
// removes leading and trailing spaces and removes any '/' from the end
//*******************************
string DirEntry::fixPath(string path) {
    trim(path);
    if (path.size() > 0 && path.back() == separator)
        path.pop_back();

    return path;
}

//*******************************
// DirEntry::removeSeparatorFromEndOfPath
//*******************************
// return the path without a separator at the end
string DirEntry::removeSeparatorFromEndOfPath(const string &path) {
    string ret = path;
    if (ret.length() > 0) {
        char &lastChar = ret.back();
        if (lastChar == separator)
            ret.pop_back(); // remove slash at end
    }

    return ret;
}

//*******************************
// DirEntry::removeGamesPathFromFrontOfPath
//*******************************
string DirEntry::removeGamesPathFromFrontOfPath(const std::string &path) {
    string gamesDir = Environment::getPathToGamesDir() + sep;
    int len = gamesDir.size();
    if (path.compare(0, len, gamesDir) == 0)
        return string(path).erase(0, len);
    else
        return path;
}

//*******************************
// DirEntry::getFileNameFromPath
//*******************************
string DirEntry::getFileNameFromPath(const string &path) {
    string copy = path; // basename() may modify its argument
    return basename(&copy[0]);
}

//*******************************
// DirEntry::getDirNameFromPath
//*******************************
string DirEntry::getDirNameFromPath(const string &path) {
    string copy = path; // dirname() may modify its argument
    return dirname(&copy[0]);
}

//*******************************
// DirEntry::isDirectory
//*******************************
bool DirEntry::isDirectory(const string &path) {
    struct stat path_stat;
    if (stat(path.c_str(), &path_stat) != 0)
        return false; // does not exist (st_mode would be uninitialized)
    return S_ISDIR(path_stat.st_mode);
}

//*******************************
// DirEntry::fileSize
//*******************************
long long DirEntry::fileSize(const string &path) {
    struct stat path_stat;
    if (stat(path.c_str(), &path_stat) != 0)
        return -1;
    if (S_ISDIR(path_stat.st_mode))
        return -1;
    return static_cast<long long>(path_stat.st_size);
}

//*******************************
// DirEntry::replaceTheseCharsWithThisChar
// replaces all the chars of a selection of chars with a single replacement char
// returns a string with those chars replaced
//*******************************
string DirEntry::replaceTheseCharsWithThisChar(string str, const string &charsToReplace, char replacementChar) {
    auto isBadChar = [&](char c) {
        return charsToReplace.find(c) != string::npos; // return if the char is a bad char
    };

    replace_if(begin(str), end(str), isBadChar, replacementChar);

    return str;
}

//*******************************
// DirEntry::fixCommaInDirName
// returns true if dir entry modified
//*******************************
bool DirEntry::fixCommaInDirOrFileName(const std::string &path, DirEntry *entry) {
    // fix for comma in dirname
    if (entry->name.find(",") != string::npos) {
        string newName = entry->name;
        Strings::replaceAll(newName, ",", "-");
        DirEntry::renameFile(path + sep + entry->name, path + sep + newName);
        entry->name = newName;
        return true;
    } else
        return false;
}

//*******************************
// DirEntry::dir
//*******************************
DirEntries DirEntry::dir(string path) {
    path = fixPath(removeSeparatorFromEndOfPath(path));
    DirEntries result;
    DIR *dir = opendir(path.c_str());
    if (dir != nullptr) {
        struct dirent *entry = readdir(dir);
        while (entry != nullptr) {
            // note: d_type is not a bool and is not available on every platform. use stat like diru() does.
            DirEntry obj(entry->d_name, isDirectory(path + sep + entry->d_name));
            result.push_back(obj);
            entry = readdir(dir);
        }

        closedir(dir);
    }
    sort(result.begin(), result.end(), DirEntry::sortDirEntryByName);
    return result;
}

//*******************************
// DirEntry::diru
//*******************************
DirEntries DirEntry::diru(string path) {
    path = fixPath(removeSeparatorFromEndOfPath(path));
    DirEntries result;
    DIR *dir = opendir(path.c_str());
    if (dir != nullptr) {
        struct dirent *entry = readdir(dir);
        while (entry != nullptr) {
            DirEntry obj(entry->d_name, isDirectory(path + sep + entry->d_name));
            if (entry->d_name[0] != '.') {
                result.push_back(obj);
            }
            entry = readdir(dir);
        }

        closedir(dir);
    }
    sort(result.begin(), result.end(), DirEntry::sortDirEntryByName);
    return result;
}

//*******************************
// DirEntry::filesAreIdentical
//*******************************
bool DirEntry::filesAreIdentical(const string &a, const string &b) {
    if (!exists(a) || !exists(b) || fileSize(a) != fileSize(b))
        return false;
    ifstream fa(a, ios::binary), fb(b, ios::binary);
    if (!fa.is_open() || !fb.is_open())
        return false;
    char bufA[4096], bufB[4096];
    while (fa && fb) {
        fa.read(bufA, sizeof(bufA));
        fb.read(bufB, sizeof(bufB));
        if (fa.gcount() != fb.gcount() || memcmp(bufA, bufB, static_cast<size_t>(fa.gcount())) != 0)
            return false;
    }
    return true;
}

//*******************************
// DirEntry::listNames
//*******************************
vector<string> DirEntry::listNames(const string &path) {
    vector<string> names;
    DIR *dir = opendir(fixPath(removeSeparatorFromEndOfPath(path)).c_str());
    if (dir != nullptr) {
        for (struct dirent *entry = readdir(dir); entry != nullptr; entry = readdir(dir)) {
            if (entry->d_name[0] == '.')
                continue;
#ifdef DT_DIR
            if (entry->d_type == DT_DIR)
                continue;
#endif
            names.push_back(entry->d_name);
        }
        closedir(dir);
    }
    return names;
}

//*******************************
// DirEntry::diru_DirsOnly
//*******************************
DirEntries DirEntry::diru_DirsOnly(string path) {
    auto temp = diru(path); // get all dirs and files
    DirEntries ret;
    copy_if(begin(temp), end(temp), back_inserter(ret),
            [](const DirEntry &dir) { return dir.isDir; }); // copy only dirs

    return ret; // return only the dirs
}

//*******************************
// DirEntry::diru_FilesOnly
//*******************************
DirEntries DirEntry::diru_FilesOnly(string path) {
    auto temp = diru(path); // get all dirs and files
    DirEntries ret;
    copy_if(begin(temp), end(temp), back_inserter(ret),
            [](const DirEntry &dir) { return !dir.isDir; }); // copy only files

    return ret; // return only the files
}

//*******************************
// DirEntry::exists
//*******************************
bool DirEntry::exists(const string &_name) {
    auto name = _name;
    fixPath(name);
    struct stat buffer;
    return (stat(name.c_str(), &buffer) == 0);
}

//*******************************
// DirEntry::createDir
//*******************************
bool DirEntry::createDir(const string &_name) {
    auto name = fixPath(_name);
#ifdef _WIN32
    const int dir_err = mkdir(name.c_str());
#else
    const int dir_err = mkdir(name.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif
    return (-1 != dir_err);
}

//*******************************
// DirEntry::rmDir
//*******************************
int DirEntry::rmDir(string path) {
    fixPath(path);
    DIR *d = opendir(path.c_str());
    int r = -1;

    if (d) {
        struct dirent *p;

        r = 0;

        while (!r && (p = readdir(d))) {
            int r2 = -1;

            /* Skip the names "." and ".." as we don't want to recurse on them. */
            if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, "..")) {
                continue;
            }

            string entryPath = path + "/" + p->d_name;
            struct stat statbuf;
            if (!stat(entryPath.c_str(), &statbuf)) {
                if (S_ISDIR(statbuf.st_mode)) {
                    r2 = rmDir(entryPath);
                } else {
                    r2 = unlink(entryPath.c_str());
                }
            }

            r = r2;
        }

        closedir(d);
    }

    if (!r) {
        r = rmdir(path.c_str());
    }

    return r;
}

//*******************************
// DirEntry::removeDirAndContents
//*******************************
// rmDir() reads the directory itself, so dot-files (a macOS zip's "._name" entries, say) go too - diru()
// skips them, which used to leave the directory behind.
bool DirEntry::removeDirAndContents(const std::string path) {
    return rmDir(path) == 0;
}

//*******************************
// DirEntry::removeFile
//*******************************
bool DirEntry::removeFile(const string &path) {
    return remove(path.c_str()) == 0;
}

//*******************************
// DirEntry::renameFile
//*******************************
bool DirEntry::renameFile(const std::string &pathFrom, const std::string &pathTo) {
    return rename(pathFrom.c_str(), pathTo.c_str()) == 0;
}

//*******************************
// DirEntry::replaceFile
//*******************************
bool DirEntry::replaceFile(const std::string &pathFrom, const std::string &pathTo) {
#ifdef _WIN32
    return MoveFileExA(pathFrom.c_str(), pathTo.c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
#else
    return rename(pathFrom.c_str(), pathTo.c_str()) == 0;
#endif
}

//*******************************
// DirEntry::copyFile
//*******************************
bool DirEntry::copyFile(const std::string &pathFrom, const std::string &pathTo) {
    return DirEntry::copy(pathFrom, pathTo) == 0;
}

//*******************************
// DirEntry::checkWritable
//*******************************
bool DirEntry::checkWritable(const ofstream &os, const string &path) {
    if (os.is_open())
        return true;
    PLOG_WARNING << "ERROR: cannot write file: " << path << " (" << strerror(errno) << ")";
    return false;
}

//*******************************
// DirEntry::copy
//*******************************
bool DirEntry::copy(const string &source, const string &dest) {
    ifstream infile(source, ios::binary);
    ofstream outfile(dest, ios::binary);

    if (!infile.good())
        return false;
    if (!outfile.good())
        return false;

    vector<char> buffer(FILE_BUFFER_SIZE);
    while (true) {
        streamsize read = infile.readsome(buffer.data(), buffer.size());
        if (read == 0)
            break;
        outfile.write(buffer.data(), read);
    }
    outfile.flush();
    return outfile.good();
}

//*******************************
// DirEntry::findFirstFile
//*******************************
string DirEntry::findFirstFile(string ext, string path) {
    fixPath(path);
    DirEntries entries = diru(path);
    for (DirEntry entry : entries) {
        if (matchExtension(entry.name, ext)) {
            return entry.name;
        }
    }
    return "";
}

//*******************************
// DirEntry::removeDotFromExtension
//*******************************
// if it begins with a ".", remove it
string DirEntry::removeDotFromExtension(const std::string &ext) {
    if (ext.c_str()[0] == '.')
        return ext.c_str() + 1;
    else
        return ext;
}

//*******************************
// DirEntry::addDotToExtension
//*******************************
// if it doesn't start with a ".", add it to the front
string DirEntry::addDotToExtension(const std::string &ext) {
    if (ext.c_str()[0] != '.')
        return string(".") + ext;
    else
        return ext;
}

//*******************************
// DirEntry::matchExtension
//*******************************
bool DirEntry::matchExtension(string path, string ext) {
    fixPath(path);
    if (path.length() >= 4) {
        string fileExt = path.substr(path.length() - 4, path.length()); // file extension includes the "."
        if (fileExt[0] != '.') {
            return false;
        } else {
            auto temp = addDotToExtension(ext);
            return lcase(fileExt) == lcase(temp);
        }
    } else {
        return false;
    };
}

//*******************************
// DirEntry::getFileExtension
//*******************************
// Return the extension of a filename without the "."
string DirEntry::getFileExtension(const string &fileName) {
    size_t i = fileName.rfind('.', fileName.length());
    if (i != string::npos) {
        return (fileName.substr(i + 1, fileName.length() - i));
    }
    return "";
}

//*******************************
// DirEntry::getFileNameWithoutExtension
//*******************************
// Return the name of a file without extension
string DirEntry::getFileNameWithoutExtension(const string &filename) {
    size_t indexBeforeDot = filename.find_last_of(".");
    return filename.substr(0, indexBeforeDot);
}

//*******************************
// DirEntry::cueToBinList
//*******************************
// Return the bin list declared in a cue file
vector<string> DirEntry::cueToBinList(string cueFile) {
    vector<string> binList;
    string line;

    ifstream is(cueFile);
    if (!is.is_open()) {
        PLOG_WARNING << "Error opening cue file: " << cueFile;
        return binList;
    }

    // Reading line by line
    while (getline(is, line)) {
        line = trim(line);
        if (line.substr(0, 4) == "FILE") {
            binList.push_back(Strings::getStringWithinChar(line, '"'));
        }
    }

    return binList;
}

//*******************************
// DirEntry::getFilesWithExtension
//*******************************
DirEntries DirEntry::getFilesWithExtension(const string &path, const DirEntries &entries,
                                           const vector<string> &extensions) {
    DirEntries fileList;
    string fileExt;
    for (const auto &entry : entries) {
        if (isDirectory(path + sep + entry.name))
            continue;
        // make it case insensitive compare (find .bin and .BIN)
        fileExt = toLowerCopy(getFileExtension(entry.name));
        if (find(extensions.begin(), extensions.end(), fileExt) != extensions.end()) {
            fileList.push_back(entry);
        }
    }
    return fileList;
}

//*******************************
// DirEntry::print
//*******************************
void DirEntry::print() const {
    PLOG_INFO << (isDir ? "Dir: " : "File: ") << name;
}

//*******************************
// DirEntries::print(const DirEntries &entries)
//*******************************
void DirEntry::print(const DirEntries &entries) {
    for (auto &entry : entries)
        entry.print();
}

//*******************************
// DirEntry::isAGameFile
//*******************************
bool DirEntry::isAGameFile(const std::string &filename) {
    if (matchExtension(filename, EXT_BIN))
        return true;
    if (matchExtension(filename, EXT_PBP))
        return true;
    if (matchExtension(filename, EXT_IMG))
        return true;
    if (matchExtension(filename, EXT_CHD))
        return true;

    return false;
}

//*******************************
// DirEntry::getGameFileImageType
//*******************************
ImageType DirEntry::getGameFileImageType(const std::string &filename) {
    if (matchExtension(filename, EXT_BIN))
        return IMAGE_BIN;
    if (matchExtension(filename, EXT_PBP))
        return IMAGE_PBP;
    if (matchExtension(filename, EXT_IMG))
        return IMAGE_IMG;
    if (matchExtension(filename, EXT_CHD))
        return IMAGE_CHD;

    return IMAGE_NO_GAME_FOUND;
}

//*******************************
// DirEntry::imageTypeUsesACueFile
//*******************************
bool DirEntry::imageTypeUsesACueFile(ImageType imageType) {
    if (imageType == IMAGE_BIN || imageType == IMAGE_IMG)
        return true;
    else
        return false;
}

//*******************************
// DirEntry::thereIsAGameFile
//*******************************
bool DirEntry::thereIsAGameFile(const DirEntries &entries) {
    return any_of(begin(entries), end(entries),
                  [](const DirEntry &entry) { return !entry.isDir && isAGameFile(entry.name); });
}

//*******************************
// DirEntry::thereIsASubDir
//*******************************
bool DirEntry::thereIsASubDir(const DirEntries &entries) {
    return any_of(begin(entries), end(entries), [](const DirEntry &entry) { return entry.isDir; });
}

//*******************************
// DirEntry::getGameFile
// Note: you must know that the game file exists in the directory before calling this function
//*******************************
tuple<ImageType, string> DirEntry::getGameFile(const DirEntries &entries) {
    auto iter = find_if(begin(entries), end(entries), [](const DirEntry &entry) { return isAGameFile(entry.name); });
    if (iter != end(entries))
        return make_tuple(getGameFileImageType(iter->name), iter->name);
    else
        return make_tuple(IMAGE_NO_GAME_FOUND, "");
}

} // namespace ableem
