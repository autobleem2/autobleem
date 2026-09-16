//
// Created by screemer on 2018-12-19.
//

#include "gui.h"
#include "gui_splash.h"
#include "../app.h"
#include <unistd.h>
#include <iostream>
#include <iomanip>
#include <cstring>
#include <cassert>

using namespace std;
using ableem::Rect;
using ableem::Size;
using ableem::Color;
using ableem::Texture;
using ableem::Event;
using ableem::Button;
//********************
// Gui::Gui
//********************
Gui::Gui() : assets_(renderer(), App::get().theme(), App::get().config()),
             text_(renderer(), App::get().theme(), assets_.themeFont, assets_.buttonTextureMap) {
    input().probePads();
}

//*******************************
// Gui::splash
//*******************************
void Gui::splash(const string &message) {
    shared_ptr<Gui> gui(Gui::getInstance());
    gui->drawText(message);
}


//*******************************
// Gui::loadAssets
//*******************************
void Gui::loadAssets(bool reloadMusic) {
    assets_.load();
    App::get().audio().loadTheme(reloadMusic);
}

//*******************************
// Gui::hideMouseCursor
//*******************************
void Gui::hideMouseCursor() {
    if (!platform().isDevHost()) {
        platform().hideAndGrabCursor();
    }
}

//*******************************
// Gui::criticalException
//*******************************
void Gui::criticalException(const string &text) {
    drawText(text);
    while (true) {
        Event e;
        while (input().poll(e)) {
            if (e.type == Event::Type::Quit)
                return;
            else if (e.type == Event::Type::KeyUp && e.key == ableem::Key::Escape)
                return;

            if (e.type == Event::Type::ButtonDown) {
                return;
            }
        }
    }
}

//*******************************
// Gui::display
//*******************************
void Gui::display(bool resume) {
    cout << platform().versionString() << endl;

    platform().setScaleQuality(2);

    loadAssets();

    if (!resume) {
        GuiSplash splashScreen(*this);
        splashScreen.show();
        hideMouseCursor();
    } else {
        App::get().session().resumingGui = true;
    }
}

//*******************************
// Gui::finish
//*******************************
void Gui::finish() {
    App::get().audio().shutdown();
    assets_.backgroundImg = Texture();
}


//*******************************
// Gui::renderFreeSpace
//*******************************
void Gui::renderFreeSpace() {
    int x = atoi(App::get().theme().data.values["fsposx"].c_str());
    int y = atoi(App::get().theme().data.values["fsposy"].c_str());
    text_.renderText(assets_.themeFont, _("Free space") + " : " + System::getAvailableSpace(), x, y);
}

//*******************************
// Gui::renderBackground
//*******************************
void Gui::renderBackground() {
    renderer().setDrawColor(Color(0x00, 0x00, 0x00, 0x00));
    renderer().clear();
    renderer().copy(assets_.backgroundImg, nullptr, &assets_.backgroundRect);
}

//*******************************
// Gui::renderLogo
//*******************************
int Gui::renderLogo(bool small) {
    if (!small) {
        renderer().copy(assets_.logo, nullptr, &assets_.logoRect);
        return 0;
    } else {
        Rect rect;
        rect.x = atoi(App::get().theme().data.values["opscreenx"].c_str());
        rect.y = atoi(App::get().theme().data.values["opscreeny"].c_str());
        rect.w = assets_.logoRect.w / 3;
        rect.h = assets_.logoRect.h / 3;
        renderer().copy(assets_.logo, nullptr, &rect);
        return rect.y + rect.h;
    }
}

//*******************************
// Gui::renderStatus
//*******************************
void Gui::renderStatus(const string &text, int posy) {
    string bg = App::get().theme().data.values["text_bg"];

    renderer().setDrawColor(Color(TextRenderer::getR(bg), TextRenderer::getG(bg), TextRenderer::getB(bg),
                                  atoi(App::get().theme().data.values["textalpha"].c_str())));
    renderer().setBlendMode(ableem::BlendMode::Blend);
    Rect rect = text_.getTextRectOfTheme();
    renderer().fillRect(rect);

    int y = atoi(App::get().theme().data.values["ttop"].c_str());
    if (posy!=-1)
        y=posy; // override the bottom status y position.  so far this has never been used.

    text_.renderText(assets_.themeFont, text, 0, y, XALIGN_CENTER);
}

//*******************************
// Gui::renderTextBar
//*******************************
void Gui::renderTextBar() {
    string bg = App::get().theme().data.values["main_bg"];
    renderer().setDrawColor(Color(TextRenderer::getR(bg), TextRenderer::getG(bg), TextRenderer::getB(bg),
                                  atoi(App::get().theme().data.values["mainalpha"].c_str())));
    renderer().setBlendMode(ableem::BlendMode::Blend);

    Rect rect2 = text_.getOpscreenRectOfTheme();

    renderer().fillRect(rect2);
}

//*******************************
// Gui::drawText
//*******************************
void Gui::drawText(const string &text, const string &topLine) {
    renderBackground();
    renderLogo(false);
    renderStatus(text);
    renderStatus(topLine, 5);
    renderer().present();
}
