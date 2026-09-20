#include "ableem/engine/sha256.h"

#include <fstream>

#include <cstring>

namespace ableem {

namespace {

const uint32_t K[64] = {0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
                        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
                        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
                        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
                        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
                        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
                        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
                        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

inline uint32_t rotr(uint32_t x, uint32_t c) {
    return (x >> c) | (x << (32 - c));
}

} // namespace

//*******************************
// Sha256::Sha256
//*******************************
Sha256::Sha256() {
    state[0] = 0x6a09e667;
    state[1] = 0xbb67ae85;
    state[2] = 0x3c6ef372;
    state[3] = 0xa54ff53a;
    state[4] = 0x510e527f;
    state[5] = 0x9b05688c;
    state[6] = 0x1f83d9ab;
    state[7] = 0x5be0cd19;
}

//*******************************
// Sha256::transform
//*******************************
void Sha256::transform(const unsigned char block[64]) {
    uint32_t w[64];
    for (int i = 0; i < 16; i++) {
        w[i] = (static_cast<uint32_t>(block[i * 4]) << 24) | (static_cast<uint32_t>(block[i * 4 + 1]) << 16) |
               (static_cast<uint32_t>(block[i * 4 + 2]) << 8) | static_cast<uint32_t>(block[i * 4 + 3]);
    }
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }

    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    uint32_t e = state[4], f = state[5], g = state[6], h = state[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + K[i] + w[i];
        uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
        uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + maj;
        h = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = b;
        b = a;
        a = t1 + t2;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
    state[4] += e;
    state[5] += f;
    state[6] += g;
    state[7] += h;
}

//*******************************
// Sha256::update
//*******************************
void Sha256::update(const unsigned char *data, size_t length) {
    bitCount += static_cast<uint64_t>(length) * 8;
    size_t i = 0;
    if (bufferLen > 0) {
        size_t take = 64 - bufferLen;
        if (take > length)
            take = length;
        memcpy(buffer + bufferLen, data, take);
        bufferLen += take;
        i = take;
        if (bufferLen == 64) {
            transform(buffer);
            bufferLen = 0;
        }
    }
    for (; i + 64 <= length; i += 64)
        transform(data + i);
    if (i < length) {
        memcpy(buffer, data + i, length - i);
        bufferLen = length - i;
    }
}

//*******************************
// Sha256::hexDigest
//*******************************
std::string Sha256::hexDigest() {
    uint64_t bits = bitCount;
    unsigned char pad = 0x80;
    update(&pad, 1);
    unsigned char zero = 0;
    while (bufferLen != 56)
        update(&zero, 1);
    unsigned char lengthBytes[8];
    for (int i = 7; i >= 0; i--) {
        lengthBytes[i] = static_cast<unsigned char>(bits & 0xff);
        bits >>= 8;
    }
    update(lengthBytes, 8);

    static const char *hex = "0123456789abcdef";
    std::string out;
    out.reserve(64);
    for (uint32_t v : state) {
        for (int shift = 28; shift >= 0; shift -= 4)
            out.push_back(hex[(v >> shift) & 0xf]);
    }
    return out;
}

//*******************************
// Sha256::ofBytes / ofString / ofFile
//*******************************
std::string Sha256::ofBytes(const unsigned char *data, size_t length) {
    Sha256 sha;
    sha.update(data, length);
    return sha.hexDigest();
}

std::string Sha256::ofString(const std::string &text) {
    return ofBytes(reinterpret_cast<const unsigned char *>(text.data()), text.size());
}

std::string Sha256::ofFile(const std::string &path) {
    std::ifstream in(path, std::ios::binary);
    if (!in)
        return "";
    Sha256 sha;
    char buffer[64 * 1024];
    while (in.read(buffer, sizeof(buffer)) || in.gcount() > 0)
        sha.update(reinterpret_cast<const unsigned char *>(buffer), static_cast<size_t>(in.gcount()));
    return sha.hexDigest();
}

} // namespace ableem
