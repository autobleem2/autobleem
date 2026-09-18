//
// Created by screemer on 30.04.2020.
//

#include <iostream>
#include "joy_info.h"
#include "SDL2/SDL_gamecontroller.h"
using namespace std;
void JoyInfo::open(int index) {
    for (int i = 0; i < 15; i++)

        gcstate.buttons[i] = 0;
    for (int i = 0; i < 6; i++) {
        gcstate.axes[i] = 0;
    }
    if (index != -1) {
        this->index = index;
        joy = SDL_JoystickOpen(index);

        if (joy != NULL) {
            if (SDL_IsGameController(index))
            {
                pad = SDL_GameControllerOpen(index);
            } else
                {
                pad = NULL;
                }
            this->index = -1;
            char guid_str[100];
            SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(index);
            SDL_JoystickGetGUIDString(guid, guid_str, 100);
            string name = SDL_JoystickNameForIndex(index);
            instanceId = SDL_JoystickInstanceID(joy);
            guidString =guid_str;
            if (SDL_IsGameController(index)) {
                name += string(" - SDL2:") + SDL_GameControllerNameForIndex(index);
                isController = true;
            }

            nameInfo =
                    "#" + to_string(index + 1) + "/" + to_string(SDL_NumJoysticks()) + "/ " + name;
            state.numAxes = SDL_JoystickNumAxes(joy);
            state.numButtons = SDL_JoystickNumButtons(joy);
            state.numHats = SDL_JoystickNumHats(joy);
            instanceId = SDL_JoystickInstanceID(joy);
            state.axesPos.resize(state.numAxes);
            state.buttonPos.resize(state.numButtons);
            state.hatsPos.resize(state.numHats);
            this->index = index;

            return;

        }
        this->index = -1;
        nameInfo = _("NO GAME CONTROLLERS OPENED");
        isController = false;
        state.numAxes = 0;
        state.numButtons = 0;
        state.numHats = 0;
        state.axesPos.clear();
        state.buttonPos.clear();
        state.hatsPos.clear();

    }
}

void JoyInfo::updateJoyState(JoyState *state, SDL_Joystick * joy) {

    SDL_PumpEvents();
    SDL_JoystickUpdate();
    if (SDL_JoystickGetAttached(joy)) {
        state->numAxes = SDL_JoystickNumAxes(joy);
        state->numButtons = SDL_JoystickNumButtons(joy);
        state->numHats = SDL_JoystickNumHats(joy);
        state->hatsPos.resize(state->numHats);
        state->axesPos.resize(state->numAxes);
        state->buttonPos.resize(state->numButtons);
        for (int i = 0; i < state->numButtons; i++) {
            state->buttonPos[i] = SDL_JoystickGetButton(joy, i);
        }

        string axisInfo="";
        for (int i = 0; i < state->numAxes; i++) {
            state->axesPos[i] = SDL_JoystickGetAxis(joy, i);
        }

        for (int i = 0; i < state->numHats; i++) {
            state->hatsPos[i] = SDL_JoystickGetHat(joy, i);
        }
    } else
    {
        state->numButtons=0;
        state->numHats=0;
        state->numAxes=0;
        state->axesPos.clear();
        state->buttonPos.clear();
        state->hatsPos.clear();
    }
}

void JoyInfo::update() {
    if (index != -1) {
        if (SDL_JoystickGetAttached(joy)) {
            updateJoyState(&state,joy);
            char guid_str[100];
            SDL_JoystickGUID guid = SDL_JoystickGetDeviceGUID(index);
            SDL_JoystickGetGUIDString(guid, guid_str, 100);
            string name = SDL_JoystickNameForIndex(index);
            instanceId = SDL_JoystickInstanceID(joy);
            if (SDL_IsGameController(index)) {
                name += string(" - SDL2:") + SDL_GameControllerNameForIndex(index);
                isController = true;
            } else
            {
                isController = false;
            }
            nameInfo =
                    "#" + to_string(index + 1) + "/" + to_string(SDL_NumJoysticks()) + " (" + guid_str + ")   " + name;

            // now update state using game controller api
            if (isController) {

              //  SDL_GameController *pad = SDL_GameControllerOpen(index);
                if (pad == NULL) {
                    return;
                }


                gcstate.buttons[0] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_A);
                gcstate.buttons[1] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_B);
                gcstate.buttons[2] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_X);
                gcstate.buttons[3] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_Y);
                gcstate.buttons[4] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_BACK);
                gcstate.buttons[5] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_GUIDE);
                gcstate.buttons[6] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_START);
                gcstate.buttons[7] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSTICK);
                gcstate.buttons[8] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSTICK);
                gcstate.buttons[9] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
                gcstate.buttons[10] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
                gcstate.buttons[11] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_UP);
                gcstate.buttons[12] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_DOWN);
                gcstate.buttons[13] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_LEFT);
                gcstate.buttons[14] = SDL_GameControllerGetButton(pad, SDL_CONTROLLER_BUTTON_DPAD_RIGHT);
                gcstate.axes[0] = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX);
                gcstate.axes[1] = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY);
                gcstate.axes[2] = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX);
                gcstate.axes[3] = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY);
                gcstate.axes[4] = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT);
                gcstate.axes[5] = SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT);


               // SDL_GameControllerClose(pad);

            }
        }
    }
}

void JoyInfo::close() {
    if (index != -1) {
        if (pad!=NULL)
        {
            SDL_GameControllerClose(pad);
        }
        if (SDL_JoystickGetAttached(joy)) {
            SDL_JoystickClose(joy);
        }
        index = -1;
    }
}

