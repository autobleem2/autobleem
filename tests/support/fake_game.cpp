#include "fake_game.h"

#include <ableem/engine/filesystem.h>

#include <cctype>
#include <cstdint>
#include <fstream>

using namespace std;

namespace test_support {

namespace {

const int SECTOR = 2352;
const int DATA = 2048;

// pads (never truncates) s to at least width bytes with pad - Python's str.ljust()
string ljust(string s, size_t width, char pad = ' ') {
    if (s.size() < width)
        s.append(width - s.size(), pad);
    return s;
}

// 4 bytes little-endian followed by 4 bytes big-endian - the "both-endian" 32-bit fields ISO9660 uses
string bothEndian32(uint32_t v) {
    string out(8, '\x00');
    out[0] = static_cast<char>(v & 0xFF);
    out[1] = static_cast<char>((v >> 8) & 0xFF);
    out[2] = static_cast<char>((v >> 16) & 0xFF);
    out[3] = static_cast<char>((v >> 24) & 0xFF);
    out[4] = static_cast<char>((v >> 24) & 0xFF);
    out[5] = static_cast<char>((v >> 16) & 0xFF);
    out[6] = static_cast<char>((v >> 8) & 0xFF);
    out[7] = static_cast<char>(v & 0xFF);
    return out;
}

string bothEndian16(uint16_t v) {
    string out(4, '\x00');
    out[0] = static_cast<char>(v & 0xFF);
    out[1] = static_cast<char>((v >> 8) & 0xFF);
    out[2] = static_cast<char>((v >> 8) & 0xFF);
    out[3] = static_cast<char>(v & 0xFF);
    return out;
}

// one ISO9660 directory record: length byte, then extended-attribute length, extent, size, date, flags,
// unit/gap, volume sequence number, the name's length and the name itself (plus a pad byte to keep the
// whole record even-length)
string dirRecord(const string &name, uint32_t extent, uint32_t size, bool isDir) {
    string rec;
    rec += '\x00';                                      // extended attribute length
    rec += bothEndian32(extent);
    rec += bothEndian32(size);
    rec.append(7, '\x00');                               // recording date
    rec += static_cast<char>(isDir ? 0x02 : 0x00);       // flags
    rec.append(2, '\x00');                               // unit size, gap
    rec += bothEndian16(1);                              // volume sequence number
    rec += static_cast<char>(static_cast<unsigned char>(name.size()));
    rec += name;
    if (rec.size() % 2 == 0)
        rec += '\x00';

    string result;
    result += static_cast<char>(static_cast<unsigned char>(rec.size() + 1));
    result += rec;
    return result;
}

// wraps up to DATA bytes of data in a MODE2 form-1 raw sector: sync, header (minute/second/frame in BCD,
// mode 2), subheader, the data padded to DATA, then zero-padded to SECTOR (no ECC/EDC, which nothing here
// checks)
string rawSector(int number, string data) {
    data = ljust(data, DATA, '\x00');

    string sync;
    sync += '\x00';
    sync.append(10, '\xFF');
    sync += '\x00';

    int total = number + 150;
    int minutes = total / (75 * 60);
    int rest = total % (75 * 60);
    int seconds = rest / 75;
    int frames = rest % 75;
    auto bcd = [](int v) -> unsigned char { return static_cast<unsigned char>(((v / 10) << 4) | (v % 10)); };

    string header;
    header += static_cast<char>(bcd(minutes));
    header += static_cast<char>(bcd(seconds));
    header += static_cast<char>(bcd(frames));
    header += static_cast<char>(0x02);

    string subheader;
    for (int i = 0; i < 2; i++) {
        subheader += static_cast<char>(0x00);
        subheader += static_cast<char>(0x00);
        subheader += static_cast<char>(0x08);
        subheader += static_cast<char>(0x00);
    }

    string body = sync + header + subheader + data;
    return ljust(body, SECTOR, '\x00');
}

// the ISO9660 image itself: PVD in sector 16, the volume descriptor set terminator in 17, the root
// directory (SYSTEM.CNF + the serial file) in sector 18
string makeIso(const string &title, const string &serialFile, int sectors = 24) {
    const int rootSector = 18;
    const int cnfSector = 19;
    const int serialSector = 20;

    string systemCnf = "BOOT = cdrom:\\" + serialFile + ";1\r\nTCB = 4\r\nEVENT = 10\r\nSTACK = 801FFFF0\r\n";

    string root;
    root += dirRecord(string(1, '\x00'), rootSector, DATA, true);
    root += dirRecord(string(1, '\x01'), rootSector, DATA, true);
    root += dirRecord(serialFile + ";1", serialSector, 4, false);
    root += dirRecord("SYSTEM.CNF;1", cnfSector, static_cast<uint32_t>(systemCnf.size()), false);

    string volumeId;
    for (char c : title)
        if (c != ' ')
            volumeId += static_cast<char>(toupper(static_cast<unsigned char>(c)));

    string pvd;
    pvd += '\x01';
    pvd += "CD001";
    pvd += '\x01';
    pvd += '\x00';
    pvd += ljust("PLAYSTATION", 32, ' ');                    // system identifier
    pvd += ljust(volumeId, 32, ' ');                         // volume identifier
    pvd.append(8, '\x00');                                   // unused
    pvd += bothEndian32(static_cast<uint32_t>(sectors));     // volume space size
    pvd.append(32, '\x00');                                  // unused
    pvd += bothEndian16(1);                                  // volume set size
    pvd += bothEndian16(1);                                  // volume sequence number
    pvd += bothEndian16(static_cast<uint16_t>(DATA));        // logical block size
    pvd += bothEndian32(10);                                 // path table size
    pvd.append(16, '\x00');                                  // the four path table locations (unused, none)

    string rootRec = ljust(dirRecord(string(1, '\x00'), rootSector, DATA, true), 34, '\x00');
    pvd += rootRec;
    pvd = ljust(pvd, DATA, '\x00');

    string terminator;
    terminator += static_cast<char>(0xFF);
    terminator += "CD001";
    terminator += static_cast<char>(0x01);

    string image;
    for (int n = 0; n < sectors; n++) {
        string data;
        if (n == 16) data = pvd;
        else if (n == 17) data = terminator;
        else if (n == rootSector) data = root;
        else if (n == cnfSector) data = systemCnf;
        else if (n == serialSector) data = "fake";
        image += rawSector(n, data);
    }
    return image;
}

} // namespace

//*******************************
// makeFakeGame
//*******************************
void makeFakeGame(const string &gamesDir, const string &title, const string &serialFile) {
    ableem::DirEntry::createDir(gamesDir);   // a no-op if it already exists
    string folder = gamesDir + ableem::sep + title;
    ableem::DirEntry::createDir(folder);

    string iso = makeIso(title, serialFile);
    ofstream bin(folder + ableem::sep + title + ".bin", ios::binary);
    bin.write(iso.data(), static_cast<streamsize>(iso.size()));
    bin.close();

    ofstream cue(folder + ableem::sep + title + ".cue", ios::binary);
    cue << "FILE \"" << title << ".bin\" BINARY\n  TRACK 01 MODE2/2352\n    INDEX 01 00:00:00\n";
    cue.close();
}

} // namespace test_support
