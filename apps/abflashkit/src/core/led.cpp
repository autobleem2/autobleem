//
// Led: the power LED.
//
#include "led.h"
#include "core/main.h"

#include <ableem/engine/log.h>

#include <chrono>
#include <fstream>

using namespace std;

//*******************************
// Led::name
//*******************************
const char *Led::name(LedMode mode) {
    switch (mode) {
    case LedMode::Off:
        return "off";
    case LedMode::Red:
        return "red";
    case LedMode::Green:
        return "green";
    case LedMode::Orange:
        return "orange";
    case LedMode::BlinkRed:
        return "blink red";
    case LedMode::BlinkGreen:
        return "blink green";
    case LedMode::BlinkOrange:
        return "blink orange";
    }
    return "?";
}

//*******************************
// SysfsLed
//*******************************
SysfsLed::SysfsLed(const string &ledsDir) : ledsDir_(ledsDir), mode_(LedMode::Off), stopping_(false) {
    blinker_ = thread(&SysfsLed::blinkLoop, this);
}

SysfsLed::~SysfsLed() {
    stopping_ = true;
    if (blinker_.joinable())
        blinker_.join();
}

void SysfsLed::setMode(LedMode mode) {
    mode_ = mode;
    show(mode, true);
}

void SysfsLed::write(const string &led, bool on) {
    ofstream out(ledsDir_ + sep + led + sep + "brightness");
    if (out)
        out << (on ? "1" : "0");
}

// the colour for a mode; `phase` is which half of a blink this is (a steady colour ignores it)
void SysfsLed::show(LedMode mode, bool phase) {
    bool red = false, green = false;
    switch (mode) {
    case LedMode::Red:
        red = true;
        break;
    case LedMode::Green:
        green = true;
        break;
    case LedMode::Orange:
        red = green = true;
        break;
    case LedMode::BlinkRed:
        red = phase;
        break;
    case LedMode::BlinkGreen:
        green = phase;
        break;
    case LedMode::BlinkOrange:
        red = green = phase;
        break;
    case LedMode::Off:
        break;
    }
    write("red", red);
    write("green", green);
}

void SysfsLed::blinkLoop() {
    bool phase = false;
    while (!stopping_) {
        this_thread::sleep_for(chrono::milliseconds(static_cast<int>(BlinkPeriodMs)));
        LedMode mode = mode_;
        if (mode == LedMode::BlinkRed || mode == LedMode::BlinkGreen || mode == LedMode::BlinkOrange) {
            phase = !phase;
            show(mode, phase);
        }
    }
}

//*******************************
// NullLed
//*******************************
void NullLed::setMode(LedMode newMode) {
    PLOG_INFO << "LED " << Led::name(newMode);
    mode = newMode;
}
