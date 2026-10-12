#include "ra_job_service.h"

#include "core/services/environment.h"
#include "core/services/retroarch_version.h"

#include <ableem/engine/filesystem.h>
#include <ableem/engine/log.h>

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <fstream>

#ifndef _WIN32
#include <dirent.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

using namespace std;

// Decided by the appliance job (B2); until then this one constant is the only place that names it.
const char *RaJobService::ApplianceRunner = "/usr/local/bin/autobleem-retroarch";

namespace {
// where an appliance keeps the version of the RetroArch it laid (App::applyUpdateSetting reads the same stamp)
const char *const ApplianceStamp = "/usr/local/share/autobleem/retroarch.version";

rajob::RunnerEnv hostRunnerEnv() {
    rajob::RunnerEnv env;
    const char *override = getenv("AB_RA_RUNNER");
    if (override != nullptr)
        env.runnerOverride = override;
#if defined(AB_PLATFORM_PSC)
    env.psc = true;
    env.root = Env::getPathToUSBRoot();
    env.abupdate = Env::getWorkingPath() + ableem::sep + "abupdate"; // beside abfetch (%r), the launcher's own folder
#elif defined(AB_APPLIANCE)
    env.appliance = true;
    env.applianceRunner = RaJobService::ApplianceRunner;
#endif
    return env;
}

#ifndef _WIN32
// bytes under `path` (files only; a link counts as itself), recursively
uint64_t treeSize(const string &path, int depth = 0) {
    struct stat st;
    if (lstat(path.c_str(), &st) != 0)
        return 0;
    if (!S_ISDIR(st.st_mode))
        return static_cast<uint64_t>(st.st_size);
    if (depth > 16)
        return 0;
    uint64_t total = 0;
    DIR *dir = opendir(path.c_str());
    if (dir == nullptr)
        return 0;
    while (const dirent *entry = readdir(dir)) {
        const string name = entry->d_name;
        if (name != "." && name != "..")
            total += treeSize(path + "/" + name, depth + 1);
    }
    closedir(dir);
    return total;
}
#endif
} // namespace

//*******************************
// RaJobService::~RaJobService
//*******************************
RaJobService::~RaJobService() {
#ifndef _WIN32
    if (pid_ > 0)
        kill(pid_, SIGTERM); // the runner stops at its atomic step; init reaps it
#endif
}

//*******************************
// RaJobService::available / raRunnerCommand / progressFile
//*******************************
bool RaJobService::available() {
    return !rajob::runnerCommand(rajob::Op::Install, false, "x", hostRunnerEnv()).empty();
}

vector<string> RaJobService::raRunnerCommand(rajob::Op op, bool withBios, const string &progressFile) {
    return rajob::runnerCommand(op, withBios, progressFile, hostRunnerEnv());
}

string RaJobService::progressFile() {
#if defined(AB_PLATFORM_PSC)
    return Env::getPathToSystemDir() + ableem::sep + "Install" + ableem::sep + "ra-job.progress";
#else
    return "/tmp/autobleem-ra-job.progress";
#endif
}

//*******************************
// RaJobService::start / startCommand
//*******************************
bool RaJobService::start(rajob::Op op, bool withBios) {
    const string file = progressFile();
    return startCommand(raRunnerCommand(op, withBios, file), file);
}

bool RaJobService::startCommand(const vector<string> &argv, const string &progressFile) {
    if (running())
        return false;
    job_.begin();
    file_ = progressFile;
    signature_.clear();
#ifdef _WIN32
    job_.state = rajob::State::Error;
    job_.code = "other";
    return false;
#else
    if (argv.empty()) {
        job_.state = rajob::State::Error;
        job_.code = "other";
        return false;
    }
    const size_t slash = progressFile.rfind('/');
    if (slash != string::npos)
        ableem::DirEntry::createDirs(progressFile.substr(0, slash));
    // an old run's last line must not read as this run's; the .log stays (it is appended)
    unlink(progressFile.c_str());
    unlink((progressFile + ".tmp").c_str());

    vector<char *> args;
    for (const string &a : argv)
        args.push_back(const_cast<char *>(a.c_str()));
    args.push_back(nullptr);
    const pid_t pid = fork();
    if (pid < 0) {
        PLOG_ERROR << "RetroArch job: fork failed: " << strerror(errno);
        job_.state = rajob::State::Error;
        job_.code = "other";
        return false;
    }
    if (pid == 0) {
        const int devnull = open("/dev/null", O_RDWR);
        if (devnull >= 0) { // the runner talks through the progress file and its .log, not the launcher's console
            dup2(devnull, 0);
            dup2(devnull, 1);
            dup2(devnull, 2);
        }
        execvp(args[0], args.data());
        _exit(127);
    }
    pid_ = pid;
    PLOG_INFO << "RetroArch job: started " << argv[0] << " (pid " << pid << "), progress " << progressFile;
    return true;
#endif
}

