// lib_ableem - engine: RetroArch's installed cores as their .info files describe them, and which core plays
// which database (system). Was the core-mapping half of the app's RetroArchService; the scanner needs the
// same table on its own thread, so it lives here and each side builds its own.
#pragma once

#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace ableem {

//******************
// CoreInfo
//******************
// one <retroarch>/info/<core>.info file: what the core is called, what it plays, and the .so it lives in
struct CoreInfo {
    std::string name; // display_name
    std::vector<std::string> extensions;
    std::vector<std::string> databases;
    std::string core_path;
    // the core reads archives itself (arcade sets): a .zip is handed over whole, never looked into
    bool block_extract = false;
};

using CoreInfoPtr = std::shared_ptr<CoreInfo>;
using CoreInfos = std::vector<CoreInfoPtr>;

//******************
// CoreInfoTable
//******************
// load() reads every .info and keeps the cores whose .so is actually in <retroarch>/cores - the info bundle
// describes every core libretro builds, a few hundred, and a database mapped to a core that is not
// installed would make every game of that system unplayable. Each database then gets the first installed
// core (the one with the most extensions first) whose .info lists it, unless the cores.cfg says otherwise:
// "<database name>=<part of a core's display name>", one per line, '#' comments - which core plays a
// system is platform knowledge (resources/platform/<platform>.cores.cfg in the app).
class CoreInfoTable {
public:
    void load(const std::string &retroarchDir, const std::string &coresCfgPath);

    // one .info file; corePath is the .so it names (<retroarch>/cores/<stem>.so)
    static CoreInfoPtr parseInfoFile(const std::string &file, const std::string &corePath);

    const CoreInfos &cores() const { return cores_; }
    const std::set<std::string> &databases() const { return databases_; } // every one an installed core lists
    bool empty() const { return cores_.empty(); }

    // the override for the database if the cfg named one, else the .info mapping; nullptr when no
    // installed core plays it. A ".lpl" suffix on the name is ignored.
    CoreInfoPtr coreForDatabase(const std::string &dbName) const;
    // the two halves of coreForDatabase, for a caller that wants to know which answered
    CoreInfoPtr overrideCoreFor(const std::string &dbName) const;
    CoreInfoPtr defaultCoreFor(const std::string &dbName) const;

private:
    CoreInfos cores_;
    std::set<std::string> databases_;
    std::vector<std::pair<std::string, CoreInfoPtr>> defaultCores_;  // database name -> core
    std::vector<std::pair<std::string, CoreInfoPtr>> overrideCores_; // lower-cased database name -> core
};

} // namespace ableem
