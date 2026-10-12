//
// RaJobService: the launcher's RetroArch install / update / remove (PLATFORM-24). It starts the runner (ra_job.h: the
// contract and the command line) as a CHILD process and follows its progress file; it never does the work itself.
//
// UpdateService's shape, minus the thread: a job is a child that works on its own, so there is nothing for a worker
// to wait on - poll(), once a frame on the main thread, reaps the child (waitpid WNOHANG) and, when the progress
// file changed (mtime/inode/size), reads its one line. stop() sends SIGTERM; the runner finishes its atomic step,
// keeps its .part files and ends with end|stopped (exit 3), and a second install resumes.
//
#pragma once

#include "ra_job.h"

#include <string>
#include <sys/types.h>
#include <vector>

class RaJobService {
public:
    using Status = rajob::JobState;

    RaJobService() = default;
    RaJobService(const RaJobService &) = delete;
    RaJobService &operator=(const RaJobService &) = delete;
    ~RaJobService(); // a job still running is stopped (SIGTERM) and reaped

    // whether this host has a runner at all (a console, an appliance, or AB_RA_RUNNER set)
    static bool available();
    // the runner's argv for an operation - the one place the command line comes from. (Env::raRunnerCommand of the
    // plan: Env lives in the engine library, the launcher keeps it here.)
    static std::vector<std::string> raRunnerCommand(rajob::Op op, bool withBios, const std::string &progressFile);
    // where the progress file lives: System/Install/ on the console, /tmp elsewhere
    static std::string progressFile();

    // the appliance's runner until its job fixes the path: one constant, one place
    static const char *ApplianceRunner;

    // start the job on this host's runner; false (and the status Error "other") when there is none or it will not fork
    bool start(rajob::Op op, bool withBios);
    // the same with an explicit argv (tests, and start() itself)
    bool startCommand(const std::vector<std::string> &argv, const std::string &progressFile);

    void stop();          // SIGTERM to a running job
    const Status &poll(); // main thread, once a frame
    const Status &status() const { return job_; }
    bool running() const { return job_.state == rajob::State::Running; }
    void reset() { job_ = Status(); } // an outcome was shown: idle again (never while running)

    // what is installed: the VERSION stamp's text ("" = none readable), and the space it takes (bytes)
    static std::string installedVersion();
    static uint64_t sizeOnDisk();

private:
    void readFile(bool force);
    void reap(bool wait);

    Status job_;
    pid_t pid_ = -1;
    std::string file_;
    std::string signature_; // mtime/inode/size of the file when last read
};
