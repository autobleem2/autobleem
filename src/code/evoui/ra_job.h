//
// ra_job.h (evoui/, pure): the launcher's side of the RetroArch install/remove contract (PLATFORM-24).
//
// The launcher starts a RUNNER as a child process (abupdate <root> --retroarch ... on the console, autobleem-retroarch
// on a Pi / PC stick) with  install|remove [--bios] --progress <FILE>.  The runner rewrites <FILE> atomically, one
// UTF-8 line:
//
//     phase <i>/<n>|<key>|<done>|<total>        a phase of n running (total 0 = unknown)
//     end|ok   end|stopped   end|error|<code>   the last content, written once
//
// and exits 0 / 3 / 1. These are the parts that need no process or screen: the line's parser, the job's status
// machine, the runner's command line, the keys' and codes' mapping and the panel's row logic - so they can be tested.
//
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace rajob {

//******************
// Op
//******************
// Update is Install (the runner re-lays everything, a newer remote version replaces the old one); it is a separate
// menu row only
enum class Op { Install, Update, Remove };

//******************
// Progress / parseLine
//******************
struct Progress {
    int phase = 0;  // 1-based; 0 = no phase line seen yet
    int phases = 0; // n, fixed for the run
    std::string key;
    uint64_t done = 0;
    uint64_t total = 0; // 0 = unknown
    bool operator==(const Progress &o) const {
        return phase == o.phase && phases == o.phases && key == o.key && done == o.done && total == o.total;
    }
};

enum class EndKind { None, Ok, Stopped, Error };

struct Line {
    enum class Kind { Garbled, Phase, End };
    Kind kind = Kind::Garbled;
    Progress progress; // Phase
    EndKind end = EndKind::None;
    std::string code; // End/Error: no-network, no-space, download, checksum, apt-provided, other
};

namespace detail {
inline std::vector<std::string> split(const std::string &s, char sep) {
    std::vector<std::string> parts(1);
    for (char c : s) {
        if (c == sep)
            parts.emplace_back();
        else
            parts.back() += c;
    }
    return parts;
}
// digits only, at most 18 of them: no sign, no blanks, no overflow
inline bool number(const std::string &s, uint64_t &out) {
    if (s.empty() || s.size() > 18)
        return false;
    uint64_t v = 0;
    for (char c : s) {
        if (c < '0' || c > '9')
            return false;
        v = v * 10 + static_cast<uint64_t>(c - '0');
    }
    out = v;
    return true;
}
} // namespace detail

// the first line of the file's text (a trailing CR/LF is fine, further lines are ignored); Kind::Garbled for anything
// that is not exactly the contract's grammar
inline Line parseLine(const std::string &text) {
    Line line;
    std::string first = text.substr(0, text.find('\n'));
    while (!first.empty() && (first.back() == '\r' || first.back() == ' '))
        first.pop_back();
    const std::vector<std::string> f = detail::split(first, '|');
    if (f[0] == "end") {
        if (f.size() == 2 && f[1] == "ok") {
            line.kind = Line::Kind::End;
            line.end = EndKind::Ok;
        } else if (f.size() == 2 && f[1] == "stopped") {
            line.kind = Line::Kind::End;
            line.end = EndKind::Stopped;
        } else if (f.size() == 3 && f[1] == "error" && !f[2].empty()) {
            line.kind = Line::Kind::End;
            line.end = EndKind::Error;
            line.code = f[2];
        }
        return line;
    }
    if (f.size() != 4 || f[0].compare(0, 6, "phase ") != 0 || f[1].empty())
        return line;
    const std::vector<std::string> of = detail::split(f[0].substr(6), '/');
    uint64_t i = 0, n = 0, done = 0, total = 0;
    if (of.size() != 2 || !detail::number(of[0], i) || !detail::number(of[1], n) || i < 1 || n < i || n > 99 ||
        !detail::number(f[2], done) || !detail::number(f[3], total))
        return line;
    line.kind = Line::Kind::Phase;
    line.progress.phase = static_cast<int>(i);
    line.progress.phases = static_cast<int>(n);
    line.progress.key = f[1];
    line.progress.done = done;
    line.progress.total = total;
    return line;
}

// a code outside the contract's list is "other"
inline std::string knownCode(const std::string &code) {
    for (const char *c : {"no-network", "no-space", "download", "checksum", "apt-provided", "other"})
        if (code == c)
            return code;
    return "other";
}

//******************
// Status / JobState
//******************
enum class State { Idle, Running, Ok, Stopped, Error };

// What a job is, from the file and from the child: feed it the file's text whenever the file changed (apply) and
// the child's end once (childExited). The file's `end` line decides the outcome; a runner that left without one is
// judged by how it left.
struct JobState {
    State state = State::Idle;
    Progress progress;
    std::string code;        // Error: the contract's code
    bool stopAsked = false;  // stop() sent SIGTERM
    Line ended;              // the file's end line, once seen

    void begin() { *this = JobState(); state = State::Running; }

    void apply(const std::string &fileText) {
        if (state != State::Running)
            return;
        const Line line = parseLine(fileText);
        if (line.kind == Line::Kind::Phase)
            progress = line.progress;
        else if (line.kind == Line::Kind::End)
            ended = line;
        // garbled (a half-seen write cannot happen with the rename, but a foreign file can): keep what there is
    }

    // exitCode: the child's exit status, or -1 when a signal ended it
    void childExited(int exitCode) {
        if (state != State::Running)
            return;
        if (ended.kind == Line::Kind::End) {
            switch (ended.end) {
            case EndKind::Ok:
                state = State::Ok;
                return;
            case EndKind::Stopped:
                state = State::Stopped;
                return;
            default:
                state = State::Error;
                code = knownCode(ended.code);
                return;
            }
        }
        // no end line: the runner died (or the file never came) - the exit status is all there is
        if (exitCode == 0)
            state = State::Ok;
        else if (exitCode == 3 || (exitCode == -1 && stopAsked))
            state = State::Stopped;
        else {
            state = State::Error;
            code = "other";
        }
    }
};

//******************
// Keys and codes to words
//******************
// the phase keys, in a run's order; a key outside the list is shown as "Working..."
inline const std::vector<std::string> &phaseKeys() {
    static const std::vector<std::string> keys = {"check-space",     "download-retroarch", "download-cores",
                                                  "download-libs",   "download-apps",      "download-assets",
                                                  "download-bios",   "unpack",             "merge-config",
                                                  "install-binary",  "remove-files"};
    return keys;
}

// "phase 3/9" as the panel's counter
inline std::string phaseCounter(const Progress &p) {
    if (p.phase <= 0)
        return "";
    return std::to_string(p.phase) + "/" + std::to_string(p.phases);
}

// 0..1, or -1 for an indeterminate bar (total unknown)
inline double fraction(const Progress &p) {
    if (p.total == 0)
        return -1;
    return p.done >= p.total ? 1.0 : static_cast<double>(p.done) / static_cast<double>(p.total);
}

// "42.1 MB"
inline std::string humanBytes(uint64_t bytes) {
    char buf[32];
    if (bytes >= 1000000)
        snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / 1000000.0);
    else if (bytes >= 1000)
        snprintf(buf, sizeof(buf), "%.0f KB", static_cast<double>(bytes) / 1000.0);
    else
        snprintf(buf, sizeof(buf), "%u B", static_cast<unsigned>(bytes));
    return buf;
}

