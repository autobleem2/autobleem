#ifndef ABPAD_SHM_BLOCK_H
#define ABPAD_SHM_BLOCK_H

// The shared memory the daemon writes and every preloaded app reads, as a file both sides mmap.
//
// A file on tmpfs rather than shm_open: /tmp is tmpfs on the console, the Pi and the PC stick alike,
// it needs no librt, and the block can be looked at with od(1) when something is wrong. Windows gets
// a named mapping instead so that `abpadd --probe` runs on the dev host - there is no preload there
// to read it, but being able to ask SDL what it makes of a pad without a console is worth the dozen
// lines.

#include <cstddef>
#include <string>

namespace abpad {

//*******************************
// ShmBlock
//*******************************
class ShmBlock {
public:
    ShmBlock() = default;
    ~ShmBlock();
    ShmBlock(const ShmBlock &) = delete;
    ShmBlock &operator=(const ShmBlock &) = delete;

    // the writer's end: creates the file at `size` and takes an exclusive lock on it, so a second
    // daemon for the same block refuses to start rather than fighting the first
    bool create(const std::string &path, size_t size);
    // the reader's end: maps what is there, read-only, and fails quietly when it is not there yet
    bool openReadOnly(const std::string &path, size_t size);
    void close();

    void *data() const { return data_; }
    const std::string &error() const { return error_; }

private:
    void *data_ = nullptr;
    size_t size_ = 0;
    int fd_ = -1;
    void *handle_ = nullptr; // Windows: the mapping object
    std::string error_;
};

// where the block lives unless AB_PAD_SHM says otherwise
std::string defaultShmPath();

} // namespace abpad

#endif
