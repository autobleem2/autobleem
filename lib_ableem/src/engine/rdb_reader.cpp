#include "ableem/engine/rdb_reader.h"

#include <cctype>
#include <cstdio>
#include <cstring>
#include <iostream>
#include <utility>
#include <vector>
#include "ableem/engine/log.h"

using namespace std;

namespace ableem {

// the rmsgpack decoding below is AutoBleem-NG's, kept as it was
namespace {

// rmsgpack type prefixes (subset used by libretro-database .rdb files)
constexpr uint8_t MPF_NIL = 0xc0;
constexpr uint8_t MPF_FALSE = 0xc2;
constexpr uint8_t MPF_TRUE = 0xc3;
constexpr uint8_t MPF_BIN8 = 0xc4;
constexpr uint8_t MPF_BIN16 = 0xc5;
constexpr uint8_t MPF_BIN32 = 0xc6;
constexpr uint8_t MPF_UINT8 = 0xcc;
constexpr uint8_t MPF_UINT16 = 0xcd;
constexpr uint8_t MPF_UINT32 = 0xce;
constexpr uint8_t MPF_UINT64 = 0xcf;
constexpr uint8_t MPF_INT8 = 0xd0;
constexpr uint8_t MPF_INT16 = 0xd1;
constexpr uint8_t MPF_INT32 = 0xd2;
constexpr uint8_t MPF_INT64 = 0xd3;
constexpr uint8_t MPF_STR8 = 0xd9;
constexpr uint8_t MPF_STR16 = 0xda;
constexpr uint8_t MPF_STR32 = 0xdb;
constexpr uint8_t MPF_ARRAY16 = 0xdc;
constexpr uint8_t MPF_ARRAY32 = 0xdd;
constexpr uint8_t MPF_MAP16 = 0xde;
constexpr uint8_t MPF_MAP32 = 0xdf;

constexpr uint8_t FIXMAP_MASK = 0x80;   // 0x80..0x8f
constexpr uint8_t FIXARRAY_MASK = 0x90; // 0x90..0x9f
constexpr uint8_t FIXSTR_MASK = 0xa0;   // 0xa0..0xbf

class Cursor {
public:
    Cursor(const uint8_t *data, size_t size) : p(data), end(data + size) {}

    bool eof() const { return p >= end; }
    size_t remaining() const { return end > p ? static_cast<size_t>(end - p) : 0; }

