//
// Created by screemer on 2019-01-24.
//

#include "gui_game_manager_menu.h"
#include <algorithm>
#include <string>
#include <iostream>
#include "gui_game_editor_menu.h"
#include "../screens/gui_confirm.h"
#include "../../app.h"
#include <ableem/engine/log.h>

using namespace std;

//*******************************
// GuiManager::init
//*******************************
void GuiManager::init() {
    useSmallerFont = true;
    GuiMenuBase::init(); // call the base class init()
    // the rows sit to the right of the preview pane: title, then the folder, elided to what is left
    xoffset_L = PreviewWidth;
    xoffset_R = PreviewWidth + 420;
    selectionBoxXOffset = PreviewWidth;
    previewFor = -1;

    // init() runs again after the editor closes and after a delete: the rows are rebuilt, not appended
    lines.clear();
    psGames.clear();
    psGames = PsGame::fromRecords(app.library().usbGames().loadUsbGames()); // Create list of games
    sort(psGames.begin(), psGames.end(), sortByTitle);                      // sort by title
    for (auto &psGame : psGames) {
        // left column              right column
        // "title"                  "path"
        string path = DirEntry::removeSeparatorFromEndOfPath(psGame->folder);
        path = DirEntry::removeGamesPathFromFrontOfPath(path);
        int panelRight = gui->text().getOpscreenRectOfTheme().x + gui->text().getOpscreenRectOfTheme().w - 20;
        int pathWidth = panelRight - (gui->text().getOpscreenRectOfTheme().x + 10 + xoffset_R);
        lines.emplace_back(gui->text().elide(font, psGame->title, 400), gui->text().elide(font, path, pathWidth));
    }
}

//*******************************
// GuiManager::render
//*******************************
void GuiManager::render() {
    renderer.clear();
    gui->renderBackground();
    gui->renderTextBar();
    yoffset = gui->renderHeader(getTitle());

    gui->renderFreeSpace(); // this is why this menu's render is special instead of using the base class

    renderLines();
    renderSelectionBox();
    renderPreview();

    gui->renderStatus(getStatusLine());
    renderer.present();
}

//*******************************
// GuiManager::renderPreview
//*******************************
void GuiManager::renderPreview() {
    if (selected < 0 || selected >= static_cast<int>(psGames.size()))
        return;
    if (previewFor != selected) {
        previewFor = selected;
        const PsGame &game = *psGames[selected];
        previewCover = ableem::Texture();
        previewSnap = ableem::Texture();
        // the same chain as the carousel's: the PNG next to the game, the cached thumbnail, a fresh look
        string cover = game.folder + sep + game.base + ".png";
        if (!DirEntry::exists(cover))
            cover = game.coverPath;
        if (cover.empty() || !DirEntry::exists(cover))
            cover =
                app.thumbnails().findBoxArt(ableem::ThumbnailLookup::PlayStationDbName, game.title, game.recordName);
        if (cover.empty())
            cover = Env::getWorkingPath() + sep + "default.png";
        previewCover = ableem::Texture::loadFile(renderer, cover);
        string snap = game.snapPath;
        if (snap.empty() || !DirEntry::exists(snap))
            snap = app.thumbnails().findSnap(ableem::ThumbnailLookup::PlayStationDbName, game.title,
                                             game.folder + sep + game.base, game.recordName);
        if (!snap.empty())
            previewSnap = ableem::Texture::loadFile(renderer, snap);
    }

    // the cover where the editor's is, the screenshot under it, both inside the pane's width
    ableem::Rect rect;
    rect.x = app.theme().classic().editorCover.x;
    rect.y = app.theme().classic().editorCover.y;
    rect.w = 226;
    rect.h = 226;
    if (previewCover.valid())
        renderer.copy(previewCover, nullptr, &rect);
    if (previewSnap.valid()) {
        ableem::Size s = previewSnap.size();
        ableem::Rect snapRect;
        snapRect.w = 226;
        snapRect.h = s.w > 0 ? 226 * s.h / s.w : 170; // aspect-fit to the cover's width
        if (snapRect.h > 190)
            snapRect.h = 190;
        snapRect.x = rect.x;
        snapRect.y = rect.y + rect.h + 10;
        renderer.copy(previewSnap, nullptr, &snapRect);
    }
}

