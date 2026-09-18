//
// AppBase: the model of a program drawn with the classic UI - config.ini, the language, the theme, the clock,
// the Gui singleton and the UI audio. The top of ab_classic. AutoBleem's App derives from it and adds the
// game library and every game service; the console tools (pscbios, abflashkit) use it as it is.
//
#pragma once

#include <memory>
#include <string>

#include "core/services/clock.h"
#include "core/services/config.h"
#include "core/services/theme.h"
#include "gui/app_audio.h"
#include "gui/gui.h"

//******************
// AppBase
//******************
// Constructed once by main() after Environment is configured. Every classic screen gets it as its `app`
// member (GuiScreen); the few non-screens that need it (Theme, AppAudio, Fonts) use AppBase::get(). The
// window title is the program's name - "AutoBleem", "PSC Bios" - and is fixed once the Gui exists.
class AppBase {
public:
    explicit AppBase(const std::string &windowTitle);
    virtual ~AppBase();
    AppBase(const AppBase &) = delete;
    AppBase &operator=(const AppBase &) = delete;
    // valid only for the lifetime of the instance (i.e. for the whole of main())
    static AppBase &get();

    Config &config() { return cfg_; }
    Theme &theme() { return theme_; }
    Clock &clock() { return clock_; }
    AppAudio &audio() { return *audio_; } // the music/sfx, not gui->audio()'s mixer device
    Lang &lang() { return lang_; }

protected:
    static AppBase *instance;
    // declaration order is construction order: config.ini is read before the Theme that names its
    // directory, and both before the Gui, whose constructor already needs the theme's font path.
    Config cfg_;
    Lang lang_; // registered as the one _() consults, before anything can call _()
    Theme theme_{cfg_};
    Clock clock_{cfg_};
    std::shared_ptr<Gui> gui_;
    std::unique_ptr<AppAudio> audio_; // needs the Gui's mixer device, so it is built in the constructor body
};
