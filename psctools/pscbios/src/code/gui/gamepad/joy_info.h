//
// Created by screemer on 30.04.2020.
//
#pragma once

#include "vector"
#include "string"
#include "../gui_screen.h"
#include "SDL2/SDL.h"
#include "../../lang.h"


using namespace std;
struct JoyState {
    int numAxes=0;
    int numButtons=0;
    int numHats=0;
    vector<Sint16> axesPos;
    vector<Uint8> buttonPos;
    vector<Uint8> hatsPos;
};

struct GCState {
    Uint8 buttons[15];
    Sint16 axes[6];
};

class JoyInfo {
public:
    int index = -1;
    int instanceId=-1;
    SDL_Joystick *joy=NULL;
    SDL_GameController *pad=NULL;
    string nameInfo;
    string guidString;
    bool isController;
    GCState gcstate;
    JoyState state;

    void updateJoyState(JoyState * state, SDL_Joystick * joy);
    void open(int index);

    void update();

    void close();

};



