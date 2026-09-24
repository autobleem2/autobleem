//
// hello: the SDK's sample extension (docs/extensions-plan.md) and its smoke test. It shows one of the
// launcher's own screens in the user's theme, keeps a bubble up for a few seconds after the launcher starts
// (a background extension's poll()), and logs every step of its life - which is all an extension is.
//
// A developer's sample, never packaged: its few words are not translated.
//
#include "gui/extension.h"
#include "gui/gui.h"
#include "gui/screens/gui_confirm.h"

#include <chrono>

class Hello : public Extension {
public:
    explicit Hello(ExtensionHost &host) : host(host) {
        PLOG_INFO << "created - my folder is " << host.folder() << ", my files go to " << host.stateDir();
    }

    void run() override {
        GuiConfirm confirm(*Gui::getInstance());
        confirm.title = "Hello";
        confirm.label = "An extension, drawn with the launcher's own screens";
        confirm.show();
        PLOG_INFO << "the answer was " << (confirm.result ? "confirm" : "cancel");
        if (confirm.result) {
            host.notify("Hello", "Confirmed in the extension", 0, 0);
            since = std::chrono::steady_clock::now(); // poll() takes it away again after a few seconds
            cleared = false;
        }
    }

    // a background extension: a bubble for the first few seconds, then gone
    void poll() override {
        using namespace std::chrono;
        const auto now = steady_clock::now();
        if (!greeted) {
            greeted = true;
            since = now;
            host.notify("Hello", "A background extension is running", 0, 0);
            PLOG_INFO << "first poll - the network is " << (host.networkUp() ? "up" : "down");
        } else if (!cleared && now - since > seconds(4)) {
            cleared = true;
            host.clearNotification();
        }
    }

    void suspend() override { PLOG_INFO << "suspended for a game"; }
    void resume() override { PLOG_INFO << "resumed"; }
    void shutdown() override { PLOG_INFO << "shut down"; }

private:
    ExtensionHost &host;
    bool greeted = false, cleared = false;
    std::chrono::steady_clock::time_point since;
};

AB_EXTENSION(Hello)
