//
// Created by screemer on 30.04.2020.
//

#include "gui_padconfig.h"
#include "../gui_keyboard.h"
#include "../../engine/gcdbprocessor.h"
#include <iostream>


void GuiPadconfig::nextController() {
    int numJoys = SDL_NumJoysticks();
    int next_idx = 0;
    int last_idx = joyinfo.index;
    if (numJoys == 0) {
        joyinfo.close();
        joyinfo.open(-1);
        next_idx = -1;
    } else {
        next_idx = last_idx += 1;
        if (last_idx >= numJoys) {
            next_idx = 0;
        }
        joyinfo.close();
        joyinfo.open(next_idx);
    }


}

void GuiPadconfig::initScanningElements() {
    scanningElements.clear();
    InputMapElement *element = new InputMapElement("a", MAP_SCAN_DIGITAL, 0, -1, -1);
    scanningElements.emplace_back(element);
    element = new InputMapElement("b", MAP_SCAN_DIGITAL, 1, -1, -1);
    scanningElements.emplace_back(element);
    element = new InputMapElement("x", MAP_SCAN_DIGITAL, 2, -1, -1);
    scanningElements.emplace_back(element);
    element = new InputMapElement("y", MAP_SCAN_DIGITAL, 3, -1, -1);
    scanningElements.emplace_back(element);
    element = new InputMapElement("back", MAP_SCAN_DIGITAL, 4, -1, -1);
    scanningElements.emplace_back(element);
    element = new InputMapElement("guide", MAP_SCAN_DIGITAL, 5, -1, -1);
    scanningElements.emplace_back(element);
    element = new InputMapElement("start", MAP_SCAN_DIGITAL, 6, -1, -1);
    scanningElements.emplace_back(element);
    element = new InputMapElement("leftstick", MAP_SCAN_DIGITAL, 7, -1, -1);
    scanningElements.emplace_back(element);
    element = new InputMapElement("rightstick", MAP_SCAN_DIGITAL, 8, -1, -1);
    scanningElements.emplace_back(element);
    element = new InputMapElement("leftshoulder", MAP_SCAN_DIGITAL, 9, -1, -1);
    scanningElements.emplace_back(element);
    element = new InputMapElement("rightshoulder", MAP_SCAN_DIGITAL, 10, -1, -1);
    scanningElements.emplace_back(element);
    element = new InputMapElement("lefttrigger", MAP_SCAN_TRIGGER, -1, 4, false);
    scanningElements.emplace_back(element);
    element = new InputMapElement("righttrigger", MAP_SCAN_TRIGGER, -1, 5, false);
    scanningElements.emplace_back(element);
    element = new InputMapElement("dpup", MAP_SCAN_DIGITAL, 11, -1, -1);
    scanningElements.emplace_back(element);
    element = new InputMapElement("dpdown", MAP_SCAN_DIGITAL, 12, -1, -1);
    scanningElements.emplace_back(element);
    element = new InputMapElement("dpleft", MAP_SCAN_DIGITAL, 13, -1, -1);
    scanningElements.emplace_back(element);
    element = new InputMapElement("dpright", MAP_SCAN_DIGITAL, 14, -1, -1);
    scanningElements.emplace_back(element);
    element = new InputMapElement("-leftx", MAP_SCAN_ANALOG, -1, 0, true);
    scanningElements.emplace_back(element);
    element = new InputMapElement("+leftx", MAP_SCAN_ANALOG, -1, 0, false);
    scanningElements.emplace_back(element);
    element = new InputMapElement("-lefty", MAP_SCAN_ANALOG, -1, 1, true);
    scanningElements.emplace_back(element);
    element = new InputMapElement("+lefty", MAP_SCAN_ANALOG, -1, 1, false);
    scanningElements.emplace_back(element);
    element = new InputMapElement("-rightx", MAP_SCAN_ANALOG, -1, 2, true);
    scanningElements.emplace_back(element);
    element = new InputMapElement("+rightx", MAP_SCAN_ANALOG, -1, 2, false);
    scanningElements.emplace_back(element);
    element = new InputMapElement("-righty", MAP_SCAN_ANALOG, -1, 3, true);
    scanningElements.emplace_back(element);
    element = new InputMapElement("+righty", MAP_SCAN_ANALOG, -1, 3, false);
    scanningElements.emplace_back(element);
};

