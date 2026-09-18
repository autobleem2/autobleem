//
// App: owns the model. main.cpp used to hold this as loose free functions and a global GameDatabase* - now
// it's one object, constructed once by main() after Environment is configured. What runs the program on top
// of it - the outer loop, the scan, a game launch - is AutoBleem (autobleem.h), in the executable.
//
#pragma once
#include <memory>
#include "app_base.h"
#include "core/model/session.h"
#include "core/services/game_catalog.h"
#include "core/services/game_query.h"
#include "core/services/game_settings.h"
#include "core/services/launch.h"
#include "core/services/lightgun.h"
#include "core/services/memcard.h"
#include "core/services/resume_point.h"
#include "core/services/retroarch.h"
#include "core/services/scan_service.h"
#include <ableem/engine/thumbnail_lookup.h>

//******************
// App
//******************
// AutoBleem's model on top of AppBase (config, language, theme, clock, Gui, audio): the game library (both
// game databases + the cover database), the session (what to show/start next) and the services. This is the
// top of ab_ui: the game-aware screens reach it through App::get() (their `app` member is the AppBase), and
// the executable's AutoBleem derives from it to add run(). It takes the ProcessRunner from whoever
// constructs it, because which one is right (fork on the console, a splash on a dev host) is that caller's
// decision, not the model's.
class App : public AppBase {
public:
    explicit App(std::unique_ptr<ProcessRunner> runner);
    ~App() override;
    // the one instance, as the game model; valid for the lifetime of the App (the whole of main())
    static App &get() { return static_cast<App &>(AppBase::get()); }
    ableem::GameLibrary &library() { return gameLibrary; }
    GameQueryService &gameQuery() { return gameQuery_; }
    GameCatalogService &gameCatalog() { return gameCatalog_; }
    GameSettingsService &gameSettings() { return gameSettings_; }
    LightgunService &lightguns() { return lightguns_; }
    LaunchService &launcher() { return launcher_; }
    MemcardService &memcards() { return memcards_; }
    ResumePointService &resumePoints() { return resumePoints_; }
    RetroArchService &retroArch() { return retroArch_; }
    // where covers and screenshots are in RetroArch's thumbnails tree; the launcher's own listing cache
    ableem::ThumbnailLookup &thumbnails() { return thumbnails_; }
    ScanService &scans() { return scans_; }
    Session &session() { return session_; }

protected:
    ableem::GameLibrary gameLibrary;
    Session session_;
    GameQueryService gameQuery_{gameLibrary, cfg_}; // after gameLibrary: it holds a reference
    GameCatalogService gameCatalog_{gameLibrary, gameQuery_};
    GameSettingsService gameSettings_{gameLibrary};
    LightgunService lightguns_{gameLibrary};
    MemcardService memcards_{gameLibrary};
    ResumePointService resumePoints_;
    RetroArchService retroArch_;
    ableem::ThumbnailLookup thumbnails_;
    ScanService scans_{gameLibrary};
    std::unique_ptr<ProcessRunner> runner_;
    LaunchService launcher_{cfg_, session_, gameLibrary, memcards_, resumePoints_, *runner_};
};
