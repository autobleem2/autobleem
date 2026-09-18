#include "ableem/engine/retroarch_cores.h"
#include "ableem/engine/filesystem.h"
#include "ableem/engine/log.h"
#include "ableem/engine/strings.h"

#include <algorithm>
#include <fstream>
#include <sstream>

using namespace std;

namespace ableem {

namespace {

bool sortByMaxExtensions(const CoreInfoPtr &i, const CoreInfoPtr &j) {
    return i->extensions.size() > j->extensions.size();
}

string stripLpl(const string &dbName) {
    return DirEntry::matchExtension(dbName, "lpl") ? DirEntry::getFileNameWithoutExtension(dbName) : dbName;
}

// the value of a `key = "a|b|c"` line, unquoted and trimmed
string unquoted(const string &line, const string &lcaseLine) {
    string value = line.substr(lcaseLine.find('=') + 1);
    value.erase(remove(value.begin(), value.end(), '\"'), value.end());
    trim(value);
    return value;
}

vector<string> splitBar(const string &value) {
    vector<string> out;
    stringstream in(value);
    string item;
    while (getline(in, item, '|'))
        out.push_back(item);
    return out;
}

} // namespace

//*******************************
// CoreInfoTable::parseInfoFile
//*******************************
CoreInfoPtr CoreInfoTable::parseInfoFile(const string &file, const string &corePath) {
    CoreInfoPtr info = make_shared<CoreInfo>();
    info->core_path = corePath;

    ifstream in(file);
    string line;
    while (getline(in, line)) {
        string lcaseLine = line;
        lcase(lcaseLine);
        if (lcaseLine.find('=') == string::npos)
            continue;
        if (lcaseLine.rfind("display_name", 0) == 0) {
            info->name = unquoted(line, lcaseLine);
        } else if (lcaseLine.rfind("supported_extensions", 0) == 0) {
            info->extensions = splitBar(unquoted(line, lcaseLine));
        } else if (lcaseLine.rfind("database", 0) == 0) {
            info->databases = splitBar(unquoted(line, lcaseLine));
        } else if (lcaseLine.rfind("block_extract", 0) == 0) {
            info->block_extract = toLowerCopy(unquoted(line, lcaseLine)) == "true";
        }
    }
    return info;
}

//*******************************
// CoreInfoTable::load
//*******************************
void CoreInfoTable::load(const string &retroarchDir, const string &coresCfgPath) {
    cores_.clear();
    databases_.clear();
    defaultCores_.clear();
    overrideCores_.clear();

    if (!DirEntry::exists(retroarchDir)) {
        PLOG_WARNING << "RetroArch not found at " << retroarchDir;
        return;
    }
    const string infoDir = retroarchDir + sep + "info";
    const string coresDir = retroarchDir + sep + "cores";
    PLOG_INFO << "Reading core info files in " << infoDir;
    for (const DirEntry &entry : DirEntry::diru_FilesOnly(infoDir)) {
        if (DirEntry::getFileExtension(entry.name) != "info")
            continue;
        string corePath = coresDir + sep + DirEntry::getFileNameWithoutExtension(entry.name) + ".so";
        CoreInfoPtr info = parseInfoFile(infoDir + sep + entry.name, corePath);
        if (!DirEntry::exists(info->core_path))
            continue;
        cores_.push_back(info);
        for (const string &db : info->databases)
            databases_.insert(db);
    }
    stable_sort(cores_.begin(), cores_.end(), sortByMaxExtensions);
    PLOG_INFO << "Installed cores: " << cores_.size() << ", databases: " << databases_.size();

    // each database gets the first core (most extensions first) whose .info lists it
    for (const string &dbName : databases_) {
        for (const CoreInfoPtr &info : cores_) {
            if (find(info->databases.begin(), info->databases.end(), dbName) != info->databases.end()) {
                defaultCores_.emplace_back(dbName, info);
                PLOG_DEBUG << "Mapping DB: " << dbName << "  Core: " << info->name;
                break;
            }
        }
    }

    if (coresCfgPath.empty())
        return;
    ifstream in(coresCfgPath);
    string line;
    while (getline(in, line)) {
        if (line.empty() || line[0] == '#' || line.find('=') == string::npos)
            continue;
        string dbName = line.substr(0, line.find('='));
        string value = line.substr(line.find('=') + 1);
        lcase(dbName);
        trim(dbName);
        trim(value);
        for (const CoreInfoPtr &info : cores_) {
            if (info->name.find(value) != string::npos) {
                overrideCores_.emplace_back(dbName, info);
                PLOG_INFO << "Core override: " << dbName << " -> " << info->name;
            }
        }
    }
}

//*******************************
// CoreInfoTable::overrideCoreFor
//*******************************
CoreInfoPtr CoreInfoTable::overrideCoreFor(const string &dbName) const {
    string key = stripLpl(dbName);
    lcase(key);
    trim(key);
    for (const auto &kv : overrideCores_) {
        if (kv.first == key)
            return kv.second;
    }
    return nullptr;
}

//*******************************
// CoreInfoTable::defaultCoreFor
//*******************************
CoreInfoPtr CoreInfoTable::defaultCoreFor(const string &dbName) const {
    const string key = stripLpl(dbName);
    for (const auto &kv : defaultCores_) {
        if (kv.first == key)
            return kv.second;
    }
    return nullptr;
}

//*******************************
// CoreInfoTable::coreForDatabase
//*******************************
CoreInfoPtr CoreInfoTable::coreForDatabase(const string &dbName) const {
    CoreInfoPtr core = overrideCoreFor(dbName);
    return core ? core : defaultCoreFor(dbName);
}

} // namespace ableem
