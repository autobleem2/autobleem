//
// TempDir: a directory that exists for the life of the object and is deleted with its contents after.
//
#pragma once

#include <ableem/engine/filesystem.h>
#include <cstdlib>
#include <string>
#ifdef _WIN32
#include <process.h>
#define AB_TEST_PID _getpid()
#else
#include <unistd.h>
#define AB_TEST_PID getpid()
#endif

//******************
// TempDir
//******************
// Every test that touches the filesystem gets its own tree, so tests cannot see each other's files and a
// failing test leaves nothing behind for the next run to trip over. Paths are built with ableem::sep, the
// same way the code under test builds them.
//
//     TempDir tmp("config");
//     tmp.makeSubDir("themes/default");
//     tmp.writeFile("config.ini", "Theme=aergb\n");
//
class TempDir {
public:
    explicit TempDir(const std::string &label) {
        // the counter keeps two TempDirs made in the same test from colliding; the pid keeps two test
        // executables run at once (ctest -j) apart - the same label and counter in both used to make one
        // delete the other's tree from under it
        static int counter = 0;
        const char *base = getenv("TMPDIR");
        if (base == nullptr)
            base = getenv("TEMP");
        if (base == nullptr)
            base = ".";
        path_ = ableem::DirEntry::removeSeparatorFromEndOfPath(base) + ableem::sep + "ab_test_" + label + "_" +
                std::to_string(AB_TEST_PID) + "_" + std::to_string(++counter);
        ableem::DirEntry::removeDirAndContents(path_); // a previous run that was killed mid-test
        ableem::DirEntry::createDir(path_);
    }

    ~TempDir() { ableem::DirEntry::removeDirAndContents(path_); }

    TempDir(const TempDir &) = delete;
    TempDir &operator=(const TempDir &) = delete;

    const std::string &path() const { return path_; }
    std::string at(const std::string &relative) const { return path_ + ableem::sep + relative; }

    // creates every level of 'relative', e.g. makeSubDir("themes/default")
    std::string makeSubDir(const std::string &relative) const;
    void writeFile(const std::string &relative, const std::string &contents) const;
    std::string readFile(const std::string &relative) const;

private:
    std::string path_;
};
