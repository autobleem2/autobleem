// lib_ableem - engine (private): sector-level readers for PS1 disc images. Used by IsoDirectoryReader only.
#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#ifndef ABLEEM_NO_CHD
#include <libchdr/chd.h>
#include <libchdr/cdrom.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <ableem/engine/log.h>

#define SECTOR_SIZE 2352
#ifndef CD_FRAME_SIZE
#define CD_FRAME_SIZE 2352   // one raw sector (libchdr/cdrom.h defines the same when CHD is compiled in)
#endif
#define DATA_SIZE 2048
#define MAX_OFFSET 500

namespace ableem {

using std::string;
using std::vector;
using std::ifstream;
using std::ios;


// Reads a raw 2352-byte-sector disc image (.bin/.img) as 2048-byte data sectors; calibrate() finds the
// data offset by locating the "CD001" ISO9660 marker in sector 16.
class CdImageReader
{
private:
    ifstream stream;
    int offset = 0;
    vector<char> buffer;    // one frame (CD_FRAME_SIZE); allocated by openImage, freed with the reader
    int sectorpos = 0;
    int currentSector = 0;
    bool opened = false;

public:
    virtual ~CdImageReader() { }

    void setOffset(int off)
    {
        offset = off;
    }
    int getOffset()
    {
        return offset;
    }
    void allocateBuffer()
    {
        buffer.assign(CD_FRAME_SIZE, 0);
    }
    char *getBuffer()
    {
        return buffer.data();
    }
    int getSectorPos()
    {
        return sectorpos;
    }
    void setSectorpos(int pos)
    {
        sectorpos = pos;
    }

    void setCurrentSector(int pos)
    {
        currentSector = pos;
    }
    int getCurrentSector()
    {
        return currentSector;
    }
    bool isOpen()
    {
        return opened;
    }
    void setOpen(bool state)
    {
        opened =state;
    }

    int calibrate(int maxOffset)
    {
        unsigned char c = 0;
        selectSector(16); // go to sector 16
        for (int i = 0; i < maxOffset; i++)
        {
            c = readChar();
            if (c != 1)
            {
                c = readChar();
            }
            else
            {
                int sectorPosNow = getSectorPos();
                int curSectorNow = getSelSector();
                string str = readString(5);
                rev(5);
                if (str == "CD001")
                {
                    PLOG_DEBUG << "CD001 found in sector:" << curSectorNow << " at pos " << sectorPosNow;
                    offset = (curSectorNow - 16) * SECTOR_SIZE + sectorPosNow - 1;
                    return 0;
                }
                else
                {
                    c = readChar();
                }
            }
        }
        return -1;
    }
    void ffd(int size)
    {
        for (int i = 0; i < size; i++)
        {
            sectorpos++;
            if (sectorpos >= DATA_SIZE)
            {
                selectSector(currentSector + 1);
            }
        }
    }
    void rev(int size)
    {
        for (int i = 0; i < size; i++)
        {
            sectorpos--;
            if (sectorpos < 0)
            {
                selectSector(currentSector - 1);
                sectorpos = (DATA_SIZE - 1);
            }
        }
    }
    unsigned char readChar()
    {
        unsigned char x;
        x = buffer[sectorpos];
        sectorpos++;
        if (sectorpos >= DATA_SIZE)
        {
            selectSector(currentSector + 1);
        }
        return x;
    }
    std::string readString(int size)
    {
        char str[size + 1];
        str[size] = 0;
        for (int i = 0; i < size; i++)
        {
            str[i] = readChar();
        }
        return str;
    }
    unsigned long readDword()
    {
        unsigned long res = 0;
        unsigned long c;
        c = readChar();
        res += c;
        c = readChar();
        res += c << (1 * 8);
        c = readChar();
        res += c << (2 * 8);
        c = readChar();
        res += c << (3 * 8);
        return res;
    }
    
    virtual void selectSector(int sectorNum)
    {
        int addr = sectorNum * SECTOR_SIZE + offset;
        stream.seekg(addr, ios::beg);
        stream.read(buffer.data(), SECTOR_SIZE);
        sectorpos = 0;
        currentSector = sectorNum;
    }
    int getSelSector()
    {
        return currentSector;
    }
    virtual int openImage(string imagePath)
    {
        allocateBuffer();
        PLOG_DEBUG << "Opening ISO image";
        offset = 0;
        opened = false;
        sectorpos = 0;
        currentSector = 0;
        stream.open(imagePath, ios::binary | ios::in);
        if (!stream.is_open() || !stream.good())
        {
            return -1;
        }
        stream.seekg(0);
        if (calibrate(MAX_OFFSET) != 0)
        {
            return -1;
        };
        opened = true;

        return 0;
    }
    virtual void closeImage()
    {
        stream.close();
        opened = false;
    }

