#include "ableem/engine/zip_archive.h"
#include "ableem/engine/filesystem.h"

#include <iostream>
#include <miniz.h>
#include "ableem/engine/log.h"
#include <ableem/engine/log.h>

using namespace std;

namespace ableem {

namespace {

// creates every level of `dir`, like mkdir -p; true if it exists afterwards
bool createDirs(const string &dir) {
    if (dir.empty() || DirEntry::isDirectory(dir))
        return true;
    size_t slash = dir.find_last_of('/');
    if (slash != string::npos && slash > 0 && !createDirs(dir.substr(0, slash)))
        return false;
    DirEntry::createDir(dir);
    return DirEntry::isDirectory(dir);
}

// a reader over one file, closed on the way out
struct Reader {
    mz_zip_archive zip;
    bool open = false;

    explicit Reader(const string &path) : zip() {
        mz_zip_zero_struct(&zip);
        open = mz_zip_reader_init_file(&zip, path.c_str(), 0) != 0;
        if (!open) {
            PLOG_WARNING << "Not a zip file: " << path << " (" << mz_zip_get_error_string(mz_zip_get_last_error(&zip))
                         << ")";
        }
    }
    ~Reader() {
        if (open)
            mz_zip_reader_end(&zip);
    }

    unsigned count() { return mz_zip_reader_get_num_files(&zip); }

    bool name(unsigned i, string &out, bool &isDir) {
        ZipEntry entry;
        if (!stat(i, entry))
            return false;
        out = entry.name;
        isDir = entry.isDir;
        return true;
    }

    bool stat(unsigned i, ZipEntry &entry) {
        mz_zip_archive_file_stat st;
        if (!mz_zip_reader_file_stat(&zip, i, &st))
            return false;
        entry.name = st.m_filename;
        entry.isDir = mz_zip_reader_is_file_a_directory(&zip, i) != 0;
        if (entry.isDir && (entry.name.empty() || entry.name.back() != '/'))
            entry.name += '/';
        entry.crc = st.m_crc32;
        entry.size = st.m_uncomp_size;
        return true;
    }
};

} // namespace

//*******************************
// ZipArchive::isSafeName
//*******************************
bool ZipArchive::isSafeName(const string &name) {
    if (name.empty())
        return false;
    if (name[0] == '/' || name.find('\\') != string::npos || name.find(':') != string::npos)
        return false;
    // no "." or ".." segment anywhere
    size_t start = 0;
    while (start <= name.size()) {
        size_t end = name.find('/', start);
        if (end == string::npos)
            end = name.size();
        string segment = name.substr(start, end - start);
        if (segment == ".." || segment == ".")
            return false;
        start = end + 1;
    }
    return true;
}

//*******************************
// ZipArchive::list
//*******************************
bool ZipArchive::list(const string &zipPath, vector<string> &names) {
    names.clear();
    Reader reader(zipPath);
    if (!reader.open)
        return false;
    for (unsigned i = 0; i < reader.count(); i++) {
        string name;
        bool isDir;
        if (!reader.name(i, name, isDir))
            continue;
        names.push_back(name);
    }
    return true;
}

//*******************************
// ZipArchive::listEntries
//*******************************
bool ZipArchive::listEntries(const string &zipPath, vector<ZipEntry> &entries) {
    entries.clear();
    Reader reader(zipPath);
    if (!reader.open)
        return false;
    for (unsigned i = 0; i < reader.count(); i++) {
        ZipEntry entry;
        if (reader.stat(i, entry))
            entries.push_back(entry);
    }
    return true;
}

//*******************************
// ZipArchive::extract
//*******************************
bool ZipArchive::extract(const string &zipPath, const string &destDir) {
    Reader reader(zipPath);
    if (!reader.open)
        return false;

    // check every name before writing anything
    for (unsigned i = 0; i < reader.count(); i++) {
        string name;
        bool isDir;
        if (!reader.name(i, name, isDir)) {
            PLOG_INFO << "Zip entry " << i << " of " << zipPath << " is unreadable";
            return false;
        }
        if (!isSafeName(name)) {
            PLOG_INFO << "Refusing zip entry '" << name << "' in " << zipPath;
            return false;
        }
    }

    if (!createDirs(destDir)) {
        PLOG_WARNING << "Could not create " << destDir;
        return false;
    }
    const string dest = destDir + sep;
    for (unsigned i = 0; i < reader.count(); i++) {
        string name;
        bool isDir;
        reader.name(i, name, isDir);
        const string target = dest + name;
        if (isDir) {
            if (!createDirs(DirEntry::removeSeparatorFromEndOfPath(target)))
                return false;
            continue;
        }
        size_t slash = target.find_last_of('/');
        if (slash != string::npos && !createDirs(target.substr(0, slash))) {
            PLOG_WARNING << "Could not create the directory for " << target;
            return false;
        }
        if (!mz_zip_reader_extract_to_file(&reader.zip, i, target.c_str(), 0)) {
            PLOG_ERROR << "Could not extract " << name << " from " << zipPath << " ("
                       << mz_zip_get_error_string(mz_zip_get_last_error(&reader.zip)) << ")";
            return false;
        }
    }
    return true;
}

} // namespace ableem