void GuiPadconfig::clearScanningMapping() {
    for (InputMapElement *el:scanningElements)
        el->gcbValue = "";

};

void GuiPadconfig::freeScanningMapping() {
    for (InputMapElement *el:scanningElements)
        delete (el);
    for (InputMapElement *el:scanningElementsFinal)
        delete (el);
    scanningElements.clear();
    scanningElementsFinal.clear();

};

void GuiPadconfig::init() {

    if (SDL_NumJoysticks() == 0) {
        gui->drawText(_("NO GAMEPADS CONNECTED"));
        SDL_Delay(1000);
    } else {
        joyinfo.open(0);
    }
    initScanningElements();


}

void GuiPadconfig::generateFinalElements() {
    string leftxm = scanningElements[17]->gcbValue;
    string leftxp = scanningElements[18]->gcbValue;
    string leftym = scanningElements[19]->gcbValue;
    string leftyp = scanningElements[20]->gcbValue;
    string rightxm = scanningElements[21]->gcbValue;
    string rightxp = scanningElements[22]->gcbValue;
    string rightym = scanningElements[23]->gcbValue;
    string rightyp = scanningElements[24]->gcbValue;
    // clean analogs if wrongly mapped
    if (leftxm == "" || leftxp == "") {
        scanningElements[17]->gcbValue = "";
        scanningElements[18]->gcbValue = "";
    }
    if (leftym == "" || leftyp == "") {
        scanningElements[19]->gcbValue = "";
        scanningElements[20]->gcbValue = "";
    }
    if (leftxm == "" || leftxp == "") {
        scanningElements[21]->gcbValue = "";
        scanningElements[22]->gcbValue = "";
    }
    if (leftym == "" || leftyp == "") {
        scanningElements[23]->gcbValue = "";
        scanningElements[24]->gcbValue = "";
    }

    leftxm = scanningElements[17]->gcbValue;
    leftxp = scanningElements[18]->gcbValue;
    leftym = scanningElements[19]->gcbValue;
    leftyp = scanningElements[20]->gcbValue;
    rightxm = scanningElements[21]->gcbValue;
    rightxp = scanningElements[22]->gcbValue;
    rightym = scanningElements[23]->gcbValue;
    rightyp = scanningElements[24]->gcbValue;



    // check leftx
    if (!leftxp.empty() && !leftxm.empty())
        if ((leftxp[0] == '-') || (leftxm[0] == '+') || (leftxp[0] == '+') || (leftxm[0] == '-')) {
            string axisnump = leftxp.substr(2);
            string axisnumm = leftxm.substr(2);
            cout << axisnumm << " " << axisnump << endl;
            if (axisnump == axisnumm) {
                InputMapElement *imp = new InputMapElement("leftx", MAP_SCAN_ANALOG, -1, -1, false);
                imp->gcbValue = "a" + axisnumm;
                if ((leftxp[0] == '-') && (leftxm[0] == '+')) {
                    imp->gcbValue += "~";
                }
                scanningElements[17]->gcbValue = "";
                scanningElements[18]->gcbValue = "";
                scanningElements.emplace_back(imp);
            }
        }
    // check lefty
    if (!leftyp.empty() && !leftym.empty())
        if ((leftyp[0] == '-') || (leftym[0] == '+') || (leftyp[0] == '+') || (leftym[0] == '-')) {
            string axisnump = leftyp.substr(2);
            string axisnumm = leftym.substr(2);
            cout << axisnumm << " " << axisnump << endl;
            if (axisnump == axisnumm) {
                InputMapElement *imp = new InputMapElement("lefty", MAP_SCAN_ANALOG, -1, -1, false);
                imp->gcbValue = "a" + axisnumm;
                if ((leftyp[0] == '-') && (leftym[0] == '+')) {
                    imp->gcbValue += "~";
                }
                scanningElements[19]->gcbValue = "";
                scanningElements[20]->gcbValue = "";
                scanningElements.emplace_back(imp);
            }
        }
    if (!rightxp.empty() && !rightxm.empty())
        if ((rightxp[0] == '-') || (rightxm[0] == '+') || (rightxp[0] == '+') || (rightxm[0] == '-')) {
            string axisnump = rightxp.substr(2);
            string axisnumm = rightxm.substr(2);
            cout << axisnumm << " " << axisnump << endl;
            if (axisnump == axisnumm) {
                InputMapElement *imp = new InputMapElement("rightx", MAP_SCAN_ANALOG, -1, -1, false);
                imp->gcbValue = "a" + axisnumm;
                if ((rightxp[0] == '-') && (rightxm[0] == '+')) {
                    imp->gcbValue += "~";
                }
                scanningElements[21]->gcbValue = "";
                scanningElements[22]->gcbValue = "";
                scanningElements.emplace_back(imp);
            }
        }
    if (!rightyp.empty() && !rightym.empty())
        if ((rightyp[0] == '-') || (rightym[0] == '+') || (rightyp[0] == '+') || (rightym[0] == '-')) {
            string axisnump = rightyp.substr(2);
            string axisnumm = rightym.substr(2);
            cout << axisnumm << " " << axisnump << endl;
            if (axisnump == axisnumm) {
                InputMapElement *imp = new InputMapElement("righty", MAP_SCAN_ANALOG, -1, -1, false);
                imp->gcbValue = "a" + axisnumm;
                if ((rightyp[0] == '-') && (rightym[0] == '+')) {
                    imp->gcbValue += "~";
                }
                scanningElements[23]->gcbValue = "";
                scanningElements[24]->gcbValue = "";
                scanningElements.emplace_back(imp);
            }
        }

    scanningElementsFinal.clear();
    vector<int> toRemove;
    for (int i = 0; i < scanningElements.size(); i++) {
        InputMapElement *el = scanningElements[i];
        if (el->gcbValue != "") scanningElementsFinal.emplace_back(el);
        else
            free(el);
    }
    InputMapElement *imp = new InputMapElement("platform", MAP_SCAN_ANALOG, -1, -1, false);
    imp->gcbValue = SDL_GetPlatform();
    scanningElementsFinal.emplace_back(imp);
    scanningElements.clear();
    initScanningElements();
}

