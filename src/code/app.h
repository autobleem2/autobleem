//
// App: owns the model. main.cpp used to hold this as loose free functions and a global GameDatabase* - now
// it's one object, constructed once by main() after Environment is configured.
//
#pragma once

#include <memory>

#include "session.h"
#include "core/services/config.h"
#include "core/services/game_catalog.h"
#include "core/services/game_query.h"
#include "engine/theme.h"
#include "engine/app_audio.h"
#include "engine/scanner.h"
#include "gui/gui.h"

//******************
// App
//******************
// Owns the model: config.ini, the game library (both game databases + the cover database), the session
// (what to show/start next), the scanner, and the Gui singleton. One instance is created in main() and its
// run() is the whole program from there on. Gui is only the screen - nothing non-graphical lives there.
class App {
public:
    App();
    ~App();
    App(const App &) = delete;
    App &operator=(const App &) = delete;

    // valid only for the lifetime of the App instance (i.e. for the whole of main()). Used by the few places
    // that are not screens and so have no `app` member of their own: the launch interceptors, PsGame, UtilTime.
    static App &get();

    int run();

    Config &config() { return cfg_; }
    Theme &theme() { return theme_; }
    AppAudio &audio() { return *audio_; }   // the music/sfx, not gui->audio()'s mixer device
    ableem::GameLibrary &library() { return gameLibrary; }
    GameQueryService &gameQuery() { return gameQuery_; }
    GameCatalogService &gameCatalog() { return gameCatalog_; }
    Session &session() { return session_; }
    Scanner &scanner() { return *scanner_; }

    // writes rc/autobleem_cfg.sh (AB_SELECTION/AB_THEME/AB_PCSX/AB_MIP) so the shell launch scripts see the
    // menu choice and theme/emulator settings after the GUI exits or before a game starts.
    void writeSelectionScript();

private:
    static App *instance;

    // declaration order is construction order: config.ini is read before the Theme that names its
    // directory, and both before the Gui, whose constructor already needs the theme's font path.
    Config cfg_;
    Theme theme_;
    std::shared_ptr<Gui> gui_;
    std::unique_ptr<AppAudio> audio_;   // needs the Gui's mixer device, so it is built in the constructor body
    std::shared_ptr<Scanner> scanner_;
    ableem::GameLibrary gameLibrary;
    GameQueryService gameQuery_{gameLibrary, cfg_};   // after gameLibrary: it holds a reference
    GameCatalogService gameCatalog_{gameLibrary, gameQuery_};
    Session session_;

    bool openLibrary();                                                   // covers dir + regional.db + internal.db
    void rescan(GamesHierarchy &gamesHierarchy, const std::string &prevPath);   // a Re/Scan menu selection
    void launchGame();                                                    // the MENU_OPTION_START handling
};
