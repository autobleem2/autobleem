#include "ableem/engine/tar_archive.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>

#include <miniz.h>

#include "ableem/engine/filesystem.h"
#include "ableem/engine/log.h"

using namespace std;

namespace ableem {

namespace {

const size_t Block = 512;

//******************
// Source
//******************
// the tar stream: the file as it is, or inflated on the way when it starts with gzip's magic
class Source {
public:
    explicit Source(const string &path) : file_(fopen(path.c_str(), "rb")) {
        if (!file_)
            return;
        fseek(file_, 0, SEEK_END);
        total_ = static_cast<uint64_t>(ftell(file_));
        fseek(file_, 0, SEEK_SET);
        unsigned char magic[2] = {0, 0};
        size_t got = fread(magic, 1, 2, file_);
        fseek(file_, 0, SEEK_SET);
        gzip_ = got == 2 && magic[0] == 0x1f && magic[1] == 0x8b;
        if (gzip_) {
            if (!skipGzipHeader())
                fail("not a gzip file");
            memset(&stream_, 0, sizeof(stream_));
            if (mz_inflateInit2(&stream_, -MZ_DEFAULT_WINDOW_BITS) != MZ_OK)
                fail("inflate init failed");
            inflating_ = true;
        }
    }
    ~Source() {
        if (inflating_)
            mz_inflateEnd(&stream_);
        if (file_)
            fclose(file_);
    }
    bool ok() const { return file_ != nullptr && error_.empty(); }
    const string &error() const { return error_; }
    uint64_t total() const { return total_; }
    uint64_t done() const { return file_ ? static_cast<uint64_t>(ftell(file_)) : 0; }

    // exactly `n` bytes, or false at the end of the stream / on an error
    bool read(unsigned char *out, size_t n) {
        if (!ok())
            return false;
        if (!gzip_)
            return fread(out, 1, n, file_) == n;
        size_t have = 0;
        while (have < n) {
            if (stream_.avail_in == 0 && !eof_) {
                size_t got = fread(in_, 1, sizeof(in_), file_);
                if (got == 0)
                    eof_ = true;
                stream_.next_in = in_;
                stream_.avail_in = static_cast<unsigned>(got);
            }
            stream_.next_out = out + have;
            stream_.avail_out = static_cast<unsigned>(n - have);
            int status = mz_inflate(&stream_, MZ_NO_FLUSH);
            size_t produced = (n - have) - stream_.avail_out;
            have += produced;
            if (status == MZ_STREAM_END) {
                streamEnd_ = true;
                break;
            }
            if (status != MZ_OK && status != MZ_BUF_ERROR) {
                fail("corrupt gzip data");
                return false;
            }
            if (status == MZ_BUF_ERROR && eof_ && produced == 0)
                break;
        }
        return have == n;
    }
    bool skip(uint64_t n) {
        unsigned char buf[8192];
        while (n > 0) {
            size_t chunk = static_cast<size_t>(n < sizeof(buf) ? n : sizeof(buf));
            if (!read(buf, chunk))
                return false;
            n -= chunk;
        }
        return true;
    }

private:
    void fail(const string &what) {
        if (error_.empty())
            error_ = what;
    }
    // RFC 1952: the fixed header, then the optional extra/name/comment/crc fields
    bool skipGzipHeader() {
        unsigned char h[10];
        if (fread(h, 1, 10, file_) != 10 || h[2] != 8)
            return false;
        const unsigned flags = h[3];
        if (flags & 4) { // FEXTRA
            unsigned char len[2];
            if (fread(len, 1, 2, file_) != 2)
                return false;
            fseek(file_, len[0] | (len[1] << 8), SEEK_CUR);
        }
        for (unsigned bit : {8u, 16u}) { // FNAME, FCOMMENT: zero-terminated
            if (flags & bit) {
                int c;
                while ((c = fgetc(file_)) != 0 && c != EOF) {
                }
            }
        }
        if (flags & 2) // FHCRC
            fseek(file_, 2, SEEK_CUR);
        return true;
    }

