#include "ableem/engine/serial_scanner.h"
#include "ableem/engine/filesystem.h"
#include "ableem/engine/iso_directory.h"
#include "ableem/engine/strings.h"
#include "binary_reader.h"
#include "md5.h"

#include <algorithm>
#include <sstream>
#include <fstream>
#include <iostream>
#include <vector>
#include "ableem/engine/log.h"

using namespace std;

namespace ableem {

// The SerialScanner class reads the serial number in a CDROM BIN file which is an ISO 9660 image of a CDROM.
// https://en.wikipedia.org/wiki/ISO_9660

//*******************************
// SerialScanner::normalizeSerial
//*******************************
string SerialScanner::normalizeSerial(string serial) {
    replace(serial.begin(), serial.end(), '_', '-');
    serial.erase(remove(serial.begin(), serial.end(), '.'), serial.end());
    string fixed = "";
    stringstream alpha;
    stringstream digits;
    bool digitsProcessing = false;
    for (int i = 0; i < serial.size(); i++) {
        int maxchars = serial[0] == 'L' ? 3 : 4;
        if (!isdigit(serial[i])) {

            if (digitsProcessing)
                continue;
            if (serial[i] == '-')
                continue;
            if (alpha.str().length() < maxchars) {
                alpha << serial[i];
            }
        } else {
            digitsProcessing = true;
            digits << serial[i];
        }
    }
    return alpha.str() + "-" + digits.str();
}

//*******************************
// SerialScanner::readSerial
//*******************************
string SerialScanner::readSerial(ImageType imageType, string path, string firstBinPath) {
    string serial = readSerialFromImage(imageType, path, firstBinPath);
    PLOG_INFO << serial;
    if (serial.empty()) {
        serial = readSerialByWorkaround(imageType, path, firstBinPath);
    }
    return serial;
}

//*******************************
// SerialScanner::readSerialFromImage
//*******************************
string SerialScanner::readSerialFromImage(ImageType imageType, string path, string firstBinPath) {
    PLOG_INFO << imageType << "   " << path << "   " << firstBinPath;
    if (imageType == IMAGE_PBP) {
        string destinationDir = path;
        string pbpFileName = DirEntry::findFirstFile(EXT_PBP, destinationDir);
        if (pbpFileName != "") {
            ifstream is;
            is.open(destinationDir + sep + pbpFileName, ios::binary);
            if (!is.is_open()) {
                PLOG_WARNING << "Cannot open PBP: " << destinationDir + sep + pbpFileName;
                return "";
            }

            long magic = readUint32LE(is);
            if (magic != 0x50425000) {
                return "";
            }
            long second = readUint32LE(is);
            if (second != 0x10000) {
                return "";
            }
            long sfoStart = readUint32LE(is);

            is.seekg(sfoStart, ios::beg);

            unsigned int signature = readUint32LE(is);
            if (signature != 1179865088) {
                return "";
            }
            readUint32LE(is); // version
            unsigned int fields_table_offs = readUint32LE(is);
            unsigned int values_table_offs = readUint32LE(is);
            int nitems = readUint32LE(is);

            vector<string> fields;
            vector<string> values;
            fields.clear();
            values.clear();
            is.seekg(sfoStart, ios::beg);
            is.seekg(fields_table_offs, ios::cur);
            for (int i = 0; i < nitems; i++) {
                string fieldName = readCString(is);
                skipZeros(is);
                fields.push_back(fieldName);
            }

            is.seekg(sfoStart, ios::beg);
            is.seekg(values_table_offs, ios::cur);
            for (int i = 0; i < nitems; i++) {
                string valueName = readCString(is);
                skipZeros(is);
                values.push_back(valueName);
            }

            is.close();

            for (int i = 0; i < nitems; i++) {
                if (fields[i] == "DISC_ID") {
                    string potentialSerial = values[i];
                    return normalizeSerial(potentialSerial);
                }
            }
        }
    }
    if (DirEntry::imageTypeUsesACueFile(imageType) || (imageType == IMAGE_CHD)) {
        string prefixes[] = {"CPCS", "ESPM", "HPS",  "LPS",  "LSP",  "SCAJ", "SCED", "SCES",
                             "SCPS", "SCUS", "SIPS", "SLES", "SLKA", "SLPM", "SLPS", "SLUS"};
        if (firstBinPath == "") {
            return ""; // not at this stage
        }

        for (int level = 1; level < 4; level++) {
            IsoDirectory dir = IsoDirectoryReader::read(firstBinPath, level, imageType == IMAGE_CHD);
            string serialFound = "";
            if (!dir.rootDir.empty()) {
                for (const string &entry : dir.rootDir) {
                    string potentialSerial = normalizeSerial(entry);
                    for (const string &prefix : prefixes) {
                        int pos = potentialSerial.find(prefix.c_str(), 0);
                        if (pos == 0) {
                            serialFound = potentialSerial;
                            PLOG_INFO << "Serial number: " << serialFound;
                            return serialFound;
                        }
                    }
                }
                string volume = normalizeSerial(dir.volumeName);
                for (const string &prefix : prefixes) {
                    int pos = volume.find(prefix.c_str(), 0);
                    if (pos == 0) {
                        serialFound = volume;
                        PLOG_INFO << "Serial number: " << serialFound;
                        return serialFound;
                    }
                }

            } else {
                return "";
            }
        }
    }
    return "";
}

//*******************************
// SerialScanner::readSerialByWorkaround
//*******************************
string SerialScanner::readSerialByWorkaround(ImageType imageType, string path, string firstBinPath) {
    string fileToScan = "";
    if (DirEntry::imageTypeUsesACueFile(imageType)) {
        fileToScan = firstBinPath;
    }
    if (imageType == IMAGE_PBP) {
        fileToScan = DirEntry::findFirstFile(EXT_PBP, path);
    }
    if (imageType == IMAGE_CHD) {
        fileToScan = DirEntry::findFirstFile(EXT_CHD, path);
    }

    // BH2 - Resident Evil 1.5
    if (fileToScan.find("BH2") != string::npos) {
        return serialFromMd5(fileToScan);
    }
    return "";
}

//*******************************
// SerialScanner::serialFromMd5
//*******************************
string SerialScanner::serialFromMd5(string scanFile) {
    const size_t oneMb = 1024 * 1024;
    ifstream is(scanFile, ios::binary);
    if (!is.is_open()) {
        return "";
    }
    is.seekg(0, ios::end);
    size_t fileSize = (size_t)is.tellg();
    vector<unsigned char> buffer;

    // md5 of the first 1 MB ("head -c 1M")
    size_t headLen = fileSize < oneMb ? fileSize : oneMb;
    buffer.resize(headLen);
    is.seekg(0, ios::beg);
    is.read((char *)buffer.data(), headLen);
    string head = Md5::ofBytes(buffer.data(), headLen);

    // md5 of the last 1 MB ("tail -c 1M")
    size_t tailLen = headLen;
    buffer.resize(tailLen);
    is.seekg(fileSize - tailLen, ios::beg);
    is.read((char *)buffer.data(), tailLen);
    string tail = Md5::ofBytes(buffer.data(), tailLen);

    return head + tail;
}

//*******************************
// SerialScanner::serialToRegion
//*******************************
string SerialScanner::serialToRegion(const string &serial) {
    string region;
    if (serial.length() >= 3) {
        char regionCode = serial[2];
        if (regionCode == 'U')
            region = "US"; // SLUS, SCUS = NTSC-U
        else if (regionCode == 'E')
            region = "Europe-Aus"; // SLES, SCES = PAL
        else if (regionCode == 'P')
            region = "Japan"; // SLPS, SLPM, SCPS = NTSC-J
    }

    return region;
}

} // namespace ableem
