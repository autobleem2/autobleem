// 64-bit file offsets on 32-bit Linux (the console, the Pi): a partition image is bigger than 2 GB
#ifndef _WIN32
#define _FILE_OFFSET_BITS 64
#endif

#include "ableem/engine/zip_writer.h"

#include <miniz.h>
#include <ableem/engine/log.h>

#include <cstdio>

namespace ableem {

struct ZipWriter::Impl {
    mz_zip_archive zip{};
    bool open = false;
    std::string path;
};

namespace {
// miniz's streaming read callback: `opaque` is the FILE* of the source
size_t readChunk(void *opaque, mz_uint64 offset, void *buffer, size_t size) {
    FILE *file = static_cast<FILE *>(opaque);
    (void)offset; // the file is read in order; miniz asks for consecutive offsets
    return fread(buffer, 1, size, file);
}
} // namespace

ZipWriter::ZipWriter() : impl(new Impl) {}

ZipWriter::~ZipWriter() {
    if (impl->open)
        mz_zip_writer_end(&impl->zip);
}

bool ZipWriter::open(const std::string &zipPath) {
    if (impl->open)
        close();
    mz_zip_zero_struct(&impl->zip);
    if (!mz_zip_writer_init_file(&impl->zip, zipPath.c_str(), 0)) {
        PLOG_WARNING << "Cannot create " << zipPath << ": "
                     << mz_zip_get_error_string(mz_zip_get_last_error(&impl->zip));
        return false;
    }
    impl->open = true;
    impl->path = zipPath;
    return true;
}

bool ZipWriter::addFile(const std::string &path, const std::string &name) {
    if (!impl->open)
        return false;
    FILE *file = fopen(path.c_str(), "rb");
    if (!file) {
        PLOG_WARNING << "Cannot read " << path;
        return false;
    }
    // miniz wants the size up front (it is what decides the plain-zip or zip64 layout - the console's
    // recovery expects the plain one, and a partition fits it): a regular file's from its end, a block
    // device's the same way, it seeks
    mz_uint64 size = 0;
#ifdef _WIN32
    if (_fseeki64(file, 0, SEEK_END) == 0) {
        long long end = _ftelli64(file);
        if (end > 0)
            size = static_cast<mz_uint64>(end);
        _fseeki64(file, 0, SEEK_SET);
    }
#else
    if (fseeko(file, 0, SEEK_END) == 0) {
        off_t end = ftello(file);
        if (end > 0)
            size = static_cast<mz_uint64>(end);
        fseeko(file, 0, SEEK_SET);
    }
#endif
    if (size == 0) {
        PLOG_WARNING << "Size of " << path << " unknown - it goes into " << impl->path << " empty";
    }
    bool ok = mz_zip_writer_add_read_buf_callback(&impl->zip, name.c_str(), readChunk, file, size, nullptr, nullptr, 0,
                                                  MZ_DEFAULT_LEVEL, nullptr, 0, nullptr, 0) != 0;
    if (!ok) {
        PLOG_WARNING << "Cannot add " << path << " to " << impl->path << ": "
                     << mz_zip_get_error_string(mz_zip_get_last_error(&impl->zip));
    }
    fclose(file);
    return ok;
}

bool ZipWriter::addBytes(const std::string &name, const std::string &bytes) {
    if (!impl->open)
        return false;
    bool ok = mz_zip_writer_add_mem(&impl->zip, name.c_str(), bytes.data(), bytes.size(), MZ_DEFAULT_LEVEL) != 0;
    if (!ok) {
        PLOG_WARNING << "Cannot add " << name << " to " << impl->path << ": "
                     << mz_zip_get_error_string(mz_zip_get_last_error(&impl->zip));
    }
    return ok;
}

bool ZipWriter::close() {
    if (!impl->open)
        return false;
    bool ok = mz_zip_writer_finalize_archive(&impl->zip) != 0;
    if (!ok) {
        PLOG_WARNING << "Cannot finish " << impl->path << ": "
                     << mz_zip_get_error_string(mz_zip_get_last_error(&impl->zip));
    }
    mz_zip_writer_end(&impl->zip);
    impl->open = false;
    return ok;
}

bool ZipWriter::isOpen() const {
    return impl->open;
}

} // namespace ableem
