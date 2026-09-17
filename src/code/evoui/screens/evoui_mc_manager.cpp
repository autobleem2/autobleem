//
// Created by screemer on 2019-07-30.
//

#include "evoui_mc_manager.h"

#include <string>
#include <iostream>
#include "../../gui/gui.h"
#include "../../gui/screens/gui_confirm.h"
#include "../../gui/screens/gui_select_memcard.h"
#include "../../core/services/environment.h"

void GuiMcManager::init() {
    rightCardName_ori = rightCardName;
    cardPath_ori = card2path;
    loadAssets();
}

void GuiMcManager::loadAssets() {
    shared_ptr<Gui> gui(Gui::getInstance());
    mcGrid = ableem::Texture::loadFile(renderer, app.theme().launcher().memcardManager.grid);
    mcPencil = ableem::Texture::loadFile(renderer, app.theme().launcher().memcardManager.pencil);
    fontJIS = Fonts::openNewSharedCachedFont(Env::getWorkingPath() + sep + "japanese.ttf", 20, renderer);

    memcard1.reset(new CardEdit(renderer));
    memcard2.reset(new CardEdit(renderer));

    memcard1->load(card1path);
    memcard2->load(card2path);

    pencilPos.w = 42;
    pencilPos.h = 42;
    pencilPos.x = mc1XStart;
    pencilPos.y = 150;
    pencilColumn = 0;
    pencilRow = 0;
    pencilMemcard = 1;

}

void GuiMcManager::pencilDown() {
    if (pencilRow != 4) {
        pencilRow++;
    }
}

void GuiMcManager::pencilUp() {
    if (pencilRow != 0) {
        pencilRow--;
    }
}

void GuiMcManager::pencilLeft() {
    if (pencilColumn != 0) {
        pencilColumn--;
    } else {
        pencilColumn = 2;
        if (pencilMemcard == 1) pencilMemcard = 2; else pencilMemcard = 1;
    }
}

void GuiMcManager::pencilRight() {
    if (pencilColumn != 2) {
        pencilColumn++;
    } else {
        pencilColumn = 0;
        if (pencilMemcard == 1) pencilMemcard = 2; else pencilMemcard = 1;
    }


}

void GuiMcManager::renderPencil(int memcard, int col, int row) {
    const int pencilShiftX = 80;
    const int pencilShiftY = 80;
    if (memcard == 1) {
        pencilPos.x = mc1XStart + (col * pencilShiftX);
    }
    if (memcard == 2) {
        pencilPos.x = mc2XStart + (col * pencilShiftX);
    }
    pencilPos.y = mcYStart + (row * pencilShiftY);
    renderer.copy(mcPencil, nullptr, &pencilPos);
}

void GuiMcManager::trySave()
{
    if (changes)
    {
        GuiConfirm confirm(*gui);
        confirm.label = _("Do you want to save memcards data ?");
        confirm.show();
        if (confirm.result) {
            memcard1->save(card1path);
            memcard2->save(card2path);
            changes = false;
        }
        changes = false;
    }
}

void GuiMcManager::renderStatic() {
    shared_ptr<Gui> gui(Gui::getInstance());
    gui->renderBackground();
    gui->renderTextBar();
    gui->text().renderTextLine("-=" + _("Memory Card Manager") + "=-", 1, 1, XALIGN_CENTER);
    gui->renderStatus(
            "|@Start| " + _("Select Right Card") +
            " | |@Select| " + _("Defragment Card") +
            "   | " + "|@X| " + _("Reload Cards") +
            "   | " + "|@T| " + _("Delete") +
            " | " + "|@S| " + _("Copy") +
            " | " + "|@O| " + _("Go back") +
            "|");

    //Draw dot matrix image
    ableem::Rect input, output;
    ableem::Size gridSize = mcGrid.size();
    input.w = output.w = gridSize.w;
    input.h = output.h = gridSize.h;
    input.x = 0, input.y = 0;
    output.x = 80;
    output.y = 80;
    renderer.copy(mcGrid, &input, &output);
    output.x = 940;
    output.y = 80;
    renderer.copy(mcGrid, &input, &output);

}