    bool read_u8(uint8_t &out) {
        if (remaining() < 1)
            return false;
        out = *p++;
        return true;
    }
    bool read_be(uint64_t &out, size_t nbytes) {
        if (remaining() < nbytes)
            return false;
        out = 0;
        for (size_t i = 0; i < nbytes; i++)
            out = (out << 8) | p[i];
        p += nbytes;
        return true;
    }
    bool read_bytes(std::string &out, size_t n) {
        if (remaining() < n)
            return false;
        out.assign(reinterpret_cast<const char *>(p), n);
        p += n;
        return true;
    }
    bool skip(size_t n) {
        if (remaining() < n)
            return false;
        p += n;
        return true;
    }

private:
    const uint8_t *p;
    const uint8_t *end;
};

// Read any rmsgpack value, discarding its contents but advancing the cursor.
bool skip_value(Cursor &c);

// Read an rmsgpack string or binary blob into `out`. libretro-database stores
// some textual fields (serial, crc, md5, sha1) as BIN, others as STR — accept
// both since AutoBleem treats them as opaque strings.
bool read_string(Cursor &c, std::string &out) {
    uint8_t type;
    if (!c.read_u8(type))
        return false;

    uint64_t len = 0;
    if ((type & 0xe0) == FIXSTR_MASK) {
        len = type & 0x1f;
    } else if (type == MPF_STR8 || type == MPF_BIN8) {
        if (!c.read_be(len, 1))
            return false;
    } else if (type == MPF_STR16 || type == MPF_BIN16) {
        if (!c.read_be(len, 2))
            return false;
    } else if (type == MPF_STR32 || type == MPF_BIN32) {
        if (!c.read_be(len, 4))
            return false;
    } else {
        return false;
    }
    return c.read_bytes(out, len);
}

// Read an rmsgpack unsigned integer. Accepts positive fixint, uintN, and small intN.
bool read_uint(Cursor &c, uint64_t &out) {
    uint8_t type;
    if (!c.read_u8(type))
        return false;
    if (type < 0x80) {
        out = type;
        return true;
    }
    switch (type) {
    case MPF_UINT8:
        return c.read_be(out, 1);
    case MPF_UINT16:
        return c.read_be(out, 2);
    case MPF_UINT32:
        return c.read_be(out, 4);
    case MPF_UINT64:
        return c.read_be(out, 8);
    case MPF_INT8: {
        uint64_t v;
        if (!c.read_be(v, 1))
            return false;
        out = static_cast<uint64_t>(static_cast<int8_t>(v));
        return true;
    }
    case MPF_INT16: {
        uint64_t v;
        if (!c.read_be(v, 2))
            return false;
        out = static_cast<uint64_t>(static_cast<int16_t>(v));
        return true;
    }
    case MPF_INT32: {
        uint64_t v;
        if (!c.read_be(v, 4))
            return false;
        out = static_cast<uint64_t>(static_cast<int32_t>(v));
        return true;
    }
    case MPF_INT64:
        return c.read_be(out, 8);
    default:
        return false;
    }
}

bool skip_value(Cursor &c) {
    uint8_t type;
    if (!c.read_u8(type))
        return false;

    // positive fixint
    if (type < 0x80)
        return true;
    // negative fixint (0xe0..0xff)
    if (type >= 0xe0)
        return true;
    // fixmap
    if ((type & 0xf0) == FIXMAP_MASK) {
        size_t n = type & 0x0f;
        for (size_t i = 0; i < n; i++) {
            if (!skip_value(c))
                return false;
            if (!skip_value(c))
                return false;
        }
        return true;
    }
    // fixarray
    if ((type & 0xf0) == FIXARRAY_MASK) {
        size_t n = type & 0x0f;
        for (size_t i = 0; i < n; i++) {
            if (!skip_value(c))
                return false;
        }
        return true;
    }
    // fixstr
    if ((type & 0xe0) == FIXSTR_MASK) {
        return c.skip(type & 0x1f);
    }

    switch (type) {
    case MPF_NIL:
    case MPF_FALSE:
    case MPF_TRUE:
        return true;
    case MPF_UINT8:
    case MPF_INT8:
        return c.skip(1);
    case MPF_UINT16:
    case MPF_INT16:
        return c.skip(2);
    case MPF_UINT32:
    case MPF_INT32:
        return c.skip(4);
    case MPF_UINT64:
    case MPF_INT64:
        return c.skip(8);
    case MPF_BIN8:
    case MPF_STR8: {
        uint64_t len;
        if (!c.read_be(len, 1))
            return false;
        return c.skip(len);
    }
    case MPF_BIN16:
    case MPF_STR16: {
        uint64_t len;
        if (!c.read_be(len, 2))
            return false;
        return c.skip(len);
    }
    case MPF_BIN32:
    case MPF_STR32: {
        uint64_t len;
        if (!c.read_be(len, 4))
            return false;
        return c.skip(len);
    }
    case MPF_ARRAY16:
    case MPF_ARRAY32: {
        uint64_t n;
        if (!c.read_be(n, type == MPF_ARRAY16 ? 2 : 4))
            return false;
        for (uint64_t i = 0; i < n; i++) {
            if (!skip_value(c))
                return false;
        }
        return true;
    }
    case MPF_MAP16:
    case MPF_MAP32: {
        uint64_t n;
        if (!c.read_be(n, type == MPF_MAP16 ? 2 : 4))
            return false;
        for (uint64_t i = 0; i < n; i++) {
            if (!skip_value(c))
                return false;
            if (!skip_value(c))
                return false;
        }
        return true;
    }
    default:
        return false;
    }
}

// Read the map header for a record. Returns false if the next byte is NIL
// (end-of-records sentinel) or if no map header is present.
bool read_map_header(Cursor &c, uint32_t &nentries, bool &sentinel) {
    sentinel = false;
    uint8_t type;
    if (!c.read_u8(type))
        return false;
    if (type == MPF_NIL) {
        sentinel = true;
        return true;
    }
    if ((type & 0xf0) == FIXMAP_MASK) {
        nentries = type & 0x0f;
        return true;
    }
    if (type == MPF_MAP16) {
        uint64_t n;
        if (!c.read_be(n, 2))
            return false;
        nentries = static_cast<uint32_t>(n);
        return true;
    }
    if (type == MPF_MAP32) {
        uint64_t n;
        if (!c.read_be(n, 4))
            return false;
        nentries = static_cast<uint32_t>(n);
        return true;
    }
    return false;
}

bool slurp(const std::string &path, std::vector<uint8_t> &out) {
    FILE *f = std::fopen(path.c_str(), "rb");
    if (!f)
        return false;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz <= 0) {
        std::fclose(f);
        return false;
    }
    const size_t size = static_cast<size_t>(sz);
    out.resize(size);
    size_t got = std::fread(out.data(), 1, size, f);
    std::fclose(f);
    return got == size;
}

} // namespace