int GuiPadconfig::getMotionChangedHat(int *dir) {
    joyinfo.update();
    for (int i = 0; i < joyinfo.state.numHats; i++) {
        if (joyinfo.state.hatsPos[i] != SDL_HAT_CENTERED) {
            switch (joyinfo.state.hatsPos[i]) {
                case SDL_HAT_UP:
                    if (dir != NULL) *dir = SDL_HAT_UP;
                    return i;
                case SDL_HAT_DOWN:
                    if (dir != NULL) *dir = SDL_HAT_DOWN;
                    return i;
                case SDL_HAT_LEFT:
                    if (dir != NULL) *dir = SDL_HAT_LEFT;
                    return i;
                case SDL_HAT_RIGHT:
                    if (dir != NULL) *dir = SDL_HAT_RIGHT;
                    return i;
                default:
                    if (dir != NULL) *dir = SDL_HAT_CENTERED;
            }
        }
    }
    return -1;
}

int GuiPadconfig::getMotionChangedAxis(int *dif, int *sign) {
    joyinfo.update();
    int differences;
    int signs;

    for (int i = 0; i < joyinfo.state.numAxes; i++) {
        differences = abs(joyinfo.state.axesPos[i] - initialState.axesPos[i]);
        signs = (joyinfo.state.axesPos[i] - initialState.axesPos[i]) > 0;
        if (differences > 32000) {
            // wait for max val - update again
            joyinfo.update();
            differences = abs(joyinfo.state.axesPos[i] - initialState.axesPos[i]);
            if (dif != NULL) *dif = differences;
            if (sign != NULL) *sign = signs;
            return i;
        }
    }
    return -1;
}

int GuiPadconfig::getNumberPressedButton() {

    joyinfo.update();
    for (int i = 0; i < joyinfo.state.numButtons; i++) {
        if (joyinfo.state.buttonPos[i] != initialState.buttonPos[i]) {
            return i;
        }
    }
    return -1;
}

