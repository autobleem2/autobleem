//
// ra_job.h (evoui/, pure): the launcher's side of the RetroArch install/remove contract (PLATFORM-24).
//
// The launcher starts a RUNNER as a child process (abupdate <root> --retroarch ... on the console, autobleem-retroarch
// on a Pi / PC stick) with  install|remove [--bios] --progress <FILE>.  The runner rewrites <FILE> atomically, one
// UTF-8 line:
//
//     phase <i>/<n>|<key>|<done>|<total>|<unit>|<item>|<item_i>|<item_n>
//     end|ok   end|stopped   end|error|<code>   the last content, written once
//
// phase line: a phase i of n; <done>/<total> count <unit> = bytes | files | items within the phase (a runner
// writes a total above 0 in every phase; a 0 is still read as "unknown" and shown as a sweeping bar). <item> is
// the package the phase is on (cores-psc-20261003.tar.gz, cheats; may be empty), <item_i>/<item_n> which package
// of how many in this phase (0/0 = none). The old 4-field form  phase <i>/<n>|<key>|<done>|<total>  still parses
// (unit "", item "", 0/0). Fields never contain '|' or a line end. The appliance's runner writes this very syntax.
//
// and exits 0 / 3 / 1. These are the parts that need no process or screen: the line's parser, the job's status
// machine, the runner's command line, the keys' and codes' mapping and the panel's row logic - so they can be tested.
//
#pragma once

