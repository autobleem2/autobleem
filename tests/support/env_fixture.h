//
// EnvFixture: points ableem::Environment at a test's own tree and puts it back afterwards.
//
#pragma once

#include <ableem/engine/environment.h>
#include <ableem/engine/filesystem.h>
#include "core/services/environment.h"
#include <string>
#include <vector>

//******************
// EnvFixture
//******************
// ableem::Environment's setters are static, i.e. global: without this every test would inherit whatever
// paths the previous one left behind, and the suite would pass or fail depending on the order it ran in.
// Construct one in any test that touches Env (directly, or through a class that reads a path).
//
//     TempDir tmp("config");
//     EnvFixture env;
//     env.setWorkingPath(tmp.path());
//
// The destructor restores every root to what it was. One caveat: when nothing had set a working path,
// Environment::getWorkingPath() answers with the current directory, so that is what gets restored rather
// than "unset" - harmless, because a test process never depends on the difference.
class EnvFixture {
public:
    EnvFixture() { saved_ = capture(); }
    ~EnvFixture() { restore(saved_); }

    EnvFixture(const EnvFixture &) = delete;
    EnvFixture &operator=(const EnvFixture &) = delete;

    // the roots a test is likely to want; anything else is derived from them by Environment itself
    void setUsbRoot(const std::string &p) { ableem::Environment::setUsbRoot(p); }
    void setGamesDir(const std::string &p) { ableem::Environment::setGamesDir(p); }
    void setWorkingPath(const std::string &p) { ableem::Environment::setWorkingPath(p); }
    void setThemesDir(const std::string &p) { ableem::Environment::setThemesDir(p); }
    void setSonyDataPath(const std::string &p) { ableem::Environment::setSonyDataPath(p); }
    void setCoversDbDir(const std::string &p) { ableem::Environment::setCoversDbDir(p); }
    void setRegionalDbFile(const std::string &p) { ableem::Environment::setRegionalDbFile(p); }
    void setInternalDbFile(const std::string &p) { ableem::Environment::setInternalDbFile(p); }
    void setInternalGamesDir(const std::string &p) { ableem::Environment::setInternalGamesDir(p); }
    void setRetroarchDir(const std::string &p) { ableem::Environment::setRetroarchDir(p); }
    void setRetroarchCoreFile(const std::string &p) { ableem::Environment::setRetroarchCoreFile(p); }

private:
    struct Roots {
        std::string usbRoot, gamesDir, regionalDbFile, internalDbFile, workingPath;
        std::string sonyDataPath, themesDir, coversDbDir, internalGamesDir, retroarchDir, retroarchCoreFile;
        std::vector<std::string> retroArchBinaries;
    };

    static Roots capture() {
        using E = ableem::Environment;
        return Roots{E::getPathToUSBRoot(), E::getPathToGamesDir(), E::getPathToRegionalDBFile(),
                     E::getPathToInternalDBFile(), E::getWorkingPath(), E::getSonyPath(),
                     E::getPathToThemesDir(), E::getPathToCoversDBDir(), E::getPathToInternalGamesDir(),
                     // an explicit override is kept as such; a derived one is "" so the derivation survives
                     E::getPathToRetroarchDir() == E::getPathToUSBRoot() + ableem::sep + "retroarch"
                         ? std::string() : E::getPathToRetroarchDir(),
                     E::getPathToRetroarchCoreFile() == E::getPathToRetroarchDir() + ableem::sep +
                             "cores/km_pcsx_rearmed_neon_libretro.so"
                         ? std::string() : E::getPathToRetroarchCoreFile(),
                     ::Environment::retroArchBinaries()};
    }

    static void restore(const Roots &r) {
        using E = ableem::Environment;
        E::setUsbRoot(r.usbRoot);
        E::setGamesDir(r.gamesDir);
        E::setRegionalDbFile(r.regionalDbFile);
        E::setInternalDbFile(r.internalDbFile);
        E::setWorkingPath(r.workingPath);
        E::setSonyDataPath(r.sonyDataPath);
        E::setThemesDir(r.themesDir);
        E::setCoversDbDir(r.coversDbDir);
        E::setInternalGamesDir(r.internalGamesDir);
        E::setRetroarchDir(r.retroarchDir);
        E::setRetroarchCoreFile(r.retroarchCoreFile);
        ::Environment::setRetroArchBinaries(r.retroArchBinaries);
    }

    Roots saved_;
};
