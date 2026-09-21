//
// Created by screemer on 2019-01-24.
//

#include "gui_confirm.h"
#include "gui_about.h"
#include <string>
#include "../gui.h"
using namespace std;

//*******************************
// GuiConfirm::render
//*******************************
// A compact dialog in the shared look (PanelStyle), centred over the dimmed screen: the header, the
// question wrapped to the panel, the two hints
void GuiConfirm::render() {
    shared_ptr<Gui> gui(Gui::getInstance());
    gui->renderBackground();
    PanelStyle style = gui->panelStyle();
    style.dim(renderer);

    const int width = 800;
    Fonts &fonts = gui->assets().themeFonts;
    const ableem::Font &font = fonts[FONT_22_MED];
    // the question wrapped to the panel
    const int textWidth = width - 2 * (PanelStyle::RowInset + 8);
    const int textHeight = font.columnHeight(label, textWidth);
    const int height = PanelStyle::HeaderHeight + 12 + textHeight + 24 + PanelStyle::FooterHeight;
    ableem::Rect panel((SCREEN_WIDTH - width) / 2, (SCREEN_HEIGHT - height) / 2, width, height);
    style.sheet(renderer, panel);

    const TextRenderer::Shadow classicShadow = gui->text().shadow();
    TextRenderer::Shadow shadow;
    shadow.enabled = style.textShadow;
    gui->text().setShadow(shadow);

    int y = style.header(*gui, panel, _("Please confirm")) + 12;
    gui->text().renderWrappedText(font, label, panel.x + PanelStyle::RowInset + 8, y, textWidth, style.text);
    style.footer(*gui,
                 ableem::Rect(panel.x, panel.y + panel.h - PanelStyle::FooterHeight, panel.w, PanelStyle::FooterHeight),
                 {{{"X"}, _("Confirm")}, {{"O"}, _("Cancel")}}, "", false);

    gui->text().setShadow(classicShadow);
    renderer.present();
}

//*******************************
// GuiConfirm::loop
//*******************************
void GuiConfirm::loop() {
    shared_ptr<Gui> gui(Gui::getInstance());
    menuVisible = true;
    while (menuVisible) {
        Event e;
        while (gui->input().poll(e)) {
            // this is for pc Only
            if (e.type == Event::Type::Quit) {
                menuVisible = false;
            }

            switch (e.type) {
            case Event::Type::ButtonDown:
                if (e.button == Button::Cross) {
                    app.audio().cursor.play();
                    result = true;
                    menuVisible = false;
                };

                if (e.button == Button::Circle) {
                    app.audio().cancel.play();
                    result = false;
                    menuVisible = false;
                };
                break;

            case Event::Type::KeyDown:
                if (e.key == Key::Return) {
                    app.audio().cursor.play();
                    result = true;
                    menuVisible = false;
                }
                if (e.key == Key::Escape) {
                    app.audio().cancel.play();
                    result = false;
                    menuVisible = false;
                }
                break;
            default:
                break;
            }
        }
    }
}
