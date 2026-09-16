//
// ForkProcessRunner: the production ProcessRunner.
//
#include "process_runner.h"
#include "system.h"

//*******************************
// ForkProcessRunner::run
//*******************************
void ForkProcessRunner::run(const std::string &exe, const std::vector<std::string> &args) {
    System::runAndWait(exe, args);
}
