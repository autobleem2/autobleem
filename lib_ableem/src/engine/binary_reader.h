// lib_ableem - engine (private): little-endian primitives over an ifstream, for PBP/SFO headers.
#pragma once

#include <fstream>
#include <string>

namespace ableem {

inline unsigned char readByte(std::ifstream &stream) {
    unsigned char c = 0;
    stream.read((char *)&c, 1);
    return c;
}

inline unsigned long readUint32LE(std::ifstream &stream) {
    unsigned long res = 0;
    res += (unsigned long)readByte(stream);
    res += (unsigned long)readByte(stream) << 8;
    res += (unsigned long)readByte(stream) << 16;
    res += (unsigned long)readByte(stream) << 24;
    return res;
}

// reads up to the next NUL (the NUL is consumed)
inline std::string readCString(std::ifstream &stream) {
    std::string str;
    char c = readByte(stream);
    while (!stream.eof() && !stream.fail() && c != 0) {
        str = str + c;
        c = readByte(stream);
    }
    return str;
}

// skips NUL padding, leaving the stream on the first non-NUL byte
inline void skipZeros(std::ifstream &stream) {
    char c = readByte(stream);
    while (!stream.eof() && !stream.fail() && c == 0) {
        c = readByte(stream);
    }
    stream.seekg(-1, std::ios::cur);
}

} // namespace ableem
