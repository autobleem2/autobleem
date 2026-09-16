//
// Created by screemer on 2019-01-24.
//

#include "gui_keyboard.h"
#include "gui_about.h"
#include <string>
#include "gui.h"
#include "../lang.h"
#include "../engine/scanner.h"
#include <iostream>

using namespace std;

vector<string> row0 = {"1", "2", "3", "4", "5", "6", "7", "8", "9", "0"};
vector<string> row1 = {"q", "w", "e", "r", "t", "y", "u", "i", "o", "p"};
vector<string> row2 = {"a", "s", "d", "f", "g", "h", "j", "k", "l", "."};
vector<string> row3 = {"z", "x", "c", "v", "b", "n", "m", "_", "-", " "};

#define numColumns 10
#define numRows 4
#define xlast (numColumns-1)
#define ylast (numRows-1)
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
    int yoffset = gui->renderLogo(true);
    gui->renderLabelBox(1, yoffset);
    gui->renderTextLine("-= " + label + " =-", 0, yoffset, XALIGN_CENTER);

    //*******************************
    // drawRectangle lambda
    //*******************************
    auto drawRectangle = [&] (ableem::Rect& rect) {
        string fg = app.theme().data.values["text_fg"];
        renderer.setDrawColor(ableem::Color(gui->getR(fg), gui->getG(fg), gui->getB(fg), 255));
        renderer.setBlendMode(ableem::BlendMode::Blend);
        renderer.drawRect(rect);
        ableem::Rect rectSelection2;
        rectSelection2.x = rect.x + 1;
        rectSelection2.y = rect.y + 1;
        rectSelection2.w = rect.w - 2;
        rectSelection2.h = rect.h - 2;
        renderer.drawRect(rectSelection2);
    };

    string displayResult;
    if (displayAsterisksInstead)
        displayResult = string(result.size(), '*');
    else
        displayResult = result;
    displayResult.insert(cursorIndex, "#");
    gui->renderTextLine(displayResult, 1, yoffset, XALIGN_CENTER);

    ableem::Rect rect2 = gui->getOpscreenRectOfTheme();
    int fontHeight = gui->themeFont.lineHeight();

    if (L2_cursor_shift || usingUsbKeyboard) {
        ableem::Rect rectEditbox = gui->getFontTextRect(gui->themeFont, displayResult);
        rectEditbox.x = gui->align_xPosition(XALIGN_CENTER, 0, rectEditbox.w);
        rectEditbox.y = (1 * rectEditbox.h) + yoffset;  // line 1 (0 == top)

        // compute the bounding box around the cursor (#)
        ableem::Size textBeforeCursorSize;
        // get the size of the text before the cursor
        if (cursorIndex > 0) {
            textBeforeCursorSize = gui->getFontTextSize(gui->themeFont, displayResult.substr(0, cursorIndex));
        }
        // get the cursor size
        ableem::Size cursorSize = gui->getFontTextSize(gui->themeFont, "#");
        // bounding box rectangle around the # cursor
        ableem::Rect cursorRect { rectEditbox.x + textBeforeCursorSize.w, rectEditbox.y,    // x, y position
                              cursorSize.w, cursorSize.h };                             // w, h

        drawRectangle(cursorRect);
    }

    if (!usingUsbKeyboard) {
        for (int x = 0; x < numColumns; x++) {
            for (int y = 0; y < numRows; y++) {
                ableem::Rect rectSelection;
                rectSelection.x = rect2.x + indentOffset;
                rectSelection.y = yoffset + fontHeight * (y + 3);
                rectSelection.w = rect2.w - (indentOffset + indentOffset);
                rectSelection.h = fontHeight;

                int buttonWidth = (rectSelection.w / 10) - (indentOffset + indentOffset);
                int buttonHeight = rectSelection.h - 2;

                rectSelection.w = buttonWidth;
                rectSelection.h = buttonHeight;

                rectSelection.x = rectSelection.x + ((buttonWidth + 11) * x);

                string bg = app.theme().data.values["key_bg"];
                renderer.setDrawColor(ableem::Color(gui->getR(bg), gui->getG(bg), gui->getB(bg),
                                       atoi(app.theme().data.values["keyalpha"].c_str())));
                renderer.setBlendMode(ableem::BlendMode::Blend);
                renderer.fillRect(rectSelection);

                string text = rows[y][x];
                if (L1_caps_shift) {
                    text = ucase(text);
                }

                gui->renderTextChar(text, 3 + y, yoffset, rectSelection.x + 10);

                // display rectangle around current character
                if (!L2_cursor_shift) { // don't draw rectangle if in move cursor mode
                    if ((selx == x) && (sely == y)) {
                        drawRectangle(rectSelection);
                    }
                }
            }
        }
    }

    if (usingUsbKeyboard) {
        gui->renderStatus(
                "|@Tab| " + _("Use Controller") + "  |@Enter| " + _("Confirm") +
                "  |@Esc| " + _("Cancel") + " |");
    } else {
        gui->renderStatus(
                "|@X| " + _("Select") + "  |@T|  " + _("Backspace") + "  |@L1| " + _("Caps") + "  |@L2| " +
                _("Move Cursor") + "(#)" + " |@S| " + _("Space") +
                "      |@Start| " + _("Confirm") + "  |@O| " + _("Cancel") + " |");
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
    gui->cursor.play();
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
    gui->cursor.play();
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
    gui->cursor.play();
    L2_cursor_shift = !L2_cursor_shift;
    usingUsbKeyboard = !usingUsbKeyboard;
    render();
}

