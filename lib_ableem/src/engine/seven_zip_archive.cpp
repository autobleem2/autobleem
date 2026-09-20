#include "ableem/engine/seven_zip_archive.h"
#include "ableem/engine/filesystem.h"
#include "ableem/engine/log.h"
#include "ableem/engine/tar_archive.h"

#include <fstream>

#ifndef ABLEEM_NO_CHD
#include <7z.h>
#include <7zAlloc.h>
#include <7zBuf.h>
#include <7zCrc.h>
#include <7zFile.h>
#endif

using namespace std;

namespace ableem {

#ifdef ABLEEM_NO_CHD

bool SevenZipArchive::list(const string &, vector<SevenZipEntry> &) {
    PLOG_WARNING << "7z archives are not supported in this build";
    return false;
}
bool SevenZipArchive::extract(const string &, const string &, string &error, const Filter &, const Progress &,
                              const string &) {
    error = "7z archives are not supported in this build";
    return false;
}

#else

namespace {

const ISzAlloc allocMain = {SzAlloc, SzFree};
const ISzAlloc allocTemp = {SzAllocTemp, SzFreeTemp};
const size_t InputBufferSize = 1 << 18;

// a UTF-16 entry name as UTF-8 with forward slashes (7z stores names with '/' already)
string utf8Name(const UInt16 *name, size_t len) {
    string out;
    for (size_t i = 0; i < len && name[i] != 0; i++) {
        uint32_t c = name[i];
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < len && name[i + 1] >= 0xDC00 && name[i + 1] < 0xE000) {
            c = 0x10000 + ((c - 0xD800) << 10) + (name[i + 1] - 0xDC00);
            i++;
        }
        if (c < 0x80) {
            out += static_cast<char>(c);
        } else if (c < 0x800) {
            out += static_cast<char>(0xC0 | (c >> 6));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else if (c < 0x10000) {
            out += static_cast<char>(0xE0 | (c >> 12));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (c >> 18));
            out += static_cast<char>(0x80 | ((c >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((c >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (c & 0x3F));
        }
    }
    for (char &ch : out)
        if (ch == '\\')
            ch = '/';
    return out;
}

const char *resultText(SRes res) {
    switch (res) {
    case SZ_ERROR_DATA:
        return "the archive is damaged";
    case SZ_ERROR_MEM:
        return "out of memory";
    case SZ_ERROR_CRC:
        return "a CRC does not match";
    case SZ_ERROR_UNSUPPORTED:
        return "an unsupported compression method";
    case SZ_ERROR_ARCHIVE:
    case SZ_ERROR_NO_ARCHIVE:
        return "not a 7z archive";
    case SZ_ERROR_INPUT_EOF:
        return "the archive is truncated";
    default:
        return "the 7z decoder failed";
    }
}

//******************
// Reader
//******************
// the archive open with the SDK's look-ahead stream, closed with the object
struct Reader {
    CFileInStream fileStream;
    CLookToRead2 lookStream;
    CSzArEx db;
    bool open = false;
    string why;

    explicit Reader(const string &path) {
        static bool crcReady = false;
        if (!crcReady) {
            CrcGenerateTable();
            crcReady = true;
        }
        lookStream.buf = nullptr;
        SzArEx_Init(&db);
        if (InFile_Open(&fileStream.file, path.c_str()) != 0) {
            why = "cannot open " + path;
            return;
        }
        FileInStream_CreateVTable(&fileStream);
        fileStream.wres = 0;
        LookToRead2_CreateVTable(&lookStream, False);
        lookStream.buf = static_cast<Byte *>(ISzAlloc_Alloc(&allocMain, InputBufferSize));
        if (!lookStream.buf) {
            why = "out of memory";
            File_Close(&fileStream.file);
            return;
        }
        lookStream.bufSize = InputBufferSize;
        lookStream.realStream = &fileStream.vt;
        LookToRead2_INIT(&lookStream) SRes res = SzArEx_Open(&db, &lookStream.vt, &allocMain, &allocTemp);
        if (res != SZ_OK) {
            why = string(resultText(res)) + ": " + path;
            return;
        }
        open = true;
    }
    ~Reader() {
        SzArEx_Free(&db, &allocMain);
        if (lookStream.buf)
            ISzAlloc_Free(&allocMain, lookStream.buf);
        File_Close(&fileStream.file);
    }
    Reader(const Reader &) = delete;
    Reader &operator=(const Reader &) = delete;

    SevenZipEntry entry(UInt32 i) const {
        SevenZipEntry e;
        size_t len = SzArEx_GetFileNameUtf16(&db, i, nullptr);
        vector<UInt16> name(len + 1);
        SzArEx_GetFileNameUtf16(&db, i, name.data());
        e.name = utf8Name(name.data(), len);
        e.isDir = SzArEx_IsDir(&db, i) != 0;
        e.size = SzArEx_GetFileSize(&db, i);
        if (e.isDir && (e.name.empty() || e.name.back() != '/'))
            e.name += '/';
        return e;
    }
};

} // namespace

//*******************************
// SevenZipArchive::list
//*******************************
bool SevenZipArchive::list(const string &archivePath, vector<SevenZipEntry> &entries) {
    entries.clear();
    Reader reader(archivePath);
    if (!reader.open) {
        PLOG_INFO << reader.why;
        return false;
    }
    for (UInt32 i = 0; i < reader.db.NumFiles; i++)
        entries.push_back(reader.entry(i));
    return true;
}

//*******************************
// SevenZipArchive::extract
//*******************************
bool SevenZipArchive::extract(const string &archivePath, const string &destDir, string &error, const Filter &filter,
                              const Progress &progress, const string &prefix) {
    Reader reader(archivePath);
    if (!reader.open) {
        error = reader.why;
        return false;
    }
    // every name checked before anything is written; the total for the progress
    vector<SevenZipEntry> entries;
    uint64_t total = 0;
    for (UInt32 i = 0; i < reader.db.NumFiles; i++) {
        SevenZipEntry e = reader.entry(i);
        if (!TarArchive::isSafeName(e.name)) {
            error = "refusing entry " + e.name;
            return false;
        }
        total += e.size;
        entries.push_back(e);
    }
    if (!DirEntry::createDirs(destDir)) {
        error = "cannot create " + destDir;
        return false;
    }
    const string dest = destDir + sep;
    Byte *outBuffer = nullptr;
    size_t outBufferSize = 0;
    UInt32 blockIndex = 0xFFFFFFFF;
    uint64_t done = 0;
    bool ok = true;
    for (UInt32 i = 0; i < reader.db.NumFiles && ok; i++) {
        const SevenZipEntry &e = entries[i];
        done += e.size;
        if (!prefix.empty() && e.name.compare(0, prefix.size(), prefix) != 0)
            continue;
        if (filter && !filter(e))
            continue;
        const string target = dest + e.name.substr(prefix.size());
        if (e.isDir) {
            if (!DirEntry::createDirs(DirEntry::removeSeparatorFromEndOfPath(target))) {
                error = "cannot create " + target;
                ok = false;
            }
            continue;
        }
        size_t offset = 0, outSize = 0;
        SRes res = SzArEx_Extract(&reader.db, &reader.lookStream.vt, i, &blockIndex, &outBuffer, &outBufferSize,
                                  &offset, &outSize, &allocMain, &allocTemp);
        if (res != SZ_OK) {
            error = string(resultText(res)) + " (" + e.name + ")";
            ok = false;
            break;
        }
        size_t slash = target.find_last_of('/');
        if (slash != string::npos && !DirEntry::createDirs(target.substr(0, slash))) {
            error = "cannot create the directory for " + target;
            ok = false;
            break;
        }
        ofstream out(target, ios::binary | ios::trunc);
        if (!out || !out.write(reinterpret_cast<const char *>(outBuffer + offset), static_cast<streamsize>(outSize))) {
            error = "cannot write " + target;
            ok = false;
            break;
        }
        if (progress)
            progress(done, total);
    }
    ISzAlloc_Free(&allocMain, outBuffer);
    return ok;
}

#endif

} // namespace ableem
