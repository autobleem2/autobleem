//
// Created by screemer on 2019-01-24.
//

#include "gui_keyboard.h"
#include <algorithm>
#include "gui_about.h"
#include <string>
#include "../gui.h"
#include <iostream>

using namespace std;

vector<string> row0 = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
vector<string> row1 = {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"};
vector<string> row2 = {"a", "s", "d", "f", "g", "h", "j", "k", "l", "."};
vector<string> row3 = {"z", "x", "c", "v", "b", "n", "m", "_", "-", " "};

#define numColumns 10
#define numRows 4
#define xlast (numColumns - 1)
#define ylast (numRows - 1)
#define indentOffset 5

vector<vector<string>> rows = {row0, row1, row2, row3};

//*******************************
// GuiKeyboard::init
//*******************************
void GuiKeyboard::init() {
    gui = Gui::getInstance();
    cursorIndex = result.size(); // the "#" cursor position starts out at the end of the string
}

//*******************************
// GuiKeyboard::render
//*******************************
void GuiKeyboard::render() {
    gui->renderBackground();
    gui->renderTextBar();
    int yoffset = gui->renderHeader(label);
    PanelStyle style = gui->panelStyle();
    const ableem::Rect content = gui->classicContent();
    Fonts &fonts = gui->assets().themeFonts;

    // the text field: a band across the panel with the text in the launcher's medium font and a caret
    // where the cursor is (a bar; the old '#' character is history)
    string displayResult = displayAsterisksInstead ? string(result.size(), '*') : result;
    const ableem::Font &fieldFont = fonts[FONT_22_MED];
    const int fieldH = 48;
    ableem::Rect field(content.x + PanelStyle::RowInset, yoffset + 6, content.w - 2 * PanelStyle::RowInset, fieldH);
    renderer.setBlendMode(ableem::BlendMode::Blend);
    renderer.setDrawColor(ableem::Color(255, 255, 255, 14));
    renderer.fillRect(field);
    renderer.setDrawColor(ableem::Color(style.secondary.r, style.secondary.g, style.secondary.b, 160));
    renderer.drawRect(field);
    const int textX = field.x + 16;
    const int textY = field.y + (fieldH - fieldFont.lineHeight()) / 2;
    gui->text().renderText_WithColor(fieldFont, displayResult, textX, textY, style.text, XALIGN_LEFT);
    {
        const int before = cursorIndex > 0 ? gui->text().textWidth(fieldFont, displayResult.substr(0, cursorIndex)) : 0;
        // the caret: solid while the cursor is being moved (L2 held, or a USB keyboard), blinking otherwise
        const bool on = L2_cursor_shift || usingUsbKeyboard || (gui->platform().ticks() / 500) % 2 == 0;
        if (on) {
            renderer.setDrawColor(style.text);
            renderer.fillRect(ableem::Rect(textX + before, textY + 2, 2, fieldFont.lineHeight() - 4));
        }
    }

    // the keys: a grid centred in what is left, each a tile with its character, the selected one on a band
    // with the text-colour edge; the caps shift shows on the keys themselves
    if (!usingUsbKeyboard) {
        const int gridTop = field.y + fieldH + 24;
        const int gridBottom = content.y + content.h - 12;
        const int gap = 8;
        const int keyW = min(96, (content.w - 2 * PanelStyle::RowInset - gap * (numColumns - 1)) / numColumns);
        const int keyH = min(72, (gridBottom - gridTop - gap * (numRows - 1)) / numRows);
        const int gridW = keyW * numColumns + gap * (numColumns - 1);
        const int gridH = keyH * numRows + gap * (numRows - 1);
        const int gridX = content.x + (content.w - gridW) / 2;
        const int gridY = gridTop + max(0, (gridBottom - gridTop - gridH) / 2);
        const ableem::Font &keyFont = fonts[FONT_22_MED];
        for (int y = 0; y < numRows; y++) {
            for (int x = 0; x < numColumns; x++) {
                ableem::Rect key(gridX + x * (keyW + gap), gridY + y * (keyH + gap), keyW, keyH);
                const bool selected = !L2_cursor_shift && selx == x && sely == y;
                renderer.setBlendMode(ableem::BlendMode::Blend);
                if (selected) {
                    renderer.setDrawColor(ableem::Color(style.text.r, style.text.g, style.text.b, 60));
                    renderer.fillRect(key);
                    renderer.setDrawColor(style.text);
                    renderer.drawRect(key);
                } else {
                    renderer.setDrawColor(ableem::Color(255, 255, 255, 18));
                    renderer.fillRect(key);
                    renderer.setDrawColor(ableem::Color(style.secondary.r, style.secondary.g, style.secondary.b, 110));
                    renderer.drawRect(key);
                }
                string text = rows[y][x];
                if (L1_caps_shift)
                    text = ucase(text);
                if (text == " ")
                    text = _("Space"); // the space key reads as a word
                const int tw = gui->text().textWidth(keyFont, text);
                gui->text().renderText_WithColor(keyFont, text, key.x + (keyW - tw) / 2,
                                                 key.y + (keyH - keyFont.lineHeight()) / 2,
                                                 selected ? style.text : style.secondary, XALIGN_LEFT);
            }
        }
    }

    if (usingUsbKeyboard) {
        gui->renderStatus("|@Tab| " + _("Use controller") + "  |@Enter| " + _("Confirm") + "  |@Esc| " + _("Cancel") +
                          " |");
    } else {
        gui->renderStatus("|@X| " + _("Select") + "  |@T|  " + _("Backspace") + "  |@L1| " + _("Caps") + "  |@L2| " +
                          _("Move cursor") + " |@S| " + _("Space") + "      |@Start| " + _("Confirm") + "  |@O| " +
                          _("Cancel") + " |");
    }
    renderer.present();
}

//*******************************
// GuiKeyboard::doKbdRight
//*******************************
void GuiKeyboard::doKbdRight() {
    L2_cursor_shift = true;
    usingUsbKeyboard = true;
    doJoyRight();
}

//*******************************
// GuiKeyboard::doKbdLeft
//*******************************
void GuiKeyboard::doKbdLeft() {
    L2_cursor_shift = true;
    usingUsbKeyboard = true;
    doJoyLeft();
}

//*******************************
// GuiKeyboard::doKbdHome
//*******************************
void GuiKeyboard::doKbdHome() {
    L2_cursor_shift = true;
    usingUsbKeyboard = true;
    cursorIndex = 0;
    render();
}

//*******************************
// GuiKeyboard::doKbdEnd
//*******************************
void GuiKeyboard::doKbdEnd() {
    L2_cursor_shift = true;
    usingUsbKeyboard = true;
    cursorIndex = result.size();
    render();
}

//*******************************
// GuiKeyboard::doKbdBackspace
//*******************************
void GuiKeyboard::doKbdBackspace() {
    app.audio().cursor.play();
    if (!result.empty() && cursorIndex > 0) {
        result = result.erase(cursorIndex - 1, 1);
        --cursorIndex;
    }
    L2_cursor_shift = true;
    usingUsbKeyboard = true;
    render();
}

//*******************************
// GuiKeyboard::doKbdDelete
//*******************************
void GuiKeyboard::doKbdDelete() {
    app.audio().cursor.play();
    if (!result.empty() && cursorIndex < result.size()) {
        result = result.erase(cursorIndex, 1);
    }
    L2_cursor_shift = true;
    usingUsbKeyboard = true;
    render();
}

//*******************************
// GuiKeyboard::doKbdTab
//*******************************
void GuiKeyboard::doKbdTab() {
    app.audio().cursor.play();
    L2_cursor_shift = !L2_cursor_shift;
    usingUsbKeyboard = !usingUsbKeyboard;
    render();
}

//*******************************
// GuiKeyboard::doKbdEscape
//*******************************
void GuiKeyboard::doKbdEscape() {
    app.audio().cursor.play();
    cancelled = true;
    menuVisible = false;
}

//*******************************
// GuiKeyboard::doKbdReturn
//*******************************
void GuiKeyboard::doKbdReturn() {
    app.audio().cursor.play();
    cancelled = false;
    menuVisible = false;
}

//*******************************
// GuiKeyboard::doKbdTextInput
//*******************************
void GuiKeyboard::doKbdTextInput(const std::string &text) {
    app.audio().cursor.play();
    result.insert(cursorIndex, text);
    cursorIndex += text.size();
    L2_cursor_shift = true;
    usingUsbKeyboard = true;
    render();
}

//*******************************
// GuiKeyboard::doL1_up
//*******************************
void GuiKeyboard::doL1_up() {
    app.audio().cursor.play();
    L1_caps_shift = false;
    render();
}

//*******************************
// GuiKeyboard::doL2_up
//*******************************
void GuiKeyboard::doL2_up() {
    app.audio().cursor.play();
    L2_cursor_shift = false;
    render();
}

//*******************************
// GuiKeyboard::doL1_down
//*******************************
void GuiKeyboard::doL1_down() {
    app.audio().cursor.play();
    L1_caps_shift = true;
    render();
}

//*******************************
// GuiKeyboard::doL2_down
//*******************************
void GuiKeyboard::doL2_down() {
    app.audio().cursor.play();
    L2_cursor_shift = true;
    render();
}

//*******************************
// GuiKeyboard::doTrianglePressed
//*******************************
void GuiKeyboard::doTriangle() {
    app.audio().cursor.play();
    if (!result.empty() && cursorIndex > 0) {
        result = result.erase(cursorIndex - 1, 1);
        --cursorIndex;
    }
    render();
}

//*******************************
// GuiKeyboard::doSquarePressed
//*******************************
void GuiKeyboard::doSquare() {
    app.audio().cursor.play();
    result.insert(cursorIndex, " ");
    ++cursorIndex;
    render();
}

//*******************************
// GuiKeyboard::doCrossPressed
//*******************************
void GuiKeyboard::doCross() {
    app.audio().cursor.play();
    string character = rows[sely][selx];
    string ch;
    if (L1_caps_shift)
        ch = ucase(character);
    else
        ch = character;
    result.insert(cursorIndex, ch);
    ++cursorIndex;
    render();
}

//*******************************
// GuiKeyboard::doStartPressed
//*******************************
void GuiKeyboard::doStart() {
    app.audio().cursor.play();
    cancelled = false;
    menuVisible = false;
}

//*******************************
// GuiKeyboard::doCirclePressed
//*******************************
void GuiKeyboard::doCircle() {
    app.audio().cursor.play();
    cancelled = true;
    menuVisible = false;
}

//*******************************
// GuiKeyboard::doJoyRight
//*******************************
void GuiKeyboard::doJoyRight() {
    app.audio().cursor.play();
    if (L2_cursor_shift) {
        if (cursorIndex != result.size())
            ++cursorIndex;
    } else {
        selx++;
        if (selx > xlast) {
            selx = 0;
        }
    }
    render();
}

//*******************************
// GuiKeyboard::doJoyLeft
//*******************************
void GuiKeyboard::doJoyLeft() {
    app.audio().cursor.play();
    if (L2_cursor_shift) {
        if (cursorIndex > 0)
            --cursorIndex;
    } else {
        selx--;
        if (selx < 0) {
            selx = xlast;
        }
    }
    render();
}

//*******************************
// GuiKeyboard::doJoyDown
//*******************************
void GuiKeyboard::doJoyDown() {
    app.audio().cursor.play();
    if (!L2_cursor_shift) {
        sely++;
        if (sely > ylast) {
            sely = 0;
        }
    }
    render();
}

//*******************************
// GuiKeyboard::doJoyUp
//*******************************
void GuiKeyboard::doJoyUp() {
    app.audio().cursor.play();
    if (!L2_cursor_shift) {
        sely--;
        if (sely < 0) {
            sely = ylast;
        }
    }
    render();
}

//*******************************
// GuiKeyboard::loop
//*******************************
void GuiKeyboard::loop() {
    shared_ptr<Gui> gui(Gui::getInstance());

    menuVisible = true;
    while (menuVisible) {
        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit) {
                menuVisible = false;
                continue;
            }

            switch (e.type) {
            case Event::Type::KeyDown:
                if (e.key == Key::Right) {
                    doKbdRight();

                } else if (e.key == Key::Left) {
                    doKbdLeft();

                } else if (e.key == Key::Home) {
                    doKbdHome();

                } else if (e.key == Key::End) {
                    doKbdEnd();

                } else if (e.key == Key::Backspace) {
                    doKbdBackspace();

                } else if (e.key == Key::Delete) {
                    doKbdDelete();

                } else if (e.key == Key::Tab) {
                    doKbdTab();

                } else if (e.key == Key::Escape) {
                    doKbdEscape();

                } else if (e.key == Key::Return) {
                    doKbdReturn();
                }
                break;

            case Event::Type::TextInput:
                doKbdTextInput(e.text);
                break;

            case Event::Type::ButtonUp:
                if (e.button == Button::L1) {
                    doL1_up();
                } else if (e.button == Button::L2) {
                    doL2_up();
                }
                break;

            case Event::Type::ButtonDown:
                if (e.button == Button::L1) { // caps shift
                    doL1_down();
                } else if (e.button == Button::L2) { // move cursor shift
                    doL2_down();
                }

                if (!L2_cursor_shift) {
                    if (e.button == Button::Triangle) { // delete char on the left
                        doTriangle();
                    } else if (e.button == Button::Square) { // insert space
                        doSquare();
                    } else if (e.button == Button::Cross) {
                        doCross();
                    } else if (e.button == Button::Start) { // Confirm
                        doStart();
                    } else if (e.button == Button::Circle) { // Cancel
                        doCircle();
                    }
                }
                break;

            case Event::Type::DpadDown:
            case Event::Type::DpadUp:
                if (gui->input().dpadRight()) {
                    doJoyRight();
                } else if (gui->input().dpadLeft()) {
                    doJoyLeft();
                }

                if (!L2_cursor_shift) {
                    if (gui->input().dpadDown()) {
                        doJoyDown();
                    } else if (gui->input().dpadUp()) {
                        doJoyUp();
                    }
                }

                break;
            default:
                break;
            }
        }
    }
}