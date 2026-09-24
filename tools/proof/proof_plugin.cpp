//
// PROOF (proof/plugin branch, never merged): the smallest plugin that uses the launcher's own SDK - a
// GuiConfirm drawn by the launcher's Gui in the user's theme, a line in the launcher's log, and a value
// read from its Env - loaded by AutoBleem::run() when AB_PLUGIN_PROOF names it. docs/extensions-plan.md,
// step 1.
//
#define PLOG_DEFAULT_INSTANCE_ID 1 // the launcher is instance 0: on Linux a plugin chaining 0 into itself recursed
#include "gui/screens/gui_confirm.h"
#include "core/services/environment.h"

#include <ableem/engine/log.h>

#ifdef _WIN32
#define PROOF_EXPORT extern "C" __declspec(dllexport)
#else
#define PROOF_EXPORT extern "C" __attribute__((visibility("default")))
#endif

// headless: a line through the launcher's log, and a call into the executable's own code
PROOF_EXPORT int ab_proof_ping(plog::IAppender *log) {
    plog::init<1>(plog::info, log);
    std::string keys;
    for (const std::string &k : Env::appPlatformKeys())
        keys += k + " ";
    PLOG_INFO << "[proof] ping from the plugin: the executable's Env::appPlatformKeys() = " << keys
              << "(build target " << Env::buildTargetKey() << ")";
    return 42;
}

// `log` is the launcher's logger: plog's own way of chaining a shared library into the program's log
PROOF_EXPORT int ab_proof_run(ableem::GuiBase *gui, plog::IAppender *log) {
    plog::init<1>(plog::info, log);
    PLOG_INFO << "[proof] hello from the plugin; the launcher's platform keys start with "
              << Env::appPlatformKeys().front() << ", USB root " << Env::getPathToUSBRoot();
    GuiConfirm confirm(*gui);
    confirm.title = "Plugin proof";
    confirm.label = "This dialog is the launcher's GuiConfirm, shown by a plugin";
    confirm.show();
    PLOG_INFO << "[proof] the user answered " << (confirm.result ? "confirm" : "cancel");
    return confirm.result ? 1 : 0;
}
