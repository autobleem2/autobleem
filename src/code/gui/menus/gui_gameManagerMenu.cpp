//
// Created by screemer on 2019-01-24.
//

#include "gui_gameManagerMenu.h"
#include <string>
#include <iostream>
#include "gui_gameEditorMenu.h"
#include "../gui_confirm.h"
#include "../../lang.h"
#include <ftw.h>
#include "../../engine/scanner.h"

using namespace std;

//*******************************
// GuiManager::init
//*******************************
void GuiManager::init() {
    useSmallerFont = true;
    GuiMenuBase::init();    // call the base class init()

    psGames.clear();
    psGames = PsGame::fromRecords(app.library().usbGames().loadUsbGames());    // Create list of games
    sort(psGames.begin(), psGames.end(), sortByTitle);  // sort by title
    for (int i = 0; i < psGames.size(); ++i) {
        // left column              right column
        // "title"                  "path"
        string path = DirEntry::removeSeparatorFromEndOfPath(psGames[i]->folder);
        path = DirEntry::removeGamesPathFromFrontOfPath(path);
        lines.emplace_back(TwoColumnsOfText(psGames[i]->title, path));
    }
}

//*******************************
// GuiManager::render
//*******************************
void GuiManager::render()
{
    renderer.clear();
    gui->renderBackground();
    gui->renderTextBar();
    yoffset = gui->renderLogo(true);

    gui->renderFreeSpace();     // this is why this menu's render is special instead of using the base class

    gui->renderTextLine(getTitle(), 0, yoffset, XALIGN_CENTER);

    renderLines();
    renderSelectionBox();

    gui->renderStatus(getStatusLine());
    renderer.present();
}

//*******************************
// GuiManager::getTitle
//*******************************
std::string GuiManager::getTitle() {
    return "-=" + _("Game manager - Select game") + "=-";
}

//*******************************
// GuiManager::getStatusLine
//*******************************
string GuiManager::getStatusLine() {
    return _("Game") + " " + to_string(selected + 1) + "/" + to_string(psGames.size()) +
           "    |@L1|/|@R1| " + _("Page") +
           "   |@X| " + _("Select") +
           "  |@S| " + _("Delete Game") +
           "  |@T| " + _("Flush covers") +
           " |@O| " + _("Close") + " |";
}

//*******************************
// GuiManager::flushCovers
//*******************************
int GuiManager::flushCovers(const char *file, const struct stat* /*sb*/, int /*flag*/, struct FTW* /*s*/)
{
    int retval = 0;

    if (DirEntry::getFileExtension(file)=="png")
    {
        remove(file);
    }

    return retval;
}

//*******************************
// GuiManager::doCircle_Pressed
//*******************************
void GuiManager::doCircle_Pressed() {
    gui->cancel.play();
    if (changes)
    {
        app.session().forceScan = true;
    }
    menuVisible = false;
}

//*******************************
// GuiManager::doSquare_Pressed
//*******************************
void GuiManager::doSquare_Pressed() {
    gui->cursor.play();
    auto game = psGames[selected];
    int gameId = game->gameId;
    string gameName = game->title;
    string gameSaveStateFolder = game->ssFolder;
    GuiConfirm confirm(*gui);
    confirm.label = _("Are you sure you want to delete") + " " + gameName + "?";
    confirm.show();
    bool delGame = confirm.result;

    if (delGame) {
        cout << "Trying to delete " << gameName << endl;
        gui->renderStatus(_("Please wait ... deleting") + " " + gameName);
        bool success = app.library().usbGames().deleteGame(gameId);
        if (success) {
            success = DirEntry::removeDirAndContents(game->folder);
            if (success) {
                PsGames currentGames = PsGame::fromRecords(app.library().usbGames().loadUsbGames());
                int numberOfGamesRemainingWithSameSaveState = count_if(begin(currentGames), end(currentGames),
                                                                       [&](const PsGamePtr &g) {
                                                                           return g->ssFolder == gameSaveStateFolder;
                                                                       });
                if (numberOfGamesRemainingWithSameSaveState == 0) {
                    GuiConfirm confirm(*gui);
                    confirm.label = _("Delete !SaveState folder for game") + " " + gameName + "?";
                    confirm.show();
                    bool delSSFolder = confirm.result;
                    if (delSSFolder)
                        DirEntry::removeDirAndContents(gameSaveStateFolder);
                }
            }
        } else {
            cout << "Failed to delete directory " << game->folder << endl;
            gui->renderStatus(_("Failed to delete") + " " + gameName);
        }
    } else {
        cout << "Failed to delete " << gameName << endl;
        gui->renderStatus(_("Failed to delete") + " " + gameName);
    }
    app.session().forceScan = true;  // in order for the sub dir hierarchy to be fixed we have to do a rescan
    //menuVisible = false;
    init(); // refresh games list and menu item count
    render();
}

//*******************************
// GuiManager::doTriangle_Pressed
//*******************************
void GuiManager::doTriangle_Pressed() {
    gui->cursor.play();
    GuiConfirm confirm(*gui);
    confirm.label = _("Are you sure you want to flush all covers?");
    confirm.show();
    bool delCovers = confirm.result;

    if (delCovers)
    {
        cout << "Trying to delete covers" << endl;
        gui->renderStatus(_("Please wait ... deleting covers..."));

        int errors = 0;
        int flags = FTW_DEPTH | FTW_PHYS | FTW_CHDIR;
        //cout << Env::getPathToGamesDir() << endl;
        if (nftw(DirEntry::fixPath(Env::getPathToGamesDir()).c_str(), flushCovers, 1, flags) != 0) {
            errors++;
        }

        app.session().forceScan = true;
        menuVisible = false;
    } else {
        render();
    }
}

//*******************************
// GuiManager::doCross_Pressed
//*******************************
void GuiManager::doCross_Pressed() {
    gui->cursor.play();
    if (!psGames.empty())
    {
        string selectedGameFolder = psGames[selected]->folder;
        {
            GuiEditor editor(*gui);
            editor.gameData = psGames[selected];
            editor.gameFolder = selectedGameFolder;
            editor.gameIni.load(selectedGameFolder + sep + GAME_INI);
            string folderNoLast = DirEntry::removeSeparatorFromEndOfPath(selectedGameFolder);
            // change "/media/Games/Racing/Driver 2" to "Driver 2"
            editor.gameIni.entry = DirEntry::getFileNameFromPath(folderNoLast);
            editor.show();
            if (editor.changes)
            {
                changes = true;
            }
        }
        selected = 0;
        firstVisibleIndex = 0;
        lastVisibleIndex = firstVisibleIndex + maxVisible - 1;

        init();
        int pos = 0;
        for (const auto & psGame : psGames)
        {
            if (psGame->folder == selectedGameFolder)
            {
                selected = pos;
                firstVisibleIndex = pos;
                lastVisibleIndex = firstVisibleIndex + maxVisible - 1;
            }
            pos++;
        }
        render();
    }
}
