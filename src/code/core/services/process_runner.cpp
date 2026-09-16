//
// ForkProcessRunner: the production ProcessRunner.
//
#include "process_runner.h"
#include "../util.h"

//*******************************
// ForkProcessRunner::run
//*******************************
void ForkProcessRunner::run(const std::string &exe, const std::vector<std::string> &args) {
    Util::runAndWait(exe, args);
}
