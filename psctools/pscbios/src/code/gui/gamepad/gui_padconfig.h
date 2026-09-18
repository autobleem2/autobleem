#pragma once

#include "../gui_screen.h"
#include "../../lang.h"
#include "../gamepad/joy_info.h"

#define MAP_UNKNOWN -1
#define MAP_AXIS 0
#define MAP_BUTTON 1
#define MAP_HAT 3

#define MAP_SCAN_DIGITAL 0
#define MAP_SCAN_ANALOG  1
#define MAP_SCAN_TRIGGER 2

#define HAT_UP    1
#define HAT_RIGHT 2
#define HAT_DOWN  4
#define HAR_LEFT  8

class InputMapElement {
public:
    InputMapElement(string api, int scanytpe, int btn, int ax, bool neg) {
        apiName = api;
        targetScanType = scanytpe;
        setBtn = btn;
        setAx = ax;
        negativeAx = neg;
    };
    string apiName;
    string gcbValue;
    int targetScanType = MAP_SCAN_DIGITAL;
    int setBtn = 0;
    int setAx = 0;
    bool negativeAx = true;
};

#define STAGE_TEST     0
#define STAGE_MAPING   1
#define STAGE_SAVE     3

class GuiPadconfig : public GuiScreen {
    vector<InputMapElement *> scanningElements;
    vector<InputMapElement *> scanningElementsFinal;
    JoyInfo joyinfo;
    JoyState initialState;
    int scannedElement = -1;
    int numJoyAtStart=0;

    string originalMappingString;
    void generateFinalElements();
    void initScanningElements();

    void saveNewMapping();

    void clearScanningMapping();

    void freeScanningMapping();

    void nextElement();

    void startMappingProcedure();

    void cancelMappingProcedure();

    string mappingToString();

    int stageMode = STAGE_TEST;

    void init();

    void render();

    void renderMappingData();

    void renderGamePad(GCState state);

    int getNumberPressedButton();

    int getMotionChangedAxis(int *dif, int *sign);

    int getMotionChangedHat(int *dir);

    void loop();

    void nextController();


    using GuiScreen::GuiScreen;
};