void GuiPadconfig::renderGamePad(GCState state) {
    if (stageMode == STAGE_TEST) {
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 150);
    } else {
        SDL_SetRenderDrawColor(renderer, 255, 0, 0, 100);

    }
    SDL_Rect rect;
    bool showAnalogState = true;
    if (stageMode == STAGE_MAPING) {
        GCState newState;
        for (int i = 0; i < 15; i++) newState.buttons[i] = 0;
        for (int i = 0; i < 6; i++) newState.axes[i] = 0;
        if (scanningElements[scannedElement]->setBtn != -1) {
            newState.buttons[scanningElements[scannedElement]->setBtn] = 1;
        }
        if (scanningElements[scannedElement]->setAx != -1) {
            int val = scanningElements[scannedElement]->negativeAx ? -32767 : 32767;
            newState.axes[scanningElements[scannedElement]->setAx] = val;
        }
        state = newState;
        if (scanningElements[scannedElement]->targetScanType != MAP_SCAN_ANALOG) {
            showAnalogState = false;
        }
    }


    int positions[15][4] = {{745, 481, 30, 30},
                            {777, 449, 30, 30},
                            {713, 449, 30, 30},
                            {745, 416, 30, 30},
                            {586, 454, 32, 21},
                            {624, 472, 32, 32},
                            {660, 454, 32, 21},
                            {548, 492, 61, 61},
                            {670, 492, 61, 61},
                            {498, 298, 51, 27},
                            {730, 298, 51, 27},
                            {508, 432, 22, 26},
                            {508, 471, 22, 26},
                            {486, 452, 26, 23},
                            {526, 452, 26, 23},};

    for (int i = 0; i < 15; i++) {
        if (state.buttons[i] != 0) {
            rect.x = positions[i][0];
            rect.y = positions[i][1];
            rect.w = positions[i][2];
            rect.h = positions[i][3];
            SDL_RenderFillRect(renderer, &rect);
        }
    }

    if (showAnalogState) {
        // left stick
        int movex = state.axes[0] * 20 / 32768;
        int movey = state.axes[1] * 20 / 32768;
        rect.x = 571 + movex;
        rect.y = 515 + movey;
        rect.w = 16;
        rect.h = 16;
        SDL_RenderFillRect(renderer, &rect);
        // right stick
        movex = state.axes[2] * 20 / 32768;
        movey = state.axes[3] * 20 / 32768;
        rect.x = 693 + movex;
        rect.y = 515 + movey;
        rect.w = 16;
        rect.h = 16;
        SDL_RenderFillRect(renderer, &rect);
    }

    // L2
    int heightt1 = state.axes[4] * 47 / 32768;
    //movey=state.axes[3]*20/32768;
    rect.x = 498;
    rect.y = 237 + 46 - heightt1;
    rect.w = 51;
    rect.h = 1 + heightt1;
    // R2
    SDL_RenderFillRect(renderer, &rect);
    heightt1 = state.axes[5] * 47 / 32768;
    rect.x = 730;
    rect.y = 237 + 46 - heightt1;
    rect.w = 51;
    rect.h = 1 + heightt1;
    SDL_RenderFillRect(renderer, &rect);

}

string GuiPadconfig::mappingToString() {

    int axdiff, axsign, hatPos;
    int btnPressed = getNumberPressedButton();
    int hatPressed = getMotionChangedHat(&hatPos);
    int axisPresed = getMotionChangedAxis(&axdiff, &axsign);

    string result = "";
    if (btnPressed != -1) {
        Mix_PlayChannel(-1, gui->cursor, 0);
        bool waitForButtonClear = true;
        while (waitForButtonClear) {
            joyinfo.update();
            waitForButtonClear = false;
            for (int i = 0; i < joyinfo.state.numButtons; i++) {
                if (joyinfo.state.buttonPos[i] != initialState.buttonPos[i]) {
                    waitForButtonClear = true;
                    continue;
                }
            }
        }
        result = "b" + to_string(btnPressed);
        goto out;
    }
    if (hatPressed != -1) {
        Mix_PlayChannel(-1, gui->cursor, 0);
        bool waitForButtonClear = true;
        while (waitForButtonClear) {
            joyinfo.update();
            waitForButtonClear = false;
            for (int i = 0; i < joyinfo.state.numHats; i++) {
                if (joyinfo.state.hatsPos[i] != initialState.hatsPos[i]) {
                    waitForButtonClear = true;
                    continue;
                }
            }
        }
        result = "h" + to_string(hatPressed) + "." + to_string(hatPos);
        goto out;
    }
    if (axisPresed != -1) {
        Mix_PlayChannel(-1, gui->cursor, 0);
        bool waitForButtonClear = true;
        while (waitForButtonClear) {
            joyinfo.update();
            waitForButtonClear = false;
            for (int i = 0; i < joyinfo.state.numAxes; i++) {
                int diff = abs(joyinfo.state.axesPos[i] - initialState.axesPos[i]);
                if (diff > 20000) {
                    waitForButtonClear = true;
                    continue;
                }
            }
        }
        string res = string(axsign ? "+" : "-") + "a" + to_string(axisPresed);
        Sint16 initial = initialState.axesPos[axisPresed];
        if (initial > -600 && initial < 600) // started from 0
        {
            result = string(axsign != 0 ? "+" : "-") + "a" + to_string(axisPresed);
        } else {
            // it started from non zero - probably analog trigger
            result = "a" + to_string(axisPresed);
        }


    }
    out:

    for (InputMapElement *map:scanningElements) {
        //skip already mapped elements
        if (map->gcbValue == result) {
            return "";
        }
    }
    return result;
}