//*******************************
// GuiManager::getTitle
//*******************************
std::string GuiManager::getTitle() {
    return _("Game manager - Select game");
}

//*******************************
// GuiManager::getStatusLine
//*******************************
string GuiManager::getStatusLine() {
    return _("Game") + " " + to_string(selected + 1) + "/" + to_string(psGames.size()) + "    |@L1|/|@R1| " +
           _("Page") + "   |@X| " + _("Select") + "  |@S| " + _("Delete game") + "  |@T| " + _("Flush covers") +
           " |@O| " + _("Back") + " |";
}

//*******************************
// GuiManager::doCircle_Pressed
//*******************************
void GuiManager::doCircle_Pressed() {
    app.audio().cancel.play();
    if (changes) {
        app.scans().requestScan();
    }
    menuVisible = false;
}

//*******************************
// GuiManager::doSquare_Pressed
//*******************************
void GuiManager::doSquare_Pressed() {
    app.audio().cursor.play();
    auto game = psGames[selected];
    int gameId = game->gameId;
    string gameName = game->title;
    string gameSaveStateFolder = game->ssFolder;
    GuiConfirm confirm(*gui);
    confirm.label = _("Are you sure you want to delete") + " " + gameName + "?";
    confirm.show();
    bool delGame = confirm.result;

    if (delGame) {
        PLOG_INFO << "Trying to delete " << gameName;
        gui->beginBusy(_("Please wait ... deleting") + " " + gameName, [this]() { render(); });
        auto result = app.gameCatalog().deleteUsbGame(*game);
        gui->endBusy();
        if (result.removed) {
            // the !SaveStates folder can be shared, so it is only offered when nothing else uses it
            if (result.saveStateFolderIsNowUnused) {
                GuiConfirm confirm(*gui);
                confirm.label = _("Delete !SaveState folder for game") + " " + gameName + "?";
                confirm.show();
                if (confirm.result)
                    app.gameCatalog().removeSaveStateFolder(result.saveStateFolder);
            }
        } else {
            gui->renderStatus(_("Failed to delete") + " " + gameName);
        }
    } else {
        PLOG_ERROR << "Failed to delete " << gameName;
        gui->renderStatus(_("Failed to delete") + " " + gameName);
    }
    app.scans().requestScan(); // in order for the sub dir hierarchy to be fixed we have to do a rescan
    // menuVisible = false;
    init();                                             // refresh games list and menu item count
    if (selected >= static_cast<int>(psGames.size())) { // the last game went: the cursor cannot stay past the end
        selected = psGames.empty() ? 0 : psGames.size() - 1;
        firstVisibleIndex = std::max(0, selected - maxVisible + 1);
        lastVisibleIndex = firstVisibleIndex + maxVisible - 1;
    }
    render();
}

//*******************************
// GuiManager::doTriangle_Pressed
//*******************************
void GuiManager::doTriangle_Pressed() {
    app.audio().cursor.play();
    GuiConfirm confirm(*gui);
    confirm.label = _("Are you sure you want to flush all covers?");
    confirm.show();
    bool delCovers = confirm.result;

    if (delCovers) {
        PLOG_INFO << "Trying to delete covers";
        gui->beginBusy(_("Please wait ... deleting covers..."), [this]() { render(); });
        PLOG_INFO << "Flushed " << app.gameCatalog().flushAllCovers() << " covers";
        gui->endBusy();

        app.scans().requestScan();
        menuVisible = false;
    } else {
        render();
    }
}

//*******************************
// GuiManager::doCross_Pressed
//*******************************
void GuiManager::doCross_Pressed() {
    app.audio().cursor.play();
    if (!psGames.empty()) {
        string selectedGameFolder = psGames[selected]->folder;
        {
            GuiEditor editor(*gui);
            editor.gameData = psGames[selected];
            editor.show();
            if (editor.changes) {
                changes = true;
            }
        }
        selected = 0;
        firstVisibleIndex = 0;
        lastVisibleIndex = firstVisibleIndex + maxVisible - 1;

        init();
        int pos = 0;
        for (const auto &psGame : psGames) {
            if (psGame->folder == selectedGameFolder) {
                selected = pos;
                firstVisibleIndex = pos;
                lastVisibleIndex = firstVisibleIndex + maxVisible - 1;
            }
            pos++;
        }
        render();
    }
}
