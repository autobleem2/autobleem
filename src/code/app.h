//
// App: owns the model. main.cpp used to hold this as loose free functions and a global GameDatabase* - now
// it's one object, constructed once by main() after Environment is configured. What runs the program on top
// of it - the outer loop, the scan, a game launch - is AutoBleem (autobleem.h), in the executable.
//
#pragma once

#include <memory>

#include "core/model/session.h"
#include "core/services/config.h"
#include "core/services/game_catalog.h"
#include "core/services/game_query.h"
#include "core/services/game_settings.h"
#include "core/services/launch.h"
#include "core/services/memcard.h"
#include "core/services/resume_point.h"
#include "core/services/retroarch.h"
#include "core/services/clock.h"
#include "core/services/theme.h"
#include "gui/app_audio.h"
#include "gui/gui.h"

//******************
// App
//******************
// Owns the model: config.ini, the game library (both game databases + the cover database), the session
// (what to show/start next), the services, and the Gui singleton. Gui is only the screen -
// nothing non-graphical lives there. This is the top of ab_ui: every screen gets it as its `app` member, and
// the executable's AutoBleem derives from it to add run(). It takes the ProcessRunner from whoever
// constructs it, because which one is right (fork on the console, a splash on a dev host) is that caller's
// decision, not the model's.
class App {
public:
    explicit App(std::unique_ptr<ProcessRunner> runner);
    ~App();
    App(const App &) = delete;
    App &operator=(const App &) = delete;

    // valid only for the lifetime of the App instance (i.e. for the whole of main()). Used by the few places
    // that are not screens and so have no `app` member of their own: Theme, AppAudio, UtilTime, Fonts.
    static App &get();

    Config &config() { return cfg_; }
    Theme &theme() { return theme_; }
    Clock &clock() { return clock_; }
    AppAudio &audio() { return *audio_; }   // the music/sfx, not gui->audio()'s mixer device
    ableem::GameLibrary &library() { return gameLibrary; }
    GameQueryService &gameQuery() { return gameQuery_; }
    GameCatalogService &gameCatalog() { return gameCatalog_; }
    GameSettingsService &gameSettings() { return gameSettings_; }
    LaunchService &launcher() { return launcher_; }
    MemcardService &memcards() { return memcards_; }
    ResumePointService &resumePoints() { return resumePoints_; }
    RetroArchService &retroArch() { return retroArch_; }
    Lang &lang() { return lang_; }
    Session &session() { return session_; }

protected:
    static App *instance;

    // declaration order is construction order: config.ini is read before the Theme that names its
    // directory, and both before the Gui, whose constructor already needs the theme's font path.
    Config cfg_;
    Lang lang_;   // registered as the one _() consults, before anything can call _()
    Theme theme_{cfg_};
    Clock clock_{cfg_};
    std::shared_ptr<Gui> gui_;
    std::unique_ptr<AppAudio> audio_;   // needs the Gui's mixer device, so it is built in the constructor body
    ableem::GameLibrary gameLibrary;
    Session session_;
    GameQueryService gameQuery_{gameLibrary, cfg_};   // after gameLibrary: it holds a reference
    GameCatalogService gameCatalog_{gameLibrary, gameQuery_};
    GameSettingsService gameSettings_{gameLibrary};
    MemcardService memcards_{gameLibrary};
    ResumePointService resumePoints_;
    RetroArchService retroArch_;
    std::unique_ptr<ProcessRunner> runner_;
    LaunchService launcher_{cfg_, session_, gameLibrary, memcards_, resumePoints_, *runner_};
};
