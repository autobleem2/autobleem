//
// TarBuilder: a .tar or .tar.gz written from a few in-memory files, for the tests of TarArchive and of the
// installer - no tar tool needed on the test host. ustar headers, GNU 'L' entries for names over 100
// characters, gzip through miniz (raw deflate between a gzip header and its crc32/size trailer).
//
#pragma once

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <miniz.h>

namespace test_support {

class TarBuilder {
public:
    TarBuilder &file(const std::string &name, const std::string &data, unsigned mode = 0644) {
        add(name, data, '0', mode);
        return *this;
    }
    TarBuilder &dir(const std::string &name) {
        add(name.back() == '/' ? name : name + "/", "", '5', 0755);
        return *this;
    }
    TarBuilder &symlink(const std::string &name, const std::string &target) {
        add(name, "", '2', 0777, target);
        return *this;
    }

    // the archive's bytes, ending with the two zero blocks
    std::string tar() const { return bytes_ + std::string(1024, '\0'); }

    std::string targz() const {
        std::string plain = tar();
        mz_stream s;
        memset(&s, 0, sizeof(s));
        mz_deflateInit2(&s, MZ_DEFAULT_LEVEL, MZ_DEFLATED, -MZ_DEFAULT_WINDOW_BITS, 9, MZ_DEFAULT_STRATEGY);
        std::string out(mz_deflateBound(&s, static_cast<mz_ulong>(plain.size())), '\0');
        s.next_in = reinterpret_cast<const unsigned char *>(plain.data());
        s.avail_in = static_cast<unsigned>(plain.size());
        s.next_out = reinterpret_cast<unsigned char *>(&out[0]);
        s.avail_out = static_cast<unsigned>(out.size());
        mz_deflate(&s, MZ_FINISH);
        out.resize(s.total_out);
        mz_deflateEnd(&s);
        const unsigned char header[10] = {0x1f, 0x8b, 8, 0, 0, 0, 0, 0, 0, 3};
        std::string gz(reinterpret_cast<const char *>(header), 10);
        gz += out;
        uint32_t crc = static_cast<uint32_t>(
            mz_crc32(MZ_CRC32_INIT, reinterpret_cast<const unsigned char *>(plain.data()), plain.size()));
        uint32_t size = static_cast<uint32_t>(plain.size());
        for (uint32_t v : {crc, size})
            for (int i = 0; i < 4; i++)
                gz.push_back(static_cast<char>((v >> (8 * i)) & 0xff));
        return gz;
    }

    bool writeTarGz(const std::string &path) const {
        std::ofstream out(path, std::ios::binary);
        std::string data = targz();
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
        return static_cast<bool>(out);
    }
    bool writeTar(const std::string &path) const {
        std::ofstream out(path, std::ios::binary);
        std::string data = tar();
        out.write(data.data(), static_cast<std::streamsize>(data.size()));
        return static_cast<bool>(out);
    }

private:
    static std::string octal(uint64_t v, size_t width) {
        std::string s(width, '0');
        for (size_t i = width - 1; i-- > 0;) {
            s[i] = static_cast<char>('0' + (v & 7));
            v >>= 3;
        }
        s[width - 1] = '\0';
        return s;
    }
    void header(const std::string &name, uint64_t size, char type, unsigned mode, const std::string &link) {
        std::string h(512, '\0');
        memcpy(&h[0], name.data(), name.size() < 100 ? name.size() : 100);
        memcpy(&h[100], octal(mode, 8).data(), 8);
        memcpy(&h[108], octal(0, 8).data(), 8);
        memcpy(&h[116], octal(0, 8).data(), 8);
        memcpy(&h[124], octal(size, 12).data(), 12);
        memcpy(&h[136], octal(0, 12).data(), 12);
        memset(&h[148], ' ', 8);
        h[156] = type;
        memcpy(&h[157], link.data(), link.size() < 100 ? link.size() : 100);
        memcpy(&h[257],
               "ustar\0"
               "00",
               8);
        unsigned sum = 0;
        for (unsigned char c : h)
            sum += c;
        std::string chk = octal(sum, 7) + " ";
        memcpy(&h[148], chk.data(), 8);
        bytes_ += h;
    }
    void add(const std::string &name, const std::string &data, char type, unsigned mode, const std::string &link = "") {
        if (name.size() > 100) {
            header("././@LongLink", name.size() + 1, 'L', 0644, "");
            pad(name + '\0');
        }
        header(name, type == '0' ? data.size() : 0, type, mode, link);
        if (type == '0')
            pad(data);
    }
    void pad(const std::string &data) {
        bytes_ += data;
        size_t rest = (512 - data.size() % 512) % 512;
        bytes_ += std::string(rest, '\0');
    }
    std::string bytes_;
};

} // namespace test_support