void GuiPadconfig::renderMappingData() {
    if (stageMode == STAGE_MAPING) {
        for (int i = 0; i < scanningElements.size(); i++) {
            InputMapElement *el = scanningElements[i];
            gui->renderTextLine(string(el->apiName) + ":" + el->gcbValue, 8 + i, 0,
                                POS_LEFT);
        }
    } else {
        for (int i = 0; i < scanningElementsFinal.size(); i++) {
            InputMapElement *el = scanningElementsFinal[i];
            gui->renderTextLine(string(el->apiName) + ":" + el->gcbValue, 8 + i, 0,
                                POS_LEFT);
        }
    }
}

void GuiPadconfig::render() {
    if ((stageMode == STAGE_MAPING) || (stageMode == STAGE_SAVE)) {
        if (SDL_NumJoysticks() != numJoyAtStart) {
            cancelMappingProcedure();
            gui->drawText(_("Gamepad configuration changed. Mapping interrupted."));
            SDL_Delay(2000);

        }
    }
    joyinfo.update();
    SDL_RenderClear(renderer);
    gui->renderBackground();
    gui->renderTextBar(700);
    int offset = 10;
    gui->renderTextLine(_("Gamepad configuration details"), 0, offset, POS_CENTER);


    if (stageMode == STAGE_MAPING) {
        string pressed = mappingToString();
        if (pressed != "") {

            scanningElements[scannedElement]->gcbValue = pressed;
            nextElement();
        }
    }


    gui->renderTextLine(joyinfo.nameInfo, 1, offset, POS_LEFT);
    gui->renderTextLine(_("Gamepad input configuration: ") + "A:" + to_string(joyinfo.state.numAxes)
                        + "  B:" + to_string(joyinfo.state.numButtons) + " D:" + to_string(joyinfo.state.numHats), 2,
                        offset, POS_LEFT);
    string buttonState = _("Buttons:") + " ";
    for (int i = 0; i < joyinfo.state.numButtons; i++) {
        buttonState += to_string(joyinfo.state.buttonPos[i]) + " ";
    }
    buttonState += " " + _("Hats:") + " ";
    for (int i = 0; i < joyinfo.state.numHats; i++) {
        buttonState += to_string(joyinfo.state.hatsPos[i]) + " ";
    }
    gui->renderTextLine(buttonState, 3, offset, POS_LEFT);
    string axesState = "";
    int axeslines = (joyinfo.state.numAxes / 15) + 1;
    vector<string> axline;
    for (int i = 0; i < axeslines; i++) {
        string line = "";
        for (int axi = (15 * i); axi < (15 * i) + 15; axi++) {

            bool sign = (joyinfo.state.axesPos[axi] * 100 / 32767) >= 0;
            auto absval = abs(joyinfo.state.axesPos[axi] * 100 / 32767);
            auto padded = std::to_string(absval);
            padded.insert(0, 3U - std::min(std::string::size_type(3), padded.length()), '0');
            axesState +=
                    string(axi < 10 ? " " : "") + "#" + to_string(axi + 1) + ":" + (sign ? " " : "-") + padded + " ";
            if (axi + 1 >= joyinfo.state.numAxes) break;

        }
        gui->renderTextLine(axesState, 4 + i, offset, POS_LEFT);
        axline.push_back(axesState);
        axesState = "";
    }

    if (stageMode == STAGE_TEST) {
        gui->renderTextLine(
                _("NOTE: Make sure none of the buttons are pressed before mapping and all analog sticks are in default position."),
                7, offset, POS_CENTER);
    } else if (stageMode == STAGE_MAPING) {
        renderMappingData();
        if (scanningElements[scannedElement]->targetScanType == MAP_SCAN_DIGITAL) {
            gui->renderTextLine(_("Press a button highlighted or (OPEN) if not avaliable."), 7, offset, POS_CENTER);

        } else if (scanningElements[scannedElement]->targetScanType == MAP_SCAN_TRIGGER) {
            gui->renderTextLine(_("Press a trigger highlighted fully or (OPEN) if not avaliable."), 7, offset,
                                POS_CENTER);
        } else if (scanningElements[scannedElement]->targetScanType == MAP_SCAN_ANALOG) {
            gui->renderTextLine(_("Move your sticks to state shown or press (OPEN) if stick position not avaliable."),
                                7, offset,
                                POS_CENTER);
        }
    } else if (stageMode == STAGE_SAVE) {
        renderMappingData();
        gui->renderTextLine(_("Mapping Complete - press (OPEN) to save. (POWER) to cancel."), 7, offset,
                            POS_CENTER);
    }

    if (stageMode == STAGE_TEST) {
        gui->renderTextLine(_("You can test your controller"), 8, offset, POS_CENTER);
    } else if (stageMode == STAGE_MAPING) {
        gui->renderTextLine(_("Updating mapping"), 8, offset, POS_CENTER);
    } else if (stageMode == STAGE_SAVE) {
        gui->renderTextLine(_("Please test a new mapping"), 8, offset, POS_CENTER);
    }
    SDL_Rect posRect;
    posRect.x = 430;
    posRect.y = 200;
    posRect.w = 420;
    posRect.h = 425;
    SDL_RenderCopy(renderer, gui->gamepadImg, nullptr, &posRect);

    renderGamePad(joyinfo.gcstate);

    if (stageMode == STAGE_TEST) {
        if (SDL_NumJoysticks() >= 0) {
            gui->renderStatus(
                    "(RESET) " + _("Next pad") + "   (OPEN) " + _("Update mapping") + "   (POWER) " + _("Exit"));
        } else {
            gui->renderStatus("(POWER)  " + _("Exit"));
        }
    } else if (stageMode == STAGE_MAPING) {
        gui->renderStatus("(OPEN) " + _("No button on controller") + "  (POWER) " + _("Cancel mapping"));
    } else {
        gui->renderStatus("(OPEN) " + _("Save") + "  (POWER) " + _("Cancel mapping"));
    }

    SDL_RenderPresent(renderer);

}

