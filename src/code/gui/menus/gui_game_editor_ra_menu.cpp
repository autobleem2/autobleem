//
// GuiEditorRA: the game editor for a RetroArch game - the one thing it can set is the light-gun flag.
//
#include "gui_game_editor_ra_menu.h"
#include "../gui.h"
#include "../../app.h"
#include "../../core/services/environment.h"

using namespace std;

#define OPT_LIGHTGUN 1

//*******************************
// GuiEditorRA::init
//*******************************
void GuiEditorRA::init() {
    string path = app.thumbnails().findBoxArt(gameData->db_name, gameData->title);
    if (path.empty())
        path = Env::getWorkingPath() + sep + "evoimg/ra-cover.png";
    cover = ableem::Texture::loadFile(renderer, path);
}

//*******************************
// GuiEditorRA::render
//*******************************
void GuiEditorRA::render() {
    shared_ptr<Gui> gui(Gui::getInstance());
    gui->renderBackground();
    gui->renderTextBar();
    int yoffset = gui->renderHeader(gui->text().elide(gui->assets().themeFonts[FONT_28_BOLD], gameData->title,
                                                      gui->classicPanel().w - 2 * PanelStyle::RowInset));

    pane.cover = cover;
    pane.facts.clear();
    pane.facts.emplace_back(_("File:"), DirEntry::getFileNameFromPath(gameData->image_path));
    pane.facts.emplace_back(_("Core:"), gameData->core_name);
    if (!gameData->publisher.empty())
        pane.facts.emplace_back(_("Published by:"), gameData->publisher);
    if (gameData->year > 0)
        pane.facts.emplace_back(_("Year:"), to_string(gameData->year));
    pane.render(*gui);

    const int right = GameDetailPane::rowsRight(*gui);
    gui->text().renderLabelBox(0, yoffset, right);
    gui->text().renderTextLine(_("Game"), 0, yoffset, XALIGN_LEFT);
    gui->text().renderSelectionBox(OPT_LIGHTGUN, yoffset, 0, ableem::Font(), right);
    gui->text().renderTextLineOptions(
        _("Lightgun Game:") + (app.lightguns().isLightgun(*gameData) ? string("|@Check|") : string("|@Uncheck|")),
        OPT_LIGHTGUN, yoffset, XALIGN_LEFT, 0, right);

    gui->renderStatus("|@O| " + _("Back") + "|");
    renderer.present();
}

//*******************************
// GuiEditorRA::loop
//*******************************
void GuiEditorRA::loop() {
    shared_ptr<Gui> gui(Gui::getInstance());
    menuVisible = true;
    while (menuVisible) {
        Event e;
        while (gui->input().poll(e)) {
            if (e.type == Event::Type::Quit) {
                menuVisible = false;
            }
            switch (e.type) {
            case Event::Type::DpadDown:
            case Event::Type::DpadUp:
                if (gui->input().dpadRight() || gui->input().dpadLeft()) {
                    app.audio().cursor.play();
                    bool on = gui->input().dpadRight();
                    if (on != app.lightguns().isLightgun(*gameData)) {
                        app.lightguns().setRetroArchLightgun(*gameData, on);
                        changed = true;
                    }
                    render();
                }
                break;
            case Event::Type::ButtonDown:
                if (e.button == Button::Circle) {
                    app.audio().cancel.play();
                    menuVisible = false;
                }
                break;
            default:
                break;
            }
        }
    }
}