void GuiMcManager::renderMemCardIcons(int memcard) {
    const int xStartMC1 = 80, xStartMC2 = 940, yStart = 80, xDecal = 10, yDecal = 10, xShift = 80, yShift = 80;
    ableem::Rect output;
    output.h = 75;
    output.w = 75;

    int start;
    CardEdit *currentCard;
    if (memcard == 1) {
        start = xStartMC1;
        currentCard = memcard1.get();
    }

    if (memcard == 2) {
        start = xStartMC2;
        currentCard = memcard2.get();
    }


    for (int i = 0; i < 15; i++) {
        int col = i % 3;
        int line = i / 3;
        int frame = 0;
        if ((pencilMemcard == memcard) && (pencilRow == line) && (pencilColumn == col)) {
            frame = animFrame;
        }
        output.x = start + (xShift * col) + xDecal;
        output.y = yStart + (yShift * line) + yDecal;
        if (currentCard->image().isUsed(i)) {
            renderer.copy(currentCard->icon(i, frame), nullptr, &output);
        }
    }
}

void GuiMcManager::renderMetaInfo() {
    shared_ptr<Gui> gui(Gui::getInstance());

    CardEdit *card;
    if (pencilMemcard == 1) {
        card = memcard1.get();

    } else {
        card = memcard2.get();
    }

    string title = card->title(pencilColumn + pencilRow * 3);
    string gameID = card->image().gameId(pencilColumn + pencilRow * 3);
    string pCode = card->image().productCode(pencilColumn + pencilRow * 3);

    string nextSlot = to_string(card->image().nextSlot(pencilColumn + pencilRow * 3));

    gui->text().renderTextLine(title, -500, 1, XALIGN_CENTER, true, fontJIS);
    gui->text().renderTextLine(gameID, 3, 1, XALIGN_CENTER, true);
    gui->text().renderTextLine(pCode, 4, 1, XALIGN_CENTER, true);



    gui->text().renderTextLine(leftCardName, -500, 1, XALIGN_LEFT, true);
    gui->text().renderTextLine(rightCardName, -500, 1, XALIGN_RIGHT, true);
}

void GuiMcManager::render() {
    shared_ptr<Gui> gui(Gui::getInstance());
    // render static elements
    renderStatic();
    //Draw Memcard images and meta info
    renderMemCardIcons(1);
    renderMemCardIcons(2);
    renderMetaInfo();

    //Draw the pencil
    renderPencil(pencilMemcard, pencilColumn, pencilRow);
    renderer.present();

}