    virtual bool endStream()
    {
        return stream.tellg() == -1;
    }
};

#ifndef ABLEEM_NO_CHD
// A CHD (MAME's compressed disc image) read through libchdr's hunk interface. A CD CHD stores every
// sector as one "unit" of unitbytes (2352 raw bytes + 96 of subcode, 2448) and packs a whole number of
// them into each hunk; the first track starts at unit 0, and CDROM_TRACK_METADATA(2) says how long it is
// and what mode it is in. Track 0 is all the ISO reader ever wants (SYSTEM.CNF lives on it), so that
// is all this reads: hunk by hunk into a cache, one unit out of it per selectSector(), and the base
// class's calibrate() then finds the 2048 data bytes inside the raw sector the way it does for a .bin.
// (The earlier libmamecd fork did the same through its cdrom_* layer; upstream libchdr has none.)
class ChdImageReader : public CdImageReader
{
private:
    chd_file *chd = nullptr;
    vector<uint8_t> hunk;                            // the decompressed hunk currently cached
    vector<uint8_t> unit;                            // one raw sector out of it
    uint32_t cachedHunk = static_cast<uint32_t>(-1);
    uint32_t hunkBytes = 0;
    uint32_t unitBytes = 0;
    uint32_t unitsPerHunk = 0;
    uint32_t trackFrames = 0;                        // sectors on track 0

    // CDROM_TRACK_METADATA2 first (it carries pregap/postgap too), else the older CDROM_TRACK_METADATA
    bool readTrackMetadata()
    {
        char metadata[256];
        uint32_t metalen = 0;
        int tracknum = 0, frames = 0, pregap = 0, postgap = 0;
        char type[32], subtype[32], pgtype[32], pgsub[32];

        if (chd_get_metadata(chd, CDROM_TRACK_METADATA2_TAG, 0, metadata, sizeof(metadata), &metalen, nullptr, nullptr) == CHDERR_NONE
            && sscanf(metadata, CDROM_TRACK_METADATA2_FORMAT, &tracknum, type, subtype, &frames, &pregap, pgtype, pgsub, &postgap) >= 4) {
            trackFrames = frames;
            return true;
        }
        if (chd_get_metadata(chd, CDROM_TRACK_METADATA_TAG, 0, metadata, sizeof(metadata), &metalen, nullptr, nullptr) == CHDERR_NONE
            && sscanf(metadata, CDROM_TRACK_METADATA_FORMAT, &tracknum, type, subtype, &frames) >= 4) {
            trackFrames = frames;
            return true;
        }
        return false;
    }

    bool readUnit(uint32_t unitNum, uint8_t *out)
    {
        uint32_t hunkNum = unitNum / unitsPerHunk;
        if (hunkNum != cachedHunk) {
            if (chd_read(chd, hunkNum, hunk.data()) != CHDERR_NONE)
                return false;
            cachedHunk = hunkNum;
        }
        memcpy(out, hunk.data() + (unitNum % unitsPerHunk) * unitBytes, unitBytes);
        return true;
    }

public:
    ~ChdImageReader() override { closeImage(); }

    void closeImage() override
    {
        setOpen(false);
        if (chd != nullptr)
        {
            chd_close(chd);
            chd = nullptr;
        }
        hunk.clear();
        cachedHunk = static_cast<uint32_t>(-1);
    }

    bool endStream() override
    {
        if (getCurrentSector() > static_cast<int>(trackFrames))
            setCurrentSector(trackFrames - 1);
        return (getCurrentSector() == static_cast<int>(trackFrames) - 1) && (getSectorPos() >= DATA_SIZE);
    }

    int openImage(string imagePath) override
    {
        allocateBuffer();
        PLOG_DEBUG << "Opening CHD image";
        setOpen(false);
        setOffset(0);
        setSectorpos(0);
        setCurrentSector(0);

        chd_error err = chd_open(imagePath.c_str(), CHD_OPEN_READ, nullptr, &chd);
        if (err != CHDERR_NONE)
        {
            PLOG_ERROR << "Error opening CHD file: " << chd_error_string(err);
            chd = nullptr;
            return -1;
        }

        const chd_header *header = chd_get_header(chd);
        hunkBytes = header->hunkbytes;
        unitBytes = header->unitbytes;
        // a CD CHD: a raw sector (plus subcode) per unit, a whole number of them per hunk
        if (unitBytes < CD_FRAME_SIZE || hunkBytes == 0 || hunkBytes % unitBytes != 0)
        {
            PLOG_ERROR << "Not a CD CHD (unit " << unitBytes << " bytes, hunk " << hunkBytes << ")";
            closeImage();
            return -1;
        }
        unitsPerHunk = hunkBytes / unitBytes;
        hunk.assign(hunkBytes, 0);
        unit.assign(unitBytes, 0);

        if (!readTrackMetadata() || trackFrames < 1)
        {
            PLOG_ERROR << "CHD has no CD track metadata";
            closeImage();
            return -1;
        }
        PLOG_DEBUG << "TOC found - track 0 has " << trackFrames << " frames";

        if (calibrate(MAX_OFFSET) != 0)
        {
            PLOG_ERROR << "Calibrate failed";
            closeImage();
            return -1;
        }
        setOpen(true);
        return 0;
    }

    void selectSector(int sectorNum) override
    {
        char *buff = getBuffer();
        // calibrate()'s offset is where the data starts inside the raw sector (24 for MODE2, 16 for
        // MODE1); the .bin reader applies it to a byte stream, where one that spills into the next
        // sector is fine - here a unit is all there is
        uint32_t offset = static_cast<uint32_t>(getOffset() < 0 ? 0 : getOffset());
        if (offset + DATA_SIZE > unitBytes) offset = 0;
        if (sectorNum >= 0 && readUnit(static_cast<uint32_t>(sectorNum), unit.data()))
            memcpy(buff, unit.data() + offset, DATA_SIZE);
        else
            memset(buff, 0, DATA_SIZE);
        setSectorpos(0);
        setCurrentSector(sectorNum);
    }
};
#endif // ABLEEM_NO_CHD

} // namespace ableem
