// Real pads as the console's kernel shows them - their evdev keys and axes, the gamecontrollerdb line SDL resolves
// them with, and which kernel control is which pad element - for the table test in test_abpad_core.cpp that sends
// every one through the whole chain. "recorded" = read off the owner's console (/proc/bus/input/devices, EVIOCGABS,
// abpadd.log); "synthetic" = written from the kernel driver's known layout and the db line, no capture.
#pragma once

#include "core/kernel_pad.h"
#include "core/mapping.h"

#include <string>
#include <vector>

namespace padfixtures {

constexpr int EvKey = 1;
constexpr int EvAbs = 3;

// one way of pressing an element on this pad: a key down, or an axis or hat at a value
struct PadControl {
    abpad::Element element;
    int type;
    int code;
    int value;
};

struct PadDescription {
    std::string label;
    std::string source;
    std::string line; // empty: no database knows it, abpadd guesses (guessMapping)
    std::vector<int> keys;
    std::vector<abpad::EvdevAbs> abs;
    std::vector<std::pair<int, int>> rest; // every axis's value at rest (a trigger at the bottom, a stick centred)
    std::vector<PadControl> controls;
    // the left stick (code -1: the pad has none): its axes and a resting offset a worn pad shows (device units)
    int leftX;
    int leftY;
    int drift;
};

// the kernel's codes
constexpr int BtnA = 0x130, BtnB = 0x131, BtnC = 0x132, BtnX = 0x133, BtnY = 0x134, BtnZ = 0x135, BtnTL = 0x136,
              BtnTR = 0x137, BtnTL2 = 0x138, BtnTR2 = 0x139, BtnSelect = 0x13a, BtnStart = 0x13b, BtnMode = 0x13c,
              BtnThumbL = 0x13d, BtnThumbR = 0x13e;
constexpr int BtnTrigger = 0x120; // a HID joystick's buttons: BTN_TRIGGER, BTN_THUMB, ... in order
constexpr int AbsX = 0x00, AbsY = 0x01, AbsZ = 0x02, AbsRX = 0x03, AbsRY = 0x04, AbsRZ = 0x05, AbsHat0X = 0x10,
              AbsHat0Y = 0x11;

using abpad::Element;

// the d-pad as hat 0 (most pads)
inline std::vector<PadControl> hatDpad() {
    return {{Element::DpUp, EvAbs, AbsHat0Y, -1},
            {Element::DpDown, EvAbs, AbsHat0Y, 1},
            {Element::DpLeft, EvAbs, AbsHat0X, -1},
            {Element::DpRight, EvAbs, AbsHat0X, 1}};
}

inline std::vector<PadControl> plus(std::vector<PadControl> a, const std::vector<PadControl> &b) {
    a.insert(a.end(), b.begin(), b.end());
    return a;
}

// 0..255 sticks and triggers centred at 128 / resting at 0, a hat: the shape of the PlayStation pads' drivers
inline std::vector<std::pair<int, int>> sonyRest() {
    return {{AbsX, 128}, {AbsY, 128}, {AbsZ, 0}, {AbsRX, 128}, {AbsRY, 128}, {AbsRZ, 0}, {AbsHat0X, 0}, {AbsHat0Y, 0}};
}

inline std::vector<abpad::EvdevAbs> sonyAbs() {
    return {{AbsX, 0, 255},  {AbsY, 0, 255},  {AbsZ, 0, 255},    {AbsRX, 0, 255},
            {AbsRY, 0, 255}, {AbsRZ, 0, 255}, {AbsHat0X, -1, 1}, {AbsHat0Y, -1, 1}};
}

inline std::vector<PadDescription> knownPads() {
    std::vector<PadDescription> pads;

    // the owner's DualSense over Bluetooth (hid-playstation): event1 of /proc/bus/input/devices, KEY 7fdb0000 at 0x120,
    // ABS 3003f, sticks/triggers 0..255; abpadd.log "player 1 is PS5 Controller (050000004c050000e60c000000810000)"
    pads.push_back(
        {"DualSense (hid-playstation, Bluetooth)",
         "recorded",
         "050000004c050000e60c000000810000,PS5 Controller,a:b0,b:b1,back:b8,dpdown:h0.4,dpleft:h0.8,"
         "dpright:h0.2,dpup:h0.1,guide:b10,leftshoulder:b4,leftstick:b11,lefttrigger:a2,leftx:a0,lefty:a1,"
         "rightshoulder:b5,rightstick:b12,righttrigger:a5,rightx:a3,righty:a4,start:b9,x:b3,y:b2,"
         "platform:Linux,",
         {BtnA, BtnB, BtnX, BtnY, BtnTL, BtnTR, BtnTL2, BtnTR2, BtnSelect, BtnStart, BtnMode, BtnThumbL, BtnThumbR},
         sonyAbs(),
         sonyRest(),
         plus({{Element::A, EvKey, BtnA, 1},
               {Element::B, EvKey, BtnB, 1},
               {Element::Y, EvKey, BtnX, 1}, // hid-playstation: Triangle is BTN_NORTH (= BTN_X)
               {Element::X, EvKey, BtnY, 1}, // Square is BTN_WEST (= BTN_Y)
               {Element::LeftShoulder, EvKey, BtnTL, 1},
               {Element::RightShoulder, EvKey, BtnTR, 1},
               {Element::LeftTrigger, EvAbs, AbsZ, 255},
               {Element::RightTrigger, EvAbs, AbsRZ, 255},
               {Element::Back, EvKey, BtnSelect, 1},
               {Element::Start, EvKey, BtnStart, 1}},
              hatDpad()),
         AbsX,
         AbsY,
         8});

    // the console's own pad (054c:0cda, its two axes 0..2 are the d-pad, L2/R2 are buttons): the 2020 facts
    // (pad-mapping.md 1.4), and the line our gamecontrollerdb.txt resolves it with
    pads.push_back({"PlayStation Classic pad",
                    "synthetic",
                    "030000004c050000da0c000011010000,Sony PlayStation Classic Controller,a:b2,b:b1,back:b8,"
                    "dpdown:+a1,dpleft:-a0,dpright:+a0,dpup:-a1,leftshoulder:b6,lefttrigger:b4,rightshoulder:b7,"
                    "righttrigger:b5,start:b9,x:b3,y:b0,platform:Linux,",
                    {BtnA, BtnB, BtnC, BtnX, BtnY, BtnZ, BtnTL, BtnTR, BtnTL2, BtnTR2},
                    {{AbsX, 0, 2}, {AbsY, 0, 2}},
                    {{AbsX, 1}, {AbsY, 1}},
                    {{Element::Y, EvKey, BtnA, 1},
                     {Element::B, EvKey, BtnB, 1},
                     {Element::A, EvKey, BtnC, 1},
                     {Element::X, EvKey, BtnX, 1},
                     {Element::LeftTrigger, EvKey, BtnY, 1},
                     {Element::RightTrigger, EvKey, BtnZ, 1},
                     {Element::LeftShoulder, EvKey, BtnTL, 1},
                     {Element::RightShoulder, EvKey, BtnTR, 1},
                     {Element::Back, EvKey, BtnTL2, 1},
                     {Element::Start, EvKey, BtnTR2, 1},
                     {Element::DpUp, EvAbs, AbsY, 0},
                     {Element::DpDown, EvAbs, AbsY, 2},
                     {Element::DpLeft, EvAbs, AbsX, 0},
                     {Element::DpRight, EvAbs, AbsX, 2}},
                    -1,
                    -1,
                    0});

    // a DualShock 4 v2 under a modern hid-sony (USB, 054c:09cc v8111): the same layout as the DualSense
    pads.push_back(
        {"DualShock 4 v2 (hid-sony, modern)",
         "synthetic",
         "030000004c050000cc09000011810000,PS4 Controller,a:b0,b:b1,back:b8,dpdown:h0.4,dpleft:h0.8,"
         "dpright:h0.2,dpup:h0.1,guide:b10,leftshoulder:b4,leftstick:b11,lefttrigger:a2,leftx:a0,lefty:a1,"
         "rightshoulder:b5,rightstick:b12,righttrigger:a5,rightx:a3,righty:a4,start:b9,x:b3,y:b2,"
         "platform:Linux,",
         {BtnA, BtnB, BtnX, BtnY, BtnTL, BtnTR, BtnTL2, BtnTR2, BtnSelect, BtnStart, BtnMode, BtnThumbL, BtnThumbR},
         sonyAbs(),
         sonyRest(),
         plus({{Element::A, EvKey, BtnA, 1},
               {Element::B, EvKey, BtnB, 1},
               {Element::Y, EvKey, BtnX, 1},
               {Element::X, EvKey, BtnY, 1},
               {Element::LeftShoulder, EvKey, BtnTL, 1},
               {Element::RightShoulder, EvKey, BtnTR, 1},
               {Element::LeftTrigger, EvAbs, AbsZ, 255},
               {Element::RightTrigger, EvAbs, AbsRZ, 255},
               {Element::Back, EvKey, BtnSelect, 1},
               {Element::Start, EvKey, BtnStart, 1}},
              hatDpad()),
         AbsX,
         AbsY,
         8});

    // a DualShock 4 (054c:05c4 v0111) as the console's 4.4 hid-sony gives it: the HID descriptor's order - 14 joystick
    // buttons from BTN_TRIGGER (Square, Cross, Circle, Triangle, L1, R1, L2, R2, Share, Options, L3, R3, PS, pad),
    // X Y Z(right x) RX(L2) RY(R2) RZ(right y)
    pads.push_back(
        {"DualShock 4 (hid-sony, kernel 4.4 layout)",
         "synthetic",
         "030000004c050000c405000011010000,PS4 Controller,a:b1,b:b2,back:b8,dpdown:h0.4,dpleft:h0.8,"
         "dpright:h0.2,dpup:h0.1,guide:b12,leftshoulder:b4,leftstick:b10,lefttrigger:a3,leftx:a0,lefty:a1,"
         "rightshoulder:b5,rightstick:b11,righttrigger:a4,rightx:a2,righty:a5,start:b9,touchpad:b13,x:b0,"
         "y:b3,platform:Linux,",
         {BtnTrigger, BtnTrigger + 1, BtnTrigger + 2, BtnTrigger + 3, BtnTrigger + 4, BtnTrigger + 5, BtnTrigger + 6,
          BtnTrigger + 7, BtnTrigger + 8, BtnTrigger + 9, BtnTrigger + 10, BtnTrigger + 11, BtnTrigger + 12,
          BtnTrigger + 13},
         sonyAbs(),
         {{AbsX, 128}, {AbsY, 128}, {AbsZ, 128}, {AbsRX, 0}, {AbsRY, 0}, {AbsRZ, 128}, {AbsHat0X, 0}, {AbsHat0Y, 0}},
         plus({{Element::X, EvKey, BtnTrigger, 1},
               {Element::A, EvKey, BtnTrigger + 1, 1},
               {Element::B, EvKey, BtnTrigger + 2, 1},
               {Element::Y, EvKey, BtnTrigger + 3, 1},
               {Element::LeftShoulder, EvKey, BtnTrigger + 4, 1},
               {Element::RightShoulder, EvKey, BtnTrigger + 5, 1},
               {Element::LeftTrigger, EvAbs, AbsRX, 255},
               {Element::RightTrigger, EvAbs, AbsRY, 255},
               {Element::Back, EvKey, BtnTrigger + 8, 1},
               {Element::Start, EvKey, BtnTrigger + 9, 1}},
              hatDpad()),
         AbsX,
         AbsY,
         8});

    // xpad: an Xbox 360 pad (045e:028e v0114) and an Xbox One pad (045e:02ea v0301) - A B X Y LB RB Back Start Guide LS
    // RS, sticks -32768..32767, triggers 0..255, the d-pad a hat
    for (const char *line :
         {"030000005e0400008e02000014010000,Xbox 360 Controller,a:b0,b:b1,back:b6,dpdown:h0.4,dpleft:h0.8,"
          "dpright:h0.2,dpup:h0.1,guide:b8,leftshoulder:b4,leftstick:b9,lefttrigger:a2,leftx:a0,lefty:a1,"
          "rightshoulder:b5,rightstick:b10,righttrigger:a5,rightx:a3,righty:a4,start:b7,x:b2,y:b3,platform:Linux,",
          "030000005e040000ea02000001030000,Xbox One Controller,a:b0,b:b1,back:b6,dpdown:h0.4,dpleft:h0.8,"
          "dpright:h0.2,dpup:h0.1,guide:b8,leftshoulder:b4,leftstick:b9,lefttrigger:a2,leftx:a0,lefty:a1,"
          "rightshoulder:b5,rightstick:b10,righttrigger:a5,rightx:a3,righty:a4,start:b7,x:b2,y:b3,platform:Linux,"}) {
        bool one = std::string(line).find("Xbox One") != std::string::npos;
        pads.push_back(
            {one ? "Xbox One pad (xpad)" : "Xbox 360 pad (xpad)",
             "synthetic",
             line,
             {BtnA, BtnB, BtnX, BtnY, BtnTL, BtnTR, BtnSelect, BtnStart, BtnMode, BtnThumbL, BtnThumbR},
             {{AbsX, -32768, 32767},
              {AbsY, -32768, 32767},
              {AbsZ, 0, 255},
              {AbsRX, -32768, 32767},
              {AbsRY, -32768, 32767},
              {AbsRZ, 0, 255},
              {AbsHat0X, -1, 1},
              {AbsHat0Y, -1, 1}},
             {{AbsX, 0}, {AbsY, 0}, {AbsZ, 0}, {AbsRX, 0}, {AbsRY, 0}, {AbsRZ, 0}, {AbsHat0X, 0}, {AbsHat0Y, 0}},
             plus({{Element::A, EvKey, BtnA, 1},
                   {Element::B, EvKey, BtnB, 1},
                   {Element::X, EvKey, BtnX, 1},
                   {Element::Y, EvKey, BtnY, 1},
                   {Element::LeftShoulder, EvKey, BtnTL, 1},
                   {Element::RightShoulder, EvKey, BtnTR, 1},
                   {Element::LeftTrigger, EvAbs, AbsZ, 255},
                   {Element::RightTrigger, EvAbs, AbsRZ, 255},
                   {Element::Back, EvKey, BtnSelect, 1},
                   {Element::Start, EvKey, BtnStart, 1}},
                  hatDpad()),
             AbsX,
             AbsY,
             3000});
    }

    // an 8BitDo SN30 Pro in its D-input mode (2dc8:6000... c82d:0160 v0111): fifteen buttons from BTN_SOUTH with B
    // first (b0 B, b1 A, b3 Y, b4 X), the triggers buttons, X Y Z RZ 0..255
    pads.push_back(
        {"8BitDo SN30 Pro (D-input)",
         "synthetic",
         "03000000c82d00000160000011010000,8BitDo SN30 Pro,a:b1,b:b0,back:b10,dpdown:h0.4,dpleft:h0.8,"
         "dpright:h0.2,dpup:h0.1,leftshoulder:b6,leftstick:b13,lefttrigger:b8,leftx:a0,lefty:a1,"
         "rightshoulder:b7,rightstick:b14,righttrigger:b9,rightx:a2,righty:a3,start:b11,x:b4,y:b3,"
         "platform:Linux,",
         {BtnA, BtnB, BtnC, BtnX, BtnY, BtnZ, BtnTL, BtnTR, BtnTL2, BtnTR2, BtnSelect, BtnStart, BtnMode, BtnThumbL,
          BtnThumbR},
         {{AbsX, 0, 255}, {AbsY, 0, 255}, {AbsZ, 0, 255}, {AbsRZ, 0, 255}, {AbsHat0X, -1, 1}, {AbsHat0Y, -1, 1}},
         {{AbsX, 128}, {AbsY, 128}, {AbsZ, 128}, {AbsRZ, 128}, {AbsHat0X, 0}, {AbsHat0Y, 0}},
         plus({{Element::B, EvKey, BtnA, 1},
               {Element::A, EvKey, BtnB, 1},
               {Element::Y, EvKey, BtnX, 1},
               {Element::X, EvKey, BtnY, 1},
               {Element::LeftShoulder, EvKey, BtnTL, 1},
               {Element::RightShoulder, EvKey, BtnTR, 1},
               {Element::LeftTrigger, EvKey, BtnTL2, 1},
               {Element::RightTrigger, EvKey, BtnTR2, 1},
               {Element::Back, EvKey, BtnSelect, 1},
               {Element::Start, EvKey, BtnStart, 1}},
              hatDpad()),
         AbsX,
         AbsY,
         8});

    // a generic USB pad (DragonRise 0079:0006 v0110, a HID joystick): twelve buttons from BTN_TRIGGER, y:b0 x:b3 a:b2
    // b:b1, the triggers buttons 6/7, X Y Z RX RY 0..255
    pads.push_back({"Generic USB pad (DragonRise)",
                    "synthetic",
                    "03000000790000000600000010010000,DragonRise Inc. Generic USB Joystick,a:b2,b:b1,back:b8,"
                    "dpdown:h0.4,dpleft:h0.8,dpright:h0.2,dpup:h0.1,leftshoulder:b4,leftstick:b10,lefttrigger:b6,"
                    "leftx:a0,lefty:a1,rightshoulder:b5,rightstick:b11,righttrigger:b7,rightx:a3,righty:a4,start:b9,"
                    "x:b3,y:b0,platform:Linux,",
                    {BtnTrigger, BtnTrigger + 1, BtnTrigger + 2, BtnTrigger + 3, BtnTrigger + 4, BtnTrigger + 5,
                     BtnTrigger + 6, BtnTrigger + 7, BtnTrigger + 8, BtnTrigger + 9, BtnTrigger + 10, BtnTrigger + 11},
                    {{AbsX, 0, 255},
                     {AbsY, 0, 255},
                     {AbsZ, 0, 255},
                     {AbsRX, 0, 255},
                     {AbsRY, 0, 255},
                     {AbsHat0X, -1, 1},
                     {AbsHat0Y, -1, 1}},
                    {{AbsX, 128}, {AbsY, 128}, {AbsZ, 128}, {AbsRX, 128}, {AbsRY, 128}, {AbsHat0X, 0}, {AbsHat0Y, 0}},
                    plus({{Element::Y, EvKey, BtnTrigger, 1},
                          {Element::B, EvKey, BtnTrigger + 1, 1},
                          {Element::A, EvKey, BtnTrigger + 2, 1},
                          {Element::X, EvKey, BtnTrigger + 3, 1},
                          {Element::LeftShoulder, EvKey, BtnTrigger + 4, 1},
                          {Element::RightShoulder, EvKey, BtnTrigger + 5, 1},
                          {Element::LeftTrigger, EvKey, BtnTrigger + 6, 1},
                          {Element::RightTrigger, EvKey, BtnTrigger + 7, 1},
                          {Element::Back, EvKey, BtnTrigger + 8, 1},
                          {Element::Start, EvKey, BtnTrigger + 9, 1}},
                         hatDpad()),
                    AbsX,
                    AbsY,
                    8});

    // a pad no database knows: eleven buttons, four axes, a hat - abpadd guesses its mapping (guessMapping: the
    // buttons in report order as A B X Y LB RB Back Start ...; no triggers)
    pads.push_back(
        {"A pad in no database (guessed)",
         "synthetic",
         "",
         {BtnA, BtnB, BtnC, BtnX, BtnY, BtnZ, BtnTL, BtnTR, BtnTL2, BtnTR2, BtnSelect},
         {{AbsX, 0, 255}, {AbsY, 0, 255}, {AbsRX, 0, 255}, {AbsRY, 0, 255}, {AbsHat0X, -1, 1}, {AbsHat0Y, -1, 1}},
         {{AbsX, 128}, {AbsY, 128}, {AbsRX, 128}, {AbsRY, 128}, {AbsHat0X, 0}, {AbsHat0Y, 0}},
         plus({{Element::A, EvKey, BtnA, 1},
               {Element::B, EvKey, BtnB, 1},
               {Element::X, EvKey, BtnC, 1},
               {Element::Y, EvKey, BtnX, 1},
               {Element::LeftShoulder, EvKey, BtnY, 1},
               {Element::RightShoulder, EvKey, BtnZ, 1},
               {Element::Back, EvKey, BtnTL, 1},
               {Element::Start, EvKey, BtnTR, 1}},
              hatDpad()),
         AbsX,
         AbsY,
         8});
    return pads;
}

} // namespace padfixtures