void GuiMcManager::loop() {
    shared_ptr<Gui> gui(Gui::getInstance());
    bool menuVisible = true;
    while (menuVisible) {

        Event e;
        while (gui->input().poll(e)) {
            // this is for pc Only
            if (e.type == Event::Type::Quit) {
                menuVisible = false;
            }
            switch (e.type) {
                case Event::Type::ButtonDown:
                    if (e.button == Button::Circle) {
                        app.audio().cancel.play();
                        trySave();
                        menuVisible = false;
                    };
                    if (e.button == Button::Cross) {
                        app.audio().cursor.play();
                        trySave();
                        memcard1->load(card1path);
                        memcard2->load(card2path);
                        changes = false;
                    };
                    if (e.button == Button::Select) {
                        app.audio().cursor.play();
                        unique_ptr<CardEdit> newCard(new CardEdit(renderer));
                        CardEdit *src = (pencilMemcard == 1) ? memcard1.get() : memcard2.get();
                        int last = 0;
                        for (int slot = 0; slot < 15; slot++) {
                            if (!src->image().isTop(slot))
                            {
                                continue;
                            }
                            int gameSize = src->image().gameSlots(slot).size();
                            vector<int> destSlots = newCard->image().findEmptySlots(gameSize);

                            if (destSlots.size() > 0)
                            {
                                app.audio().cursor.play();
                                int exportSize = src->image().exportSize(slot);
                                vector<unsigned char> buffer(exportSize);
                                src->image().exportGame(slot,buffer.data());
                                newCard->importGame(buffer.data(),exportSize);
                                changes = true;

                            }
                        }
                        // the compacted card replaces the old one (which is deleted by the unique_ptr)
                        if (pencilMemcard == 1) {
                            memcard1 = std::move(newCard);
                        } else {
                            memcard2 = std::move(newCard);
                        }
                    }

                    if (e.button == Button::Start) {
                        app.audio().cursor.play();
                        trySave();
                        GuiSelectMemcard select(*gui);
                        select.listType=MC_MANAGER;
                        select.show();
                        if (select.selected!=-1)
                        {
                            if (select.selected==0) {
                                rightCardName = rightCardName_ori;
                                card2path = cardPath_ori;
                                memcard2->load(card2path);
                            } else
                            {
                                // this is custom
                                int cardNumCustom=atoi(select.cardSelected.substr(1,1).c_str());
                                string memcard = select.cardSelected.substr(4);
                                string cardPath =  Env::getPathToMemCardsDir() + sep + memcard  + "/card" + to_string(cardNumCustom) + ".mcd";

                                rightCardName = select.cardSelected;
                                card2path = cardPath;
                                cout << "Card:" << cardPath << endl;
                                memcard2->load(card2path);
                            }
                            changes = false;
                        }
                    }
                    if (e.button == Button::Triangle) {
                        CardEdit *card;
                        if (pencilMemcard == 1) {
                            card = memcard1.get();
                        } else {
                            card = memcard2.get();
                        }
                        int slot = pencilColumn + pencilRow * 3;
                        if (!card->image().isTop(slot)) {
                            app.audio().cancel.play();
                            continue;
                        }
                        if (card->image().isFree(slot)) {
                            app.audio().cursor.play();
                            continue;
                        }
                        app.audio().cursor.play();
                        card->deleteGame(slot);
                        changes=true;


                    };
                    if (e.button == Button::Square) {
                        CardEdit *src, *dest;
                        if (pencilMemcard == 1) {
                            src = memcard1.get();
                            dest = memcard2.get();
                        } else {
                            src = memcard2.get();
                            dest = memcard1.get();
                        }
                        int slot = pencilColumn + pencilRow * 3;
                        if (!src->image().isTop(slot)) {
                            app.audio().cancel.play();
                            continue;
                        }
                        if (src->image().isFree(slot)) {
                            app.audio().cursor.play();
                            continue;
                        }

                        int gameSize = src->image().gameSlots(slot).size();
                        vector<int> destSlots = dest->image().findEmptySlots(gameSize);

                        if (destSlots.size() > 0)
                        {
                            app.audio().cursor.play();
                            int exportSize = src->image().exportSize(slot);
                            vector<unsigned char> buffer(exportSize);
                            src->image().exportGame(slot,buffer.data());
                            dest->importGame(buffer.data(),exportSize);
                            changes = true;
                        } else
                        {
                            app.audio().cancel.play();
                        }
                    };
                    break;

                case Event::Type::DpadDown:  /* Handle Joystick Motion */
                case Event::Type::DpadUp:
                    if (gui->input().dpadCentered()) {

                    }
                    if (gui->input().dpadLeft()) {
                        app.audio().cursor.play();
                        pencilLeft();
                    }
                    if (gui->input().dpadRight()) {
                        app.audio().cursor.play();
                        pencilRight();
                    }
                    if (gui->input().dpadUp()) {
                        app.audio().cursor.play();
                        pencilUp();
                    }
                    if (gui->input().dpadDown()) {
                        app.audio().cursor.play();
                        pencilDown();
                    }
                    break;
                default:
                    break;
            }
        }
        counter++;
        if (counter > 5) {
            animFrame++;
            if (animFrame > 2) animFrame = 0;
            counter = 0;
        }
        render();
    }
}