//*******************************
// RaJobService::stop
//*******************************
void RaJobService::stop() {
#ifndef _WIN32
    if (running() && pid_ > 0 && !job_.stopAsked) {
        job_.stopAsked = true;
        kill(pid_, SIGTERM);
    }
#endif
}

//*******************************
// RaJobService::poll
//*******************************
const RaJobService::Status &RaJobService::poll() {
    if (!running())
        return job_;
    readFile(false);
    reap(false);
    return job_;
}

// one stat per frame; the line is read only when the file is another (the runner renames a new one over it)
void RaJobService::readFile(bool force) {
#ifndef _WIN32
    struct stat st;
    if (stat(file_.c_str(), &st) != 0)
        return; // not there yet
    char sig[96];
#ifdef __linux__
    const long nsec = st.st_mtim.tv_nsec;
#else
    const long nsec = 0;
#endif
    snprintf(sig, sizeof(sig), "%lld.%ld/%llu/%lld", static_cast<long long>(st.st_mtime), nsec,
             static_cast<unsigned long long>(st.st_ino), static_cast<long long>(st.st_size));
    if (!force && signature_ == sig)
        return;
    signature_ = sig;
    ifstream in(file_, ios::binary);
    char buf[1024];
    in.read(buf, sizeof(buf));
    job_.apply(string(buf, static_cast<size_t>(in.gcount())));
#else
    (void)force;
#endif
}

void RaJobService::reap(bool wait) {
#ifndef _WIN32
    if (pid_ <= 0)
        return;
    int status = 0;
    const pid_t done = waitpid(pid_, &status, wait ? 0 : WNOHANG);
    if (done != pid_)
        return;
    pid_ = -1;
    readFile(true); // the last line, written just before the exit
    job_.childExited(WIFEXITED(status) ? WEXITSTATUS(status) : -1);
    PLOG_INFO << "RetroArch job: ended, state " << static_cast<int>(job_.state) << " code " << job_.code;
#else
    (void)wait;
#endif
}

//*******************************
// RaJobService::installedVersion / sizeOnDisk
//*******************************
string RaJobService::installedVersion() {
    // the console's stamp next to the binary (and a stick laid by the PC installer), else the appliance's
    string version = retroarch_version::installed(Env::getPathToUSBRoot());
    if (version.empty()) {
        ifstream stamp(ApplianceStamp);
        string line;
        if (stamp && getline(stamp, line))
            version = retroarch_version::parse(line);
        if (version.empty() && stamp)
            version = line;
    }
    if (version.empty()) {
        const char *dev = getenv("AB_UPDATE_RETROARCH_VERSION"); // a dev host's pretend stamp
        if (dev != nullptr)
            version = dev;
    }
    return version;
}

// what Remove would delete (the binary and the folders named in the contract), not the user's roms and saves
uint64_t RaJobService::sizeOnDisk() {
#ifdef _WIN32
    return 0;
#else
    uint64_t total = 0;
    for (const string &binary : Env::retroArchBinaries())
        total += treeSize(binary);
    const string dir = Env::getPathToRetroarchDir();
    for (const char *name : {"cores", "info", "assets", "database", "cheats", "overlays", "shaders", "downloads",
                             "records", "Retroarch themes"})
        total += treeSize(dir + ableem::sep + name);
    return total;
#endif
}
