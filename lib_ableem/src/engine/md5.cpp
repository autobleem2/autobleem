#include "md5.h"

#include <cstring>

namespace ableem {

namespace {

const uint32_t K[64] = {0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501,
                        0x698098d8, 0x8b44f7af, 0xffff5bb1, 0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821,
                        0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453, 0xd8a1e681, 0xe7d3fbc8,
                        0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a,
                        0xfffa3942, 0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70,
                        0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05, 0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665,
                        0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d, 0x85845dd1,
                        0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};

const uint32_t R[64] = {7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 7,  12, 17, 22, 5,  9,  14, 20, 5,  9,
                        14, 20, 5,  9,  14, 20, 5,  9,  14, 20, 4,  11, 16, 23, 4,  11, 16, 23, 4,  11, 16, 23,
                        4,  11, 16, 23, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21, 6,  10, 15, 21};

inline uint32_t rotl(uint32_t x, uint32_t c) {
    return (x << c) | (x >> (32 - c));
}

} // namespace

//*******************************
// Md5::Md5
//*******************************
Md5::Md5() {
    state[0] = 0x67452301;
    state[1] = 0xefcdab89;
    state[2] = 0x98badcfe;
    state[3] = 0x10325476;
}

//*******************************
// Md5::transform
//*******************************
void Md5::transform(const unsigned char block[64]) {
    uint32_t M[16];
    for (int i = 0; i < 16; i++) {
        M[i] = (uint32_t)block[i * 4] | ((uint32_t)block[i * 4 + 1] << 8) | ((uint32_t)block[i * 4 + 2] << 16) |
               ((uint32_t)block[i * 4 + 3] << 24);
    }
    uint32_t a = state[0], b = state[1], c = state[2], d = state[3];
    for (uint32_t i = 0; i < 64; i++) {
        uint32_t f, g;
        if (i < 16) {
            f = (b & c) | (~b & d);
            g = i;
        } else if (i < 32) {
            f = (d & b) | (~d & c);
            g = (5 * i + 1) % 16;
        } else if (i < 48) {
            f = b ^ c ^ d;
            g = (3 * i + 5) % 16;
        } else {
            f = c ^ (b | ~d);
            g = (7 * i) % 16;
        }
        uint32_t temp = d;
        d = c;
        c = b;
        b = b + rotl(a + f + K[i] + M[g], R[i]);
        a = temp;
    }
    state[0] += a;
    state[1] += b;
    state[2] += c;
    state[3] += d;
}

//*******************************
// Md5::update
//*******************************
void Md5::update(const unsigned char *data, size_t length) {
    bitCount += (uint64_t)length * 8;
    while (length > 0) {
        size_t take = 64 - bufferLen;
        if (take > length)
            take = length;
        memcpy(buffer + bufferLen, data, take);
        bufferLen += take;
        data += take;
        length -= take;
        if (bufferLen == 64) {
            transform(buffer);
            bufferLen = 0;
        }
    }
}

//*******************************
// Md5::hexDigest
//*******************************
std::string Md5::hexDigest() {
    uint64_t bits = bitCount; // the message length before the padding below is added
    unsigned char pad = 0x80;
    update(&pad, 1);
    unsigned char zero = 0;
    while (bufferLen != 56) {
        update(&zero, 1);
    }
    unsigned char lengthBytes[8];
    for (int i = 0; i < 8; i++) {
        lengthBytes[i] = (unsigned char)(bits >> (8 * i));
    }
    update(lengthBytes, 8);

    static const char hex[] = "0123456789abcdef";
    std::string out;
    for (int i = 0; i < 4; i++) {
        for (int j = 0; j < 4; j++) {
            unsigned char byte = (unsigned char)(state[i] >> (8 * j));
            out += hex[byte >> 4];
            out += hex[byte & 0x0f];
        }
    }
    return out;
}

//*******************************
// Md5::ofBytes
//*******************************
std::string Md5::ofBytes(const unsigned char *data, size_t length) {
    Md5 md5;
    md5.update(data, length);
    return md5.hexDigest();
}

} // namespace ableem