//******************
// runnerCommand
//******************
struct RunnerEnv {
    std::string runnerOverride; // AB_RA_RUNNER: the runner on a dev host (words split on blanks)
    bool psc = false;         // the console: abupdate <root> --retroarch
    std::string root;         // the console's stick root
    std::string abupdate;     // the console's abupdate
    bool appliance = false;   // a Pi / PC stick
    std::string applianceRunner; // autobleem-retroarch (path decided by the appliance job)
};

// the argv of the runner, [] when this host has none. Contract: install|remove [--bios] --progress <FILE>
inline std::vector<std::string> runnerCommand(Op op, bool withBios, const std::string &progressFile,
                                              const RunnerEnv &env) {
    std::vector<std::string> argv;
    if (!env.runnerOverride.empty()) {
        std::string word;
        for (char c : env.runnerOverride + " ") {
            if (c == ' ' || c == '\t') {
                if (!word.empty())
                    argv.push_back(word);
                word.clear();
            } else
                word += c;
        }
    } else if (env.psc) {
        argv = {env.abupdate, env.root, "--retroarch"};
    } else if (env.appliance) {
        argv = {env.applianceRunner};
    } else
        return {};
    if (argv.empty())
        return {};
    // PSC: abupdate <ROOT> --retroarch install|remove ...; elsewhere <RUNNER> install|remove ...
    argv.push_back(op == Op::Remove ? "remove" : "install");
    if (withBios)
        argv.push_back("--bios");
    argv.push_back("--progress");
    argv.push_back(progressFile);
    return argv;
}

//******************
// The panel's rows
//******************
enum class RowAction { Install, Update, Remove, DeleteBios };

struct Row {
    RowAction action;
    bool enabled;
};

// Install while absent, Update while installed (both need the network, greyed without), Remove always works
// offline. "Also delete the BIOS files" is a row of its own, there only while Remove is the chosen row.
inline bool biosRowShown(RowAction chosen) {
    return chosen == RowAction::Remove || chosen == RowAction::DeleteBios;
}

inline std::vector<Row> panelRows(bool installed, bool networkUp, RowAction chosen) {
    std::vector<Row> rows;
    rows.push_back({RowAction::Install, networkUp && !installed});
    rows.push_back({RowAction::Update, networkUp && installed});
    rows.push_back({RowAction::Remove, true});
    if (biosRowShown(chosen))
        rows.push_back({RowAction::DeleteBios, true});
    return rows;
}

// the System menu's row is there where a runner exists; the Quick menu's "Install RetroArch..." only while absent
inline bool quickInstallRowShown(bool managerAvailable, bool installed) {
    return managerAvailable && !installed;
}

} // namespace rajob