    FILE *file_;
    uint64_t total_ = 0;
    bool gzip_ = false, inflating_ = false, eof_ = false, streamEnd_ = false;
    mz_stream stream_;
    unsigned char in_[65536];
    string error_;
};

uint64_t octal(const unsigned char *field, size_t len) {
    uint64_t v = 0;
    // GNU base-256 for sizes over 8 GB - not something a package of ours has, but read it right
    if (len > 0 && (field[0] & 0x80)) {
        for (size_t i = 1; i < len; i++)
            v = (v << 8) | field[i];
        return v;
    }
    for (size_t i = 0; i < len; i++) {
        unsigned char c = field[i];
        if (c == ' ' || c == 0)
            continue;
        if (c < '0' || c > '7')
            break;
        v = v * 8 + (c - '0');
    }
    return v;
}

string field(const unsigned char *p, size_t len) {
    size_t n = 0;
    while (n < len && p[n] != 0)
        n++;
    return string(reinterpret_cast<const char *>(p), n);
}

string cleanName(string name) {
    while (name.rfind("./", 0) == 0)
        name = name.substr(2);
    while (!name.empty() && name[0] == '/')
        name = name.substr(1);
    if (!name.empty() && name.back() == '/')
        name.pop_back();
    return name;
}

// walks the archive, handing each entry and a way to read or skip its data to `visit`; visit returns
// false to stop. `error` is set on a malformed archive.
bool walk(const string &tarPath, string &error,
          const function<bool(const TarEntry &, Source &, uint64_t padded)> &visit) {
    Source src(tarPath);
    if (!src.ok()) {
        error = src.error().empty() ? "cannot open " + tarPath : src.error();
        return false;
    }
    unsigned char h[Block];
    string longName;
    while (src.read(h, Block)) {
        bool zero = true;
        for (size_t i = 0; i < Block && zero; i++)
            zero = h[i] == 0;
        if (zero)
            break; // the end-of-archive blocks
        const uint64_t size = octal(h + 124, 12);
        const uint64_t padded = (size + Block - 1) / Block * Block;
        const char type = h[156] == 0 ? '0' : static_cast<char>(h[156]);
        const bool ustar = memcmp(h + 257, "ustar", 5) == 0;
        if (type == 'L') { // GNU: the next entry's name
            string data(static_cast<size_t>(size), '\0');
            if (size > 0 && !src.read(reinterpret_cast<unsigned char *>(&data[0]), static_cast<size_t>(size)))
                break;
            src.skip(padded - size);
            longName = field(reinterpret_cast<const unsigned char *>(data.data()), data.size());
            continue;
        }
        TarEntry e;
        string name = field(h, 100);
        if (ustar && h[345] != 0)
            name = field(h + 345, 155) + "/" + name;
        if (!longName.empty()) {
            name = longName;
            longName.clear();
        }
        e.name = cleanName(name);
        e.size = size;
        e.mode = static_cast<unsigned>(octal(h + 100, 8));
        e.isDir = type == '5' || (type == '0' && !name.empty() && name.back() == '/');
        e.isSymlink = type == '2' || type == '1';
        e.isFile = type == '0' || type == '7';
        if (e.isDir)
            e.isFile = false;
        if (type == 'x' || type == 'g') { // pax headers: skipped (the names are in the ustar fields too)
            src.skip(padded);
            continue;
        }
        if (!visit(e, src, padded))
            return src.error().empty();
    }
    if (!src.error().empty()) {
        error = src.error();
        return false;
    }
    return true;
}

} // namespace

//*******************************
// TarArchive::isSafeName
//*******************************
bool TarArchive::isSafeName(const string &name) {
    if (name.empty() || name[0] == '/' || name.find('\\') != string::npos || name.find(':') != string::npos)
        return false;
    size_t pos = 0;
    while (pos <= name.size()) {
        size_t next = name.find('/', pos);
        string part = name.substr(pos, next == string::npos ? string::npos : next - pos);
        if (part == "..")
            return false;
        if (next == string::npos)
            break;
        pos = next + 1;
    }
    return true;
}

//*******************************
// TarArchive::list
//*******************************
bool TarArchive::list(const string &tarPath, vector<TarEntry> &entries, string &error) {
    entries.clear();
    return walk(tarPath, error, [&](const TarEntry &e, Source &src, uint64_t padded) {
        entries.push_back(e);
        return src.skip(padded);
    });
}

//*******************************
// TarArchive::readEntry
//*******************************
bool TarArchive::readEntry(const string &tarPath, const string &name, string &data, string &error) {
    bool found = false;
    bool walked = walk(tarPath, error, [&](const TarEntry &e, Source &src, uint64_t padded) {
        if (e.name != name || !e.isFile)
            return src.skip(padded);
        data.assign(static_cast<size_t>(e.size), '\0');
        if (e.size > 0 && !src.read(reinterpret_cast<unsigned char *>(&data[0]), static_cast<size_t>(e.size)))
            return false;
        found = true;
        return false; // done
    });
    if (!walked)
        return false;
    if (!found)
        error = "no " + name + " in " + tarPath;
    return found;
}

//*******************************
// TarArchive::extract
//*******************************
bool TarArchive::extract(const string &tarPath, const string &destDir, string &error, const Filter &filter,
                         const Progress &progress, const string &prefix) {
    if (!DirEntry::createDirs(destDir)) {
        error = "cannot create " + destDir;
        return false;
    }
    bool ok = true;
    bool walked = walk(tarPath, error, [&](const TarEntry &e, Source &src, uint64_t padded) {
        if (progress)
            progress(src.done(), src.total());
        string name = e.name;
        if (!prefix.empty()) {
            if (name.rfind(prefix, 0) != 0)
                return src.skip(padded);
            name = name.substr(prefix.size());
            if (name.empty())
                return src.skip(padded);
        }
        if (name.empty()) // "./", the archive's root
            return src.skip(padded);
        if (!isSafeName(name)) {
            error = "refusing entry " + e.name;
            ok = false;
            return false;
        }
        if ((filter && !filter(e)) || e.isSymlink || (!e.isFile && !e.isDir))
            return src.skip(padded);
        const string target = destDir + "/" + name;
        if (e.isDir) {
            if (!DirEntry::createDirs(target)) {
                error = "cannot create " + target;
                ok = false;
                return false;
            }
            return src.skip(padded);
        }
        size_t slash = target.find_last_of('/');
        if (slash != string::npos && !DirEntry::createDirs(target.substr(0, slash))) {
            error = "cannot create " + target.substr(0, slash);
            ok = false;
            return false;
        }
        ofstream out(target, ios::binary | ios::trunc);
        if (!out) {
            error = "cannot write " + target;
            ok = false;
            return false;
        }
        unsigned char buf[65536];
        uint64_t left = e.size;
        while (left > 0) {
            size_t chunk = static_cast<size_t>(left < sizeof(buf) ? left : sizeof(buf));
            if (!src.read(buf, chunk)) {
                error = "truncated archive at " + e.name;
                ok = false;
                return false;
            }
            out.write(reinterpret_cast<const char *>(buf), static_cast<streamsize>(chunk));
            left -= chunk;
        }
        out.close();
        if (!out) {
            error = "cannot write " + target;
            ok = false;
            return false;
        }
        return src.skip(padded - e.size);
    });
    if (walked && ok && progress) {
        progress(1, 1);
    }
    return walked && ok;
}

} // namespace ableem