//*******************************
// RdbReader::open
//*******************************
bool RdbReader::open(const string &path) {
    records_.clear();
    bySerial_.clear();
    byName_.clear();
    valid_ = false;

    vector<uint8_t> buf;
    if (!slurp(path, buf)) {
        PLOG_WARNING << "rdb: cannot read " << path;
        return false;
    }

    // Header: 8 bytes "RARCHDB\0" + 8-byte big-endian metadata offset.
    // Some databases use a zero metadata offset; in that case records run to EOF.
    if (buf.size() < 16 || memcmp(buf.data(), "RARCHDB\0", 8) != 0) {
        PLOG_INFO << "rdb: bad magic in " << path;
        return false;
    }
    uint64_t metadata_offset = 0;
    for (int i = 0; i < 8; i++)
        metadata_offset = (metadata_offset << 8) | buf[8 + i];
    if ((metadata_offset != 0 && metadata_offset < 16) || metadata_offset > buf.size()) {
        PLOG_WARNING << "rdb: invalid metadata offset in " << path;
        return false;
    }
    const size_t record_end = metadata_offset == 0 ? buf.size() : static_cast<size_t>(metadata_offset);

    Cursor c(buf.data() + 16, record_end - 16);

    while (!c.eof()) {
        uint32_t nentries = 0;
        bool sentinel = false;
        if (!read_map_header(c, nentries, sentinel)) {
            PLOG_INFO << "rdb: malformed record header at offset " << (buf.size() - c.remaining()) << " in " << path;
            return false;
        }
        if (sentinel)
            break;

        Record rec;
        for (uint32_t i = 0; i < nentries; i++) {
            string key;
            if (!read_string(c, key)) {
                PLOG_INFO << "rdb: bad key at offset " << (buf.size() - c.remaining()) << " in " << path;
                return false;
            }
            bool ok = true;
            if (key == "name")
                ok = read_string(c, rec.name);
            else if (key == "region")
                ok = read_string(c, rec.region);
            else if (key == "serial")
                ok = read_string(c, rec.serial);
            else if (key == "publisher")
                ok = read_string(c, rec.publisher);
            else if (key == "developer")
                ok = read_string(c, rec.developer);
            else if (key == "genre")
                ok = read_string(c, rec.genre);
            else if (key == "releaseyear" || key == "releasemonth" || key == "users") {
                uint64_t v = 0;
                ok = read_uint(c, v);
                if (key == "releaseyear")
                    rec.releaseyear = static_cast<int>(v);
                else if (key == "releasemonth")
                    rec.releasemonth = static_cast<int>(v);
                else
                    rec.users = static_cast<int>(v);
            } else {
                ok = skip_value(c);
            }
            if (!ok) {
                PLOG_INFO << "rdb: bad value for " << key << " at offset " << (buf.size() - c.remaining()) << " in "
                          << path;
                return false;
            }
        }

        size_t idx = records_.size();
        records_.push_back(std::move(rec));
        const Record &r = records_.back();
        if (!r.serial.empty())
            bySerial_.emplace(r.serial, idx);
        if (!r.name.empty())
            byName_.emplace(r.name, idx);
    }

    valid_ = true;
    PLOG_INFO << "rdb: loaded " << records_.size() << " records from " << path;
    return true;
}

//*******************************
// RdbReader::findBySerial
//*******************************
const RdbReader::Record *RdbReader::findBySerial(const string &serial) const {
    auto it = bySerial_.find(serial);
    if (it != bySerial_.end())
        return &records_[it->second];

    // a record whose serial starts with this one followed by a non-digit: re-release suffixes
    // ("SLUS-01251GH"), disc separators ("SLUS-00594-1") - but not "SLUS-012510" for "SLUS-01251"
    for (const auto &record : records_) {
        if (record.serial.size() <= serial.size())
            continue;
        if (record.serial.compare(0, serial.size(), serial) != 0)
            continue;
        const char next = record.serial[serial.size()];
        if (isdigit(static_cast<unsigned char>(next)))
            continue;
        return &record;
    }
    return nullptr;
}

//*******************************
// RdbReader::findByName
//*******************************
const RdbReader::Record *RdbReader::findByName(const string &name) const {
    auto it = byName_.find(name);
    if (it == byName_.end())
        return nullptr;
    return &records_[it->second];
}

} // namespace ableem