void GuiPadconfig::cancelMappingProcedure() {
    clearScanningMapping();
    if (originalMappingString != "") {
        SDL_GameControllerAddMapping(originalMappingString.c_str());
    }
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_KEYDOWN,SDL_KEYUP);
    stageMode = STAGE_TEST;
}


void GuiPadconfig::startMappingProcedure() {
    numJoyAtStart = SDL_NumJoysticks();
    if (SDL_IsGameController(joyinfo.index)) {
        originalMappingString = SDL_GameControllerMappingForDeviceIndex(joyinfo.index);
    } else {
        originalMappingString = "";
    }
    int lastIndex = joyinfo.index;
    clearScanningMapping();
    joyinfo.close();
    joyinfo.open(lastIndex);
    joyinfo.updateJoyState(&initialState, joyinfo.joy);
    SDL_PumpEvents();
    SDL_FlushEvents(SDL_KEYDOWN,SDL_KEYUP);
    stageMode = STAGE_MAPING;
    scannedElement = 0;

}

void GuiPadconfig::nextElement() {
    if (stageMode != STAGE_MAPING) return;
    if ((scannedElement + 1) < scanningElements.size()) {
        scannedElement++;

    } else {
        generateFinalElements();
        string mappingString = "";
        string newName = SDL_JoystickName(joyinfo.joy);
        newName.resize(remove_if(newName.begin(), newName.end(),[](char x){return !isalnum(x) && !isspace(x);})-newName.begin());
        mappingString += joyinfo.guidString + "," + newName + ",";
        for (InputMapElement *el:scanningElementsFinal) {
            mappingString += el->apiName + ":" + el->gcbValue + ",";
        }
        cout << "New: Mapping String:" << endl << mappingString << endl;
        int added = SDL_GameControllerAddMapping(mappingString.c_str());
        if (added != -1) {
            int idx = joyinfo.index;
            joyinfo.close();
            joyinfo.open(idx);
        }
        SDL_PumpEvents();
        SDL_FlushEvents(SDL_KEYDOWN,SDL_KEYUP);
        stageMode = STAGE_SAVE;
    }

}