#include <algorithm>
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
    std::string unit;   // bytes | files | items; "" on an old 4-field line
    std::string item;   // the current package's name, may be empty
    int itemIndex = 0;  // which package of itemCount in this phase; 0/0 = none
    int itemCount = 0;
    bool operator==(const Progress &o) const {
        return phase == o.phase && phases == o.phases && key == o.key && done == o.done && total == o.total &&
               unit == o.unit && item == o.item && itemIndex == o.itemIndex && itemCount == o.itemCount;
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
    if ((f.size() != 4 && f.size() != 8) || f[0].compare(0, 6, "phase ") != 0 || f[1].empty())
        return line;
    const std::vector<std::string> of = detail::split(f[0].substr(6), '/');
    uint64_t i = 0, n = 0, done = 0, total = 0, itemIndex = 0, itemCount = 0;
    if (of.size() != 2 || !detail::number(of[0], i) || !detail::number(of[1], n) || i < 1 || n < i || n > 99 ||
        !detail::number(f[2], done) || !detail::number(f[3], total))
        return line;
    if (f.size() == 8) {
        if (!(f[4] == "bytes" || f[4] == "files" || f[4] == "items") || !detail::number(f[6], itemIndex) ||
            !detail::number(f[7], itemCount) || itemIndex > itemCount || itemCount > 99999)
            return line;
        line.progress.unit = f[4];
        line.progress.item = f[5];
        line.progress.itemIndex = static_cast<int>(itemIndex);
        line.progress.itemCount = static_cast<int>(itemCount);
    }
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
    std::string code;       // Error: the contract's code
    bool stopAsked = false; // stop() sent SIGTERM
    Line ended;             // the file's end line, once seen

    void begin() {
        *this = JobState();
        state = State::Running;
    }

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
        // no end line: the runner died (or the file never came) - the exit status is all there is. Exit 0 without
        // 'end|ok' (missing, garbled, only a phase line, empty) is not a success: nothing says it finished.
        // Exit 3 without 'end|stopped' stays Stopped: our SIGTERM may take the runner before it can write.
        if (exitCode == 3 || (exitCode == -1 && stopAsked))
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
    static const std::vector<std::string> keys = {
        "check-space",   "download-retroarch", "download-cores", "download-libs",
        "download-apps", "download-assets",    "download-bios",  "unpack",
        "merge-config",  "install-binary",     "remove-files"};
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

// The bar of one phase: a known total is a fraction of THIS phase only; total 0 (or no phase yet) is indeterminate
// and sweeps to and fro from the phase's start (phaseAgeMs = time since the phase began), never a carried-over value.
struct Bar {
    bool indeterminate = false;
    double value = 0;      // 0..1
    bool overReal = false; // a stalled bar: `value` is the real fraction and `sweep` a moving mark over it
    double sweep = 0;      // 0..1, where the mark is (overReal only)
};

// the to-and-fro position of a sweep, 0..1, 2 s for a round
inline double sweepPosition(unsigned ageMs) {
    const unsigned t = ageMs % 2000;
    return (t < 1000 ? t : 2000 - t) / 1000.0;
}

inline Bar barFor(const Progress &p, unsigned phaseAgeMs) {
    Bar bar;
    if (p.phase <= 0)
        return bar; // nothing seen yet: empty
    const double f = fraction(p);
    if (f >= 0) {
        bar.value = f;
        return bar;
    }
    bar.indeterminate = true;
    bar.value = sweepPosition(phaseAgeMs);
    return bar;
}

// Movement must always be visible: a bar whose phase, package and count have not changed for more than StallMs shows a
// sweep over its real value (a runner's single long step writes nothing in between).
constexpr unsigned StallMs = 1000;

inline Bar barWithStall(const Progress &p, unsigned phaseAgeMs, unsigned stalledMs) {
    Bar bar = barFor(p, phaseAgeMs);
    if (p.phase > 0 && !bar.indeterminate && stalledMs > StallMs) {
        bar.indeterminate = true;
        bar.overReal = true;
        bar.sweep = sweepPosition(stalledMs - StallMs);
    }
    return bar;
}

// When (phase, item, done) last changed: feed it the progress every frame, ask how long it has stood still.
class StallClock {
public:
    void feed(unsigned nowMs, const Progress &p) {
        if (!seen_ || p.phase != phase_ || p.item != item_ || p.itemIndex != itemIndex_ || p.done != done_) {
            seen_ = true;
            phase_ = p.phase;
            item_ = p.item;
            itemIndex_ = p.itemIndex;
            done_ = p.done;
            changedMs_ = nowMs;
        }
    }
    unsigned stalledMs(unsigned nowMs) const { return seen_ && nowMs > changedMs_ ? nowMs - changedMs_ : 0; }
    void reset() { *this = StallClock(); }

private:
    bool seen_ = false;
    int phase_ = 0;
    std::string item_;
    int itemIndex_ = 0;
    uint64_t done_ = 0;
    unsigned changedMs_ = 0;
};

// An item's name for the screen: the runner's '_' is a space (Retroarch_themes -> Retroarch themes). Display only.
inline std::string displayName(const std::string &item) {
    std::string out = item;
    for (char &c : out)
        if (c == '_')
            c = ' ';
    return out;
}

// What a package line calls the package: a plain description. A file name (it has a '.') is never shown - the phase's
// description stands for it; a plain name (cheats, Retroarch_themes) is shown with spaces for '_'.
inline std::string packageWhat(const std::string &description, const std::string &item) {
    if (item.empty() || item.find('.') != std::string::npos)
        return description;
    return displayName(item);
}

// The unit of a phase's numbers: the runner's, or - on an old 4-field line - bytes for a download, items otherwise.
inline std::string effectiveUnit(const Progress &p) {
    if (!p.unit.empty())
        return p.unit;
    return p.key.compare(0, 8, "download") == 0 ? "bytes" : "items";
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

// "18.4 GB" / "512.0 MB" (a stick's free space: always one decimal)
inline std::string humanSpace(uint64_t bytes) {
    char buf[32];
    if (bytes >= 1000000000ULL)
        snprintf(buf, sizeof(buf), "%.1f GB", static_cast<double>(bytes) / 1e9);
    else
        snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(bytes) / 1e6);
    return buf;
}

// the used part of the stick, 0..1 (the thin bar of the panel); 0 when the size is unknown
inline double usedFraction(uint64_t freeBytes, uint64_t totalBytes) {
    if (totalBytes == 0 || freeBytes >= totalBytes)
        return 0;
    return static_cast<double>(totalBytes - freeBytes) / static_cast<double>(totalBytes);
}

// "128 of 289 MB" / "1200 of 5400 files" / "2 of 4", as the numbers and what they count (the screen puts the words
// around them). Bytes are MB (a total under 10 MB with one decimal) or KB (under 1 MB): never a raw byte count.
struct Amount {
    enum class Kind { MB, KB, Files, Items };
    Kind kind = Kind::Items;
    std::string done;
    std::string total;
};

inline Amount amountFor(uint64_t done, uint64_t total, const std::string &unit) {
    Amount a;
    char buf[32];
    if (unit == "bytes") {
        if (total < 1000000) {
            a.kind = Amount::Kind::KB;
            snprintf(buf, sizeof(buf), "%.0f", static_cast<double>(done) / 1e3);
            a.done = buf;
            snprintf(buf, sizeof(buf), "%.0f", static_cast<double>(total) / 1e3);
            a.total = buf;
        } else {
            a.kind = Amount::Kind::MB;
            const char *format = total < 10000000 ? "%.1f" : "%.0f";
            snprintf(buf, sizeof(buf), format, static_cast<double>(done) / 1e6);
            a.done = buf;
            snprintf(buf, sizeof(buf), format, static_cast<double>(total) / 1e6);
            a.total = buf;
        }
        return a;
    }
    a.kind = unit == "files" ? Amount::Kind::Files : Amount::Kind::Items;
    a.done = std::to_string(done);
    a.total = std::to_string(total);
    return a;
}

// an empty unit (an old 4-field line) is read by the phase's key
inline Amount amountFor(const Progress &p) {
    return amountFor(p.done, p.total, effectiveUnit(p));
}

// what the Remove confirm lists: the lines as ids, the screen gives them their words
enum class RemoveItem {
    // deleted
    RetroArch,
    Cores,
    MenuAssets,
    Databases,
    Cheats,
    Overlays,
    Shaders,
    Bios,
    // stays
    Games,
    Playlists,
    Covers,
    Saves,
    SaveStates,
    Settings,
};

struct RemoveLists {
    std::vector<RemoveItem> deleted;
    std::vector<RemoveItem> stays;
};

inline RemoveLists removeLists(bool withBios) {
    RemoveLists lists;
    lists.deleted = {RemoveItem::RetroArch, RemoveItem::Cores,    RemoveItem::MenuAssets, RemoveItem::Databases,
                     RemoveItem::Cheats,    RemoveItem::Overlays, RemoveItem::Shaders};
    if (withBios)
        lists.deleted.push_back(RemoveItem::Bios);
    lists.stays = {RemoveItem::Games, RemoveItem::Playlists,  RemoveItem::Covers,
                   RemoveItem::Saves, RemoveItem::SaveStates, RemoveItem::Settings};
    return lists;
}

// "2.4 MB/s" (decimal is '.' or ',' as the language writes it); "" while the speed is not known
inline std::string speedText(double bytesPerSecond, char decimal) {
    char buf[32];
    if (bytesPerSecond >= 1e6)
        snprintf(buf, sizeof(buf), "%.1f MB/s", bytesPerSecond / 1e6);
    else
        snprintf(buf, sizeof(buf), "%.0f KB/s", bytesPerSecond / 1e3);
    std::string out = buf;
    for (char &c : out)
        if (c == '.')
            c = decimal;
    return out;
}

// the time left as the meter says it: "About 1 min left" is rounded to whole minutes from 60 s, to 5 s below
inline std::string roughDuration(double seconds) {
    if (seconds < 0)
        return "";
    char buf[32];
    if (seconds >= 60)
        snprintf(buf, sizeof(buf), "%ld min", static_cast<long>(seconds / 60.0 + 0.5));
    else
        snprintf(buf, sizeof(buf), "%ld s", std::max(5L, static_cast<long>(seconds / 5.0 + 0.5) * 5));
    return buf;
}

// the whole job, 0..1, in equal steps: phase i of n at fraction f is (i - 1 + f) / n
inline double jobFraction(const Progress &p) {
    if (p.phase <= 0 || p.phases <= 0)
        return 0;
    const double f = fraction(p) < 0 ? 0 : fraction(p);
    return (static_cast<double>(p.phase - 1) + f) / static_cast<double>(p.phases);
}

// "3 min 20 s" / "45 s"; "" when it is not known
inline std::string humanDuration(double seconds) {
    if (seconds < 0)
        return "";
    const long s = static_cast<long>(seconds + 0.5);
    char buf[32];
    if (s >= 60)
        snprintf(buf, sizeof(buf), "%ld min %02ld s", s / 60, s % 60);
    else
        snprintf(buf, sizeof(buf), "%ld s", s);
    return buf;
}

// The speed of a download, smoothed over a few seconds: feed it (time, bytes done) every frame; it keeps an
// exponentially weighted rate, so one stalled or quick read does not make the numbers jump. A new phase or package
// (done going back) starts it again.
class RateMeter {
public:
    void feed(unsigned nowMs, uint64_t done) {
        if (!started_ || done < lastDone_ || nowMs < lastMs_) {
            started_ = true;
            rate_ = 0;
            haveRate_ = false;
            samples_ = 0;
            lastMs_ = nowMs;
            lastDone_ = done;
            return;
        }
        const unsigned dt = nowMs - lastMs_;
        if (dt < 250)
            return; // too short to tell
        const double instant = static_cast<double>(done - lastDone_) * 1000.0 / dt;
        const double weight = dt >= 4000 ? 1.0 : dt / 4000.0; // a window of about 4 s
        rate_ = haveRate_ ? rate_ + (instant - rate_) * weight : instant;
        haveRate_ = true;
        samples_++;
        lastMs_ = nowMs;
        lastDone_ = done;
    }
    void reset() { *this = RateMeter(); }
    // a number is shown only from MinSamples readings on: the first ones swing too much to be believed
    static constexpr int MinSamples = 3;
    bool known() const { return haveRate_ && rate_ > 0 && samples_ >= MinSamples; }
    double bytesPerSecond() const { return rate_; }
    // seconds left for `remaining` bytes, -1 when the speed is not known yet
    double secondsLeft(uint64_t remaining) const { return known() ? static_cast<double>(remaining) / rate_ : -1; }

private:
    bool started_ = false;
    bool haveRate_ = false;
    int samples_ = 0;
    double rate_ = 0;
    unsigned lastMs_ = 0;
    uint64_t lastDone_ = 0;
};

// Stop (Circle) is for Install / Update only: a Remove runs to the end (half of it undone would leave RetroArch
// broken); a Remove left half-done by a power loss is finished by the next Install or Remove.
inline bool stopAllowed(Op op) {
    return op != Op::Remove;
}

// the notice before Install / Update (10-15 minutes, ~400 MB); Remove has its own confirm
inline bool noticeFirst(Op op) {
    return op != Op::Remove;
}

//******************
// runnerCommand
//******************
struct RunnerEnv {
    std::string runnerOverride;  // AB_RA_RUNNER: the runner on a dev host (words split on blanks)
    bool psc = false;            // the console: abupdate <root> --retroarch
    std::string root;            // the console's stick root
    std::string abupdate;        // the console's abupdate
    bool appliance = false;      // a Pi / PC stick
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

// How the end of a run is shown: a plain message with one OK for what happened (Done, Stopped, every error), nothing
// while no run has ended. The yes/no questions (Install/Remove, "Stop?") are confirms and not part of an outcome.
enum class EndBox { None, Message };

inline EndBox boxFor(State state) {
    return state == State::Ok || state == State::Stopped || state == State::Error ? EndBox::Message : EndBox::None;
}

// the System menu's row is there where a runner exists; the Quick menu's "Install RetroArch..." only while absent
inline bool quickInstallRowShown(bool managerAvailable, bool installed) {
    return managerAvailable && !installed;
}

} // namespace rajob
