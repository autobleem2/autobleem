//
// PscBios: the program - AppBase (the main GUI's config.ini, theme, language, the Gui) plus the console
// backend the screens talk to, and run(), which shows the main screen until it is closed.
//
#pragma once

#include "app_base.h"
#include "core/console_backend.h"

#include <memory>

//******************
// PscBios
//******************
class PscBios : public AppBase {
public:
    explicit PscBios(std::unique_ptr<ConsoleBackend> console);
    // the one instance, for the screens (their `app` member is the AppBase)
    static PscBios &get() { return static_cast<PscBios &>(AppBase::get()); }

    ConsoleBackend &console() { return *console_; }

    // shows the hardware information screen; returns when it is closed
    int run();

private:
    std::unique_ptr<ConsoleBackend> console_;
};
