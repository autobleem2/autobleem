#include "gui_keep_display.h"
#include "gui/gui.h"

using namespace std;

//*******************************
// GuiKeepDisplay::init / updateLabel
//*******************************
void GuiKeepDisplay::init() {
    GuiConfirm::init();
    title = _("Display");
    confirmLabel = _("Keep");
    cancelLabel = _("Go back");
    updateLabel(Seconds);
}

void GuiKeepDisplay::updateLabel(int secondsLeft) {
    label = _("Keep this display mode?") + " " + modeLabel + ". " + _("Going back to the previous mode in") + " " +
            to_string(secondsLeft) + " s";
}

//*******************************
// GuiKeepDisplay::loop
//*******************************
void GuiKeepDisplay::loop() {
    shared_ptr<Gui> gui(Gui::getInstance());
    const unsigned int start = gui->platform().ticks();
    int shown = Seconds;
    result = false;
    menuVisible = true;
    while (menuVisible) {
        const int left = Seconds - static_cast<int>((gui->platform().ticks() - start) / 1000);
        if (left <= 0) {
            result = false; // nobody answered: the new mode shows nothing on this display
            break;
        }
        if (left != shown) {
            shown = left;
            updateLabel(left);
        }
        render();

        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit) {
                menuVisible = false;
            } else if (e.type == Event::Type::ButtonDown && e.button == Button::Cross) {
                app.audio().cursor.play();
                result = true;
                menuVisible = false;
            } else if (e.type == Event::Type::ButtonDown && e.button == Button::Circle) {
                app.audio().cancel.play();
                menuVisible = false;
            } else if (e.type == Event::Type::KeyDown && e.key == Key::Return) {
                result = true;
                menuVisible = false;
            } else if (e.type == Event::Type::KeyDown && e.key == Key::Escape) {
                menuVisible = false;
            }
        }
    }
}
