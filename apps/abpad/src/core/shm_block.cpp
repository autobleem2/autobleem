#include "core/shm_block.h"

#include <cerrno>
#include <cstdio>
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

bool replaceFile(const string &path, const string &text, int) {
    string temporary = path + ".new";
    FILE *file = fopen(temporary.c_str(), "wb");
    if (!file) {
        return false;
    }
    bool ok = fwrite(text.data(), 1, text.size(), file) == text.size();
    ok = (fclose(file) == 0) && ok;
    remove(path.c_str()); // Windows' rename does not replace
    if (!ok || rename(temporary.c_str(), path.c_str()) != 0) {
        remove(temporary.c_str());
        return false;
    }
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
// A block left in the sticky /tmp by another user (or with a mode we may not write): Debian's fs.protected_regular
// refuses even root an O_CREAT open of it (EACCES), so an existing file is opened without O_CREAT and only a missing
// one is created, with O_EXCL. A file we still may not open is replaced - unless a daemon holds its lock.
static int openBlockFile(const string &path) {
    for (int attempt = 0; attempt < 3; ++attempt) {
        int fd = ::open(path.c_str(), O_RDWR);
        if (fd >= 0) {
            return fd;
        }
        if (errno == ENOENT) {
            fd = ::open(path.c_str(), O_RDWR | O_CREAT | O_EXCL | O_NOFOLLOW, 0666);
            if (fd >= 0 || errno != EEXIST) {
                return fd;
            }
            continue; // somebody made it between the two calls
        }
        if (errno != EACCES) {
            return -1;
        }
        int probe = ::open(path.c_str(), O_RDONLY);
        if (probe < 0) {
            if (errno == ENOENT) {
                continue;
            }
            return -1;
        }
        bool held = flock(probe, LOCK_EX | LOCK_NB) != 0;
        ::close(probe);
        if (held) {
            return -1; // a live daemon's block: not ours to take away
        }
        unlink(path.c_str());
    }
    return -1;
}

bool ShmBlock::create(const string &path, size_t size) {
    close();
    int fd = openBlockFile(path);
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

//*******************************
// replaceFile (POSIX)
//*******************************
bool replaceFile(const string &path, const string &text, int mode) {
    string temporary = path + ".new";
    unlink(temporary.c_str()); // a stale one from an earlier run, whoever owned it: root may not open it for writing
    int fd = ::open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, mode);
    if (fd < 0) {
        return false;
    }
    size_t done = 0;
    while (done < text.size()) {
        ssize_t n = ::write(fd, text.data() + done, text.size() - done);
        if (n <= 0) {
            ::close(fd);
            unlink(temporary.c_str());
            return false;
        }
        done += static_cast<size_t>(n);
    }
    fchmod(fd, static_cast<mode_t>(mode)); // the umask must not narrow it: an App may not be us
    ::close(fd);
    if (rename(temporary.c_str(), path.c_str()) != 0) {
        unlink(temporary.c_str());
        return false;
    }
    return true;
}

void ShmBlock::close() {
    if (data_) {
        munmap(data_, size_);
        data_ = nullptr;
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
