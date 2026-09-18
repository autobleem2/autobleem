#include "gui_memcards_menu.h"
#include <string>
#include "../gui.h"
#include "../../core/main.h"
#include "../screens/gui_confirm.h"
#include "../screens/gui_keyboard.h"

using namespace std;

//*******************************
// GuiMemcards::init
//*******************************
void GuiMemcards::init() {
    GuiMenuBase::init(); // call the base init

    lines = app.memcards().listCards();
}

//*******************************
// GuiMemcards::getStatusLine
//*******************************
// returns the status line at the bottom
string GuiMemcards::getStatusLine() {
    return _("Card") + " " + to_string(selected + 1) + "/" + to_string(getVerticalSize()) + "   |@L1|/|@R1| " +
           _("Page") + "   |@X| " + _("Rename") + "  |@S| " + _("New Card") + "   |@T| " + _("Delete") + "  |@O| " +
           _("Go back") + "|";
}

//*******************************
// GuiMemcards::doCirclePressed
//*******************************
void GuiMemcards::doCircle_Pressed() {
    app.audio().cancel.play();
    menuVisible = false;
}

//*******************************
// GuiMemcards::doSquarePressed
//*******************************
void GuiMemcards::doSquare_Pressed() {
    app.audio().cursor.play();
    GuiKeyboard keyboard(*gui);
    keyboard.label = _("Enter new card name");
    keyboard.show();
    string result = keyboard.result;
    bool cancelled = keyboard.cancelled;

    if (result.empty()) {
        cancelled = true;
    }

    string testResult = result;
    if (Strings::compareCaseInsensitive("sony", testResult)) {
        cancelled = true;
    }

    if (!cancelled) {
        app.memcards().createCard(result);
        lines = app.memcards().listCards();
        int i = 0;
        for (const string &card : lines) {
            if (card == result) {
                selected = i;
                firstVisibleIndex = i;
                lastVisibleIndex = firstVisibleIndex + maxVisible - 1;

                if (getVerticalSize() > maxVisible) {
                    if (lastVisibleIndex >= getVerticalSize()) {
                        lastVisibleIndex = getVerticalSize() - 1;
                        firstVisibleIndex = lastVisibleIndex - maxVisible + 1;
                    }
                }
            }
            i++;
        }
    }
    render();
}

//*******************************
// GuiMemcards::doTrianglePressed
//*******************************
void GuiMemcards::doTriangle_Pressed() {
    app.audio().cursor.play();
    if (getVerticalSize() != 0) {
        GuiConfirm guiConfirm(*gui);
        guiConfirm.label = _("Delete card") + " '" + lines[selected] + "' ?";
        guiConfirm.show();
        bool result = guiConfirm.result;

        if (result) {
            app.memcards().removeCard(lines[selected]);
            lines = app.memcards().listCards();
        }
        render();
    }
}

//*******************************
// GuiMemcards::doCrossPressed
//*******************************
void GuiMemcards::doCross_Pressed() {
    app.audio().cursor.play();
    if (lines.empty()) {
        return;
    }

    GuiKeyboard keyboard(*gui);
    keyboard.label = _("Enter new name for card") + " '" + lines[selected] + "'";
    keyboard.result = lines[selected];
    keyboard.show();
    string result = keyboard.result;
    bool cancelled = keyboard.cancelled;

    if (result.empty()) {
        cancelled = true;
    }

    string testResult = result;
    if (Strings::compareCaseInsensitive("sony", testResult)) {
        cancelled = true;
    }

    for (const string &card : lines) {
        if (card == result) {
            // orevent overwrite other card
            cancelled = true;
        }
    }

    if (!cancelled) {
        app.memcards().renameCard(lines[selected], result);
        init();
        int pos = 0;
        for (const string &card : lines) {
            if (card == result) {
                selected = pos;
                firstVisibleIndex = pos;
                lastVisibleIndex = firstVisibleIndex + maxVisible - 1;
            }
            pos++;
        }
    }
    render();
}
