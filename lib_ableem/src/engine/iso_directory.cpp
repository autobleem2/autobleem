#include "ableem/engine/iso_directory.h"
#include "ableem/engine/strings.h"
#include "cd_image_reader.h"

#include <memory>
#include <fstream>
#include <iostream>
#include "ableem/engine/log.h"

using namespace std;

namespace ableem {

namespace {

//*******************************
// removeVersion
// "SLUS_012.34;1" -> "SLUS_012.34"
//*******************************
string removeVersion(string input) {
    int len = input.length();
    if (len > 2) {
        int semiColon = input.find(';', 0);
        if (semiColon != -1) {
            input = input.substr(0, input.find(';', 0));
        }
    }
    return input;
}

//*******************************
// emptyDir
//*******************************
IsoDirectory emptyDir() {
    IsoDirectory dir;
    dir.systemName = "UNKNOWN";
    dir.volumeName = "UNKNOWN";
    return dir;
}

//*******************************
// readDir
// walks one directory extent, recursing into sub-directories up to maxlevel
//*******************************
void readDir(vector<string> *data, CdImageReader *reader, unsigned int sector, int maxlevel, int level) {
    int originalSector = sector;
    if (level >= maxlevel) {
        return;
    }
    reader->selectSector(sector);
    for (int i = 0; i < 200; i++) {
        sector = reader->getSelSector();
        if (reader->endStream()) {
            break;
        }
        int startpos = reader->getSectorPos();
        int startsector = reader->getSelSector();
        long len = reader->readChar();

        if (len == 0) {
            int lastReadPos = reader->getSectorPos();
            reader->selectSector(sector + 1);
            sector = reader->getSelSector();
            if (lastReadPos < 2048 - 62) {
                break;
            }
            continue;
        }
        reader->readChar();                     // extended attribute record length
        unsigned int loc = reader->readDword(); // extent location (LE half)
        reader->ffd(4);                         // extent location (BE half)
        reader->ffd(8);                         // data length
        reader->ffd(7);                         // recording date
        int attr = reader->readChar();          // file flags: bit 1 = directory
        reader->ffd(6);                         // unit size, gap, volume sequence number

        int fileNameLen = reader->readChar();
        if (fileNameLen < 2) {
            reader->selectSector(startsector);
            reader->ffd(startpos + len);
            if (fileNameLen == 0)
                break;
            continue;
        }
        string fileName = reader->readString(fileNameLen);
        data->push_back(removeVersion(fileName));
        if ((attr >> 1) & 1) {
            readDir(data, reader, loc, maxlevel, level + 1);
        }
        reader->selectSector(sector);
        reader->ffd(startpos + len);
    }
    reader->selectSector(originalSector);
}

} // namespace

//*******************************
// IsoDirectoryReader::read
//*******************************
IsoDirectory IsoDirectoryReader::read(const string &imagePath, int maxLevel, bool isChd) {
    unique_ptr<CdImageReader> reader;

    if (!isChd) {
        reader.reset(new CdImageReader());
    } else {
#ifdef ABLEEM_NO_CHD
        PLOG_WARNING << "CHD support not compiled in, skipping " << imagePath;
        return emptyDir();
#else
        reader.reset(new ChdImageReader());
#endif
    }
    reader->openImage(imagePath);
    if (!reader->isOpen()) {
        return emptyDir();
    }
    reader->selectSector(16);           // primary volume descriptor
    reader->ffd(8);
    string system = reader->readString(32);
    string volname = reader->readString(32);
    reader->ffd(86);
    int sector = reader->readDword();   // root directory record: extent location
    reader->selectSector(sector);
    IsoDirectory result;
    result.systemName = trim(system);
    result.volumeName = trim(volname);
    readDir(&result.rootDir, reader.get(), sector, maxLevel, 0);
    reader->closeImage();
    if (result.rootDir.empty()) {
        return emptyDir();
    }
    return result;
}

} // namespace ableem
