// lib_ableem - engine: the log. plog (vendored, header-only) behind a two-line setup: Log::initConsoleOnly()
// first thing in main(), Log::addFile() once the environment knows where the logs directory is, then
// PLOG_INFO / PLOG_WARNING / PLOG_ERROR / PLOG_DEBUG anywhere. Every line goes to the console (stdout - what
// run.sh / autobleem-session tee into AB_out.txt) and to a rolling file, 1 MB x 3, that does not grow forever
// on a stick. A PLOG_* before any init is silently dropped, which is why the console comes first.
//
//   12:32:13 INFO  [scanGamesDirectory:371] Scanning: /media/Games
//
// From AutoBleem-NG's log.h. This is the only logging in the code base - no cout anywhere.
#pragma once

#include <iomanip>
#include <string>

#include <plog/Log.h>
#include <plog/Init.h>
#include <plog/Appenders/ConsoleAppender.h>
#include <plog/Appenders/RollingFileAppender.h>
#include <plog/Util.h>

namespace ableem {

//******************
// Log
//******************
namespace Log {

// "HH:MM:SS LEVEL [function:line] message" - no date (the PSC has no clock to trust anyway), no thread id
class Formatter {
public:
    static plog::util::nstring header() { return plog::util::nstring(); }

    static plog::util::nstring format(const plog::Record &record) {
        tm t;
        plog::util::localtime_s(&t, &record.getTime().time);

        plog::util::nostringstream ss;
        ss << std::setfill('0') << std::setw(2) << t.tm_hour << ":" << std::setfill('0') << std::setw(2) << t.tm_min
           << ":" << std::setfill('0') << std::setw(2) << t.tm_sec << " " << std::setfill(' ') << std::setw(5)
           << std::left << plog::severityToString(record.getSeverity()) << " [" << record.getFunc() << ":"
           << record.getLine() << "] " << record.getMessage() << "\n";
        return ss.str();
    }
};

#ifndef ABLEEM_LOG_LEVEL
#ifdef NDEBUG
#define ABLEEM_LOG_LEVEL plog::info
#else
#define ABLEEM_LOG_LEVEL plog::debug
#endif
#endif

// once, first thing: the console. The tests and the tools stop here.
inline void initConsoleOnly(plog::Severity maxSeverity = ABLEEM_LOG_LEVEL) {
    static plog::ConsoleAppender<Formatter> consoleAppender;
    plog::init(maxSeverity, &consoleAppender);
}

// once, after initConsoleOnly(): the rolling file as well. logFile's directory must exist.
inline void addFile(const std::string &logFile) {
    static plog::RollingFileAppender<Formatter> fileAppender(logFile.c_str(), 1024 * 1024, 3);
    if (plog::get() != nullptr)
        plog::get()->addAppender(&fileAppender);
}

// both at once
inline void init(const std::string &logFile, plog::Severity maxSeverity = ABLEEM_LOG_LEVEL) {
    initConsoleOnly(maxSeverity);
    addFile(logFile);
}

inline void setLevel(plog::Severity maxSeverity) {
    if (plog::get() != nullptr)
        plog::get()->setMaxSeverity(maxSeverity);
}

} // namespace Log

} // namespace ableem