//*******************************
// GuiKeyboard::doKbdEscape
//*******************************
void GuiKeyboard::doKbdEscape() {
    gui->cursor.play();
    cancelled = true;
    menuVisible = false;
}

//*******************************
// GuiKeyboard::doKbdReturn
//*******************************
void GuiKeyboard::doKbdReturn() {
    gui->cursor.play();
    cancelled = false;
    menuVisible = false;
}

//*******************************
// GuiKeyboard::doKbdTextInput
//*******************************
void GuiKeyboard::doKbdTextInput(const std::string& text) {
    gui->cursor.play();
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
    gui->cursor.play();
    L1_caps_shift = false;
    render();
}

//*******************************
// GuiKeyboard::doL2_up
//*******************************
void GuiKeyboard::doL2_up() {
    gui->cursor.play();
    L2_cursor_shift = false;
    render();
}

//*******************************
// GuiKeyboard::doL1_down
//*******************************
void GuiKeyboard::doL1_down() {
    gui->cursor.play();
    L1_caps_shift = true;
    render();
}

//*******************************
// GuiKeyboard::doL2_down
//*******************************
void GuiKeyboard::doL2_down() {
    gui->cursor.play();
    L2_cursor_shift = true;
    render();
}

//*******************************
// GuiKeyboard::doTrianglePressed
//*******************************
void GuiKeyboard::doTriangle() {
    gui->cursor.play();
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
    gui->cursor.play();
    result.insert(cursorIndex, " ");
    ++cursorIndex;
    render();
}

//*******************************
// GuiKeyboard::doCrossPressed
//*******************************
void GuiKeyboard::doCross() {
    gui->cursor.play();
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
    gui->cursor.play();
    cancelled = false;
    menuVisible = false;
}

//*******************************
// GuiKeyboard::doCirclePressed
//*******************************
void GuiKeyboard::doCircle() {
    gui->cursor.play();
    cancelled = true;
    menuVisible = false;
}

//*******************************
// GuiKeyboard::doJoyRight
//*******************************
void GuiKeyboard::doJoyRight() {
    gui->cursor.play();
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
    gui->cursor.play();
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
    gui->cursor.play();
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
    gui->cursor.play();
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
                    if (e.button == Button::L1) {     // caps shift
                        doL1_down();
                    } else if (e.button == Button::L2) {     // move cursor shift
                        doL2_down();
                    }

                    if (!L2_cursor_shift) {
                        if (e.button == Button::Triangle) {   // delete char on the left
                            doTriangle();
                        } else if (e.button == Button::Square) {     //insert space
                            doSquare();
                        } else if (e.button == Button::Cross) {
                            doCross();
                        } else if (e.button == Button::Start) {  // Confirm
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