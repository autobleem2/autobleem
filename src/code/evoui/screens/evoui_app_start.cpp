//
// Created by screemer on 2019-12-04.
//

#include "evoui_app_start.h"
#include <string>
#include <sstream>
#include <iostream>
#include "../../gui/gui.h"
#include <ableem/engine/log.h>

using namespace std;

std::string GuiAppStart::getStringLine(const std::string &str, int lineNo) {
    std::string line;
    std::istringstream stream(str);
    while (lineNo-- >= 0)
        std::getline(stream, line);
    return line;
}

void GuiAppStart::init() {
    font = Fonts::openNewSharedCachedFont(Env::getPathToFontsDir() + sep + "OpenSans-Medium.ttf", 20, renderer);
    // Try to load app.ini
    appName = game->title;

    if (DirEntry::exists(game->readme_path)) {
        std::ifstream t(game->readme_path);
        t.seekg(0, std::ios::end);
        size_t size = t.tellg();
        PLOG_DEBUG << "Readme file size:" << size;
        buffer = "";
        t.seekg(0);
        std::string temp;
        while (std::getline(t, temp)) {
            buffer = buffer + temp + "\n";
        };

        int newlines = 0;
        const char *p = &buffer.c_str()[0];
        for (int i = 0; i < size; i++) {
            if (p[i] == '\n') {
                newlines++;
            }
        }
        totalLines = newlines + 1;
        readmeLoaded = true;
    }
}

void GuiAppStart::render() {
    shared_ptr<Gui> gui(Gui::getInstance());
    gui->renderBackground();
    // readme:
    renderer.setDrawColor(ableem::Color(0, 0, 0, 128));
    renderer.setBlendMode(ableem::BlendMode::Blend);

    ableem::Rect rect2;
    rect2.x = 10;
    rect2.y = 10;
    rect2.w = 1260;
    rect2.h = 600;

    renderer.fillRect(rect2);

    // scrollbar
    rect2.x = 1240;
    rect2.y = 40;
    rect2.w = 20;
    rect2.h = 20 * 25;

    renderer.fillRect(rect2);

    // draw scroll position
    if (maxLines < totalLines) {
        renderer.setDrawColor(ableem::Color(255, 255, 255, 255));
        int heightOfBar = 500 / (totalLines - maxLines);

        rect2.x = 1242;
        rect2.y = 40 + firstLine * heightOfBar;
        rect2.w = 16;
        rect2.h = heightOfBar;
        renderer.fillRect(rect2);
    }
    int yoffset = 15;
    gui->text().renderTextLine(appName, 0, yoffset, XALIGN_LEFT, 10, font);
    renderer.setDrawColor(ableem::Color(255, 255, 255, 255));
    renderer.drawLine(ableem::Point{rect2.x, 35}, ableem::Point{rect2.w, 35});

    if (scrolling > 0) {
        firstLine++;
    }
    if (scrolling < 0) {
        firstLine--;
    }
    if (firstLine < 0) {
        firstLine = 0;
    }
    if (maxLines < totalLines) {
        if (firstLine > totalLines - maxLines) {
            firstLine = totalLines - maxLines;
        }
    } else {
        if (firstLine > totalLines - maxLines) {
            firstLine = 0;
        }
    }

    int currentLine = 2;
    if (!readmeLoaded) {
        gui->text().renderTextLine(_("ReadMe file not found"), 2, yoffset, XALIGN_LEFT, 10, font);
    } else {
        for (int i = firstLine; i < firstLine + maxLines; i++) {
            std::string lineInFile = getStringLine(buffer, i);
            gui->text().renderTextLine(getStringLine(buffer, i), currentLine, yoffset, XALIGN_LEFT, 10, font);
            currentLine++;
        }
    }

    gui->renderStatus("|@X| " + _("OK") + "  |@O| " + _("Cancel") + "|");
    renderer.present();
}

void GuiAppStart::loop() {
    shared_ptr<Gui> gui(Gui::getInstance());
    bool menuVisible = true;
    while (menuVisible) {
        Event e;
        render();
        while (gui->input().poll(e)) {
            switch (e.type) {
            case Event::Type::ButtonDown:
                if (e.button == Button::Cross) {
                    result = true;
                    menuVisible = false;
                };
                if (e.button == Button::Circle) {
                    result = false;
                    menuVisible = false;
                };
                break;

            case Event::Type::DpadDown: /* Handle Joystick Motion */
            case Event::Type::DpadUp:
                if (totalLines != 0) {
                    if (gui->input().dpadUp()) {
                        scrolling = -1;
                    }
                    if (gui->input().dpadDown()) {

                        scrolling = 1;
                    }
                    if (gui->input().dpadCentered()) {
                        scrolling = 0;
                    }
                }
                break;
            default:
                break;
            }
        }
    }
}