void GuiPadconfig::saveNewMapping() {
    auto *keyb = new GuiKeyboard(renderer);
    string newName =  SDL_JoystickName(joyinfo.joy);
    newName.resize(remove_if(newName.begin(), newName.end(),[](char x){return !isalnum(x) && !isspace(x);})-newName.begin());
    keyb->result = newName;
    keyb->label = _("Enter name for new gamepad");
    keyb->show();
    if (keyb->cancelled) {
        delete keyb;
        cancelMappingProcedure();
        return;
    }
    newName = keyb->result;
    newName.resize(remove_if(newName.begin(), newName.end(),[](char x){return !isalnum(x) && !isspace(x);})-newName.begin());
    delete keyb;
    if (newName.empty()) {
        cancelMappingProcedure();
        return;
    }

    string mappingString = "";
    mappingString += joyinfo.guidString + "," + newName + ",";
    for (InputMapElement *el:scanningElementsFinal) {
        mappingString += el->apiName + ":" + el->gcbValue + ",";
    }
    int added = SDL_GameControllerAddMapping(mappingString.c_str());
    if (added != -1) {
        gcdbprocessor processor;
        processor.readFile(gui->mapper.getControllerDBPath());
        processor.replaceMapping(joyinfo.guidString, mappingString);
        processor.saveFile(gui->mapper.getControllerDBPath());
        processor.free();
        gui->drawText(_("Mapping stored to database"));
    } else {
        gui->drawText(_("Error Storing mapping to database"));
    }
    SDL_Delay(2000);
    stageMode = STAGE_TEST;
}

void GuiPadconfig::loop() {
    menuVisible = true;
    while (menuVisible) {
        SDL_JoystickUpdate();
        if (stageMode != STAGE_MAPING) {
            SDL_GameControllerUpdate();
        }
        SDL_Event e;
        render();
        while (SDL_PollEvent(&e)) {

            switch (e.type) {
                case SDL_KEYDOWN:
                    if (e.key.keysym.scancode == SDL_SCANCODE_SLEEP || e.key.keysym.sym == SDLK_ESCAPE) {
                        Mix_PlayChannel(-1, gui->cursor, 0);
                        if (stageMode == STAGE_TEST) {
                            joyinfo.close();
                            freeScanningMapping();
                            menuVisible = false;

                        } else cancelMappingProcedure();
                    }
                    if (e.key.keysym.scancode == SDL_SCANCODE_AUDIOPLAY || e.key.keysym.sym == SDLK_SPACE) {
                        if (stageMode == STAGE_TEST) {
                            if (SDL_NumJoysticks() > 0) {
                                Mix_PlayChannel(-1, gui->cursor, 0);
                                nextController();
                            }
                        }
                    }
                    if (e.key.keysym.scancode == SDL_SCANCODE_EJECT || e.key.keysym.sym == SDLK_RETURN) {

                        switch (stageMode)
                        {
                            case STAGE_TEST:
                                if (SDL_NumJoysticks() > 0) {
                                    Mix_PlayChannel(-1, gui->cursor, 0);
                                    startMappingProcedure();
                                }
                                break;
                            case STAGE_MAPING:
                                Mix_PlayChannel(-1, gui->cursor, 0);
                                nextElement();
                                break;
                            case STAGE_SAVE:
                                Mix_PlayChannel(-1, gui->cursor, 0);
                                saveNewMapping();

                        }


                    }
                    break;
                case SDL_JOYDEVICEADDED:
                    if (stageMode == STAGE_TEST) {
                        gui->drawText(_("Gamepad configuration changed."));
                        SDL_Delay(2000);
                        if (joyinfo.index == -1) {
                            joyinfo.open(0);
                        }
                    }
                    break;
                case SDL_JOYDEVICEREMOVED:
                    if (stageMode == STAGE_TEST) {
                        gui->drawText(_("Gamepad configuration changed."));
                        SDL_Delay(2000);
                        if (e.jdevice.which == joyinfo.instanceId) {
                            joyinfo.close();
                            joyinfo.open(0);
                        }
                    }
                    break;
            }
        }
    }
}