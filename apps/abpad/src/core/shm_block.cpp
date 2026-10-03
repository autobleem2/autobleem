#include "core/shm_block.h"

#include <cstdlib>
#include <cstring>

#ifdef _WIN32
#include <windows.h>
#else
#include <fcntl.h>
#include <sys/file.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

using namespace std;

namespace abpad {

//*******************************
// defaultShmPath
//*******************************
string defaultShmPath() {
    const char *fromEnvironment = getenv("AB_PAD_SHM");
    if (fromEnvironment && *fromEnvironment) {
        return fromEnvironment;
    }
#ifdef _WIN32
    return "AbPadState";
#else
    return "/tmp/abpad.state";
#endif
}

ShmBlock::~ShmBlock() {
    close();
}

#ifdef _WIN32

//*******************************
// ShmBlock::create / openReadOnly / close (Windows)
//*******************************
bool ShmBlock::create(const string &path, size_t size) {
    close();
    HANDLE mapping =
        CreateFileMappingA(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(size), path.c_str());
    if (!mapping) {
        error_ = "CreateFileMapping failed";
        return false;
    }
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mapping);
        error_ = "another abpadd already has " + path;
        return false;
    }
    data_ = MapViewOfFile(mapping, FILE_MAP_ALL_ACCESS, 0, 0, size);
    if (!data_) {
        CloseHandle(mapping);
        error_ = "MapViewOfFile failed";
        return false;
    }
    handle_ = mapping;
    size_ = size;
    return true;
}

bool ShmBlock::openReadOnly(const string &path, size_t size) {
    close();
    HANDLE mapping = OpenFileMappingA(FILE_MAP_READ, FALSE, path.c_str());
    if (!mapping) {
        error_ = "no abpadd is running";
        return false;
    }
    data_ = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, size);
    if (!data_) {
        CloseHandle(mapping);
        error_ = "MapViewOfFile failed";
        return false;
    }
    handle_ = mapping;
    size_ = size;
    return true;
}

void ShmBlock::close() {
    if (data_) {
        UnmapViewOfFile(data_);
        data_ = nullptr;
    }
    if (handle_) {
        CloseHandle(static_cast<HANDLE>(handle_));
        handle_ = nullptr;
    }
    size_ = 0;
}

#else

//*******************************
// ShmBlock::create (POSIX)
//*******************************
bool ShmBlock::create(const string &path, size_t size) {
    close();
    int fd = ::open(path.c_str(), O_RDWR | O_CREAT, 0666);
    if (fd < 0) {
        error_ = "cannot create " + path;
        return false;
    }
    // one daemon per block: a second one is a mistake, and two writers would fight over the seqlock
    if (flock(fd, LOCK_EX | LOCK_NB) != 0) {
        ::close(fd);
        error_ = "another abpadd already has " + path;
        return false;
    }
    if (ftruncate(fd, static_cast<off_t>(size)) != 0) {
        ::close(fd);
        error_ = "cannot size " + path;
        return false;
    }
    void *mapped = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (mapped == MAP_FAILED) {
        ::close(fd);
        error_ = "cannot map " + path;
        return false;
    }
    // everyone may read it: an App runs as whoever the launcher runs as, which need not be us
    fchmod(fd, 0666);
    data_ = mapped;
    size_ = size;
    fd_ = fd;
    return true;
}

//*******************************
// ShmBlock::openReadOnly (POSIX)
//*******************************
bool ShmBlock::openReadOnly(const string &path, size_t size) {
    close();
    int fd = ::open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        error_ = "no abpadd is running";
        return false;
    }
    struct stat facts;
    if (fstat(fd, &facts) != 0 || static_cast<size_t>(facts.st_size) < size) {
        ::close(fd);
        error_ = "the block is not the size this build expects";
        return false;
    }
    void *mapped = mmap(nullptr, size, PROT_READ, MAP_SHARED, fd, 0);
    ::close(fd); // the mapping keeps the file alive; the shim holds no descriptor of its own
    if (mapped == MAP_FAILED) {
        error_ = "cannot map " + path;
        return false;
    }
    data_ = mapped;
    size_ = size;
    return true;
}

void ShmBlock::close() {
    if (data_) {
        munmap(data_, size_);
        data_ = nullptr;
    }
    if (fd_ >= 0) {
        ::close(fd_); // which drops the flock
        fd_ = -1;
    }
    size_ = 0;
}

#endif

} // namespace abpad
