//
// GuiEditorRA: the game editor for a RetroArch game - the one thing it can set is the light-gun flag.
//
#include "gui_game_editor_ra_menu.h"
#include "../gui.h"
#include "../../app.h"
#include "../../core/services/environment.h"

using namespace std;

#define OPT_LIGHTGUN 4

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
    int line = 0;
    gui->renderBackground();
    gui->renderTextBar();
    int yoffset = gui->renderLogo(true);

    gui->text().renderTextLine("-=" + gameData->title + "=-", line++, yoffset, XALIGN_CENTER);
    gui->text().renderTextLine(_("File:") + " " + DirEntry::getFileNameFromPath(gameData->image_path), line++, yoffset,
                               XALIGN_CENTER);
    gui->text().renderTextLine(_("Core:") + " " + gameData->core_name, line++, yoffset, XALIGN_CENTER);
    line++;

    gui->text().renderTextLineOptions(
        _("Lightgun Game:") + (app.lightguns().isLightgun(*gameData) ? string("|@Check|") : string("|@Uncheck|")),
        OPT_LIGHTGUN, yoffset, XALIGN_LEFT, 300);
    gui->text().renderSelectionBox(OPT_LIGHTGUN, yoffset, 300);

    gui->renderStatus("|@O| " + _("Go back") + "|");

    ableem::Rect rect;
    rect.x = app.theme().classic().editorCover.x;
    rect.y = app.theme().classic().editorCover.y;
    rect.w = 226;
    rect.h = 226;
    renderer.copy(cover, nullptr, &rect);

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
