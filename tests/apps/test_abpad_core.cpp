// abpad_core: what the virtual gamepad is made of once SDL does the reading - the gamecontrollerdb
// line a layout is written as, the controller state the daemon publishes turned into the raw pad an
// app's SDL is answered with, the shared-memory handover between the two, the per-app profile and the
// key table keyboard mode sends.

#include "core/kernel_pad.h"
#include "core/key_names.h"
#include "core/mapping.h"
#include "core/profile.h"
#include "core/shared_state.h"
#include "core/virtual_pad.h"
#include "doctest/doctest.h"

#include <initializer_list>
#include <sstream>
#include <utility>
#include <vector>
#include <string>

using namespace abpad;
using namespace std;

namespace {

// the community line for the console's own pad, as src/resources/gamecontrollerdb.txt carries it. The shim
// never resolves it - SDL does - but it is the clearest statement of why any of this exists: b0 is
// Triangle here and A on the pad every Linux port was written against.
const char *const kPscLine = "030000004c050000da0c000011010000,Playstation Classic Controller,"
                             "a:b2,b:b1,back:b8,leftshoulder:b6,lefttrigger:b4,rightshoulder:b7,"
                             "righttrigger:b5,start:b9,x:b3,y:b0,dpdown:+a1,dpleft:-a0,dpright:+a0,"
                             "dpup:-a1,platform:Linux,";

// a pad as the daemon would publish it, with these elements held
ControllerState pressing(initializer_list<Element> elements) {
    ControllerState state;
    for (Element element : elements) {
        if (isAxis(element)) {
            state.set(element, static_cast<int16_t>(32767));
        } else {
            state.set(element, true);
        }
    }
    return state;
}

} // namespace

TEST_CASE("a mapping line parses into its bindings and back") {
    PadMapping mapping;
    REQUIRE(PadMapping::parseLine(kPscLine, mapping));

    CHECK(mapping.guid == "030000004c050000da0c000011010000");
    CHECK(mapping.name == "Playstation Classic Controller");
    CHECK(mapping.platform == "Linux");
    CHECK(mapping[Element::A].kind == Binding::Kind::Button);
    CHECK(mapping[Element::A].index == 2);
    CHECK(mapping[Element::Y].index == 0);
    CHECK(mapping[Element::DpUp].kind == Binding::Kind::Axis);
    CHECK(mapping[Element::DpUp].index == 1);
    CHECK(mapping[Element::DpUp].half == -1);
    CHECK(mapping[Element::LeftX].bound() == false); // the pad has no sticks
    CHECK(mapping[Element::Guide].bound() == false);

    // the line it writes back is one SDL_GameControllerAddMapping would take, and parses to the same
    PadMapping again;
    REQUIRE(PadMapping::parseLine(mapping.toLine(), again));
    CHECK(again.toLine() == mapping.toLine());
    CHECK(again[Element::DpUp].toString() == "-a1");
}

TEST_CASE("the binding forms a mapping line can use") {
    CHECK(Binding::parse("b3").kind == Binding::Kind::Button);
    CHECK(Binding::parse("b3").index == 3);

    Binding hat = Binding::parse("h0.4");
    CHECK(hat.kind == Binding::Kind::Hat);
    CHECK(hat.index == 0);
    CHECK(hat.hatMask == 4);

    Binding half = Binding::parse("+a1");
    CHECK(half.kind == Binding::Kind::Axis);
    CHECK(half.half == 1);
    CHECK(half.inverted == false);

    Binding inverted = Binding::parse("-a3~");
    CHECK(inverted.half == -1);
    CHECK(inverted.inverted);
    CHECK(inverted.toString() == "-a3~");

    CHECK(Binding::parse("").bound() == false);
    CHECK(Binding::parse("z9").bound() == false);
    CHECK(Binding::parse("h0").bound() == false); // a hat without a mask
    CHECK(Binding::parse("b").bound() == false);
}

TEST_CASE("comments, blank lines and half-written lines are not mappings") {
    PadMapping mapping;
    CHECK(PadMapping::parseLine("# a comment", mapping) == false);
    CHECK(PadMapping::parseLine("", mapping) == false);
    CHECK(PadMapping::parseLine("   ", mapping) == false);
    CHECK(PadMapping::parseLine("030000004c050000da0c000011010000", mapping) == false);
    // an element this build does not model is skipped, not a failure
    CHECK(PadMapping::parseLine("0300,Pad,a:b0,paddle1:b11,touchpad:b12,", mapping));
    CHECK(mapping[Element::A].index == 0);
}

TEST_CASE("a pad shown to an app as an Xbox 360 pad") {
    const VirtualLayout &layout = virtualLayout(VirtualPadKind::X360);

    CHECK(layout.name == "Microsoft X-Box 360 pad");
    CHECK(layout.buttonCount == 11);
    CHECK(layout.axisCount == 6);
    CHECK(layout.hatCount == 1);

    SUBCASE("Cross arrives as button 0, which is what the app calls A") {
        RawPadState raw = buildRawState(layout, pressing({Element::A}));
        REQUIRE(raw.buttons.size() == 11);
        CHECK(raw.buttons[0]);
        CHECK(raw.buttons[1] == false);
    }

    SUBCASE("Triangle is not button 0 any more") {
        RawPadState raw = buildRawState(layout, pressing({Element::Y}));
        CHECK(raw.buttons[0] == false);
        CHECK(raw.buttons[3]); // y:b3 on the X360 pad
    }

    SUBCASE("the d-pad arrives on the hat the app reads") {
        RawPadState raw = buildRawState(layout, pressing({Element::DpUp}));
        REQUIRE(raw.hats.size() == 1);
        CHECK(raw.hats[0] == 1);
        CHECK(raw.axes[0] == 0); // and not on the left stick
    }

    SUBCASE("two d-pad directions at once are one hat value") {
        RawPadState raw = buildRawState(layout, pressing({Element::DpUp, Element::DpRight}));
        CHECK(raw.hats[0] == (1 | 2));
    }

    SUBCASE("an untouched trigger rests at the bottom of its travel") {
        RawPadState raw = buildRawState(layout, ControllerState());
        CHECK(raw.axes[2] == -32768);
        CHECK(raw.axes[5] == -32768);
        CHECK(raw.axes[0] == 0); // a stick still rests in the middle
    }

    SUBCASE("a pulled trigger reaches the top of it") {
        RawPadState raw = buildRawState(layout, pressing({Element::LeftTrigger}));
        CHECK(raw.axes[2] == 32766);
    }

    SUBCASE("a stick keeps its value and its sign") {
        ControllerState state;
        state.set(Element::LeftX, static_cast<int16_t>(-20000));
        state.set(Element::RightY, static_cast<int16_t>(12345));
        RawPadState raw = buildRawState(layout, state);
        CHECK(raw.axes[0] == -20000);
        CHECK(raw.axes[4] == 12345);
    }
}

TEST_CASE("a pad shown to an app that was ported for the console's own") {
    const VirtualLayout &layout = virtualLayout(VirtualPadKind::Psc);

    SUBCASE("A becomes the PSC pad's Cross, button 2") {
        RawPadState raw = buildRawState(layout, pressing({Element::A}));
        REQUIRE(raw.buttons.size() == 10);
        CHECK(raw.buttons[2]);
        CHECK(raw.axes.size() == 2);
        CHECK(raw.hats.empty()); // the PSC pad has no hat to put anything on
    }

    SUBCASE("the d-pad goes onto the axes, because that is where that pad has it") {
        RawPadState raw =
            buildRawState(layout, controllerView(VirtualPadKind::Psc, pressing({Element::DpLeft}), MovementAid::Both));
        CHECK(raw.axes[0] == -32768);
        CHECK(raw.axes[1] == 0);
    }

    SUBCASE("a trigger becomes a button") {
        RawPadState raw = buildRawState(layout, pressing({Element::LeftTrigger}));
        CHECK(raw.buttons[4]);
    }

    SUBCASE("an element the layout has nowhere to put is dropped, not misplaced") {
        RawPadState raw = buildRawState(layout, pressing({Element::Guide, Element::LeftStick}));
        for (bool pressed : raw.buttons) {
            CHECK(pressed == false);
        }
    }
}

// The two pad outputs a PE app can be given (PadMode= psc / x360, the shim and the kernel device), from a DualSense as
// the console's hid-playstation reports it (pad-mapping.md 1.5): sticks and triggers 0..255 with the centre at 128, the
// d-pad a hat of -1..1. The fixture reads it the way the daemon does (SDL's evdev numbering, SDL's PS5 line).
namespace {

ControllerState physicalPad(initializer_list<pair<Element, int>> values) {
    ControllerState state;
    for (const auto &v : values) {
        if (isAxis(v.first)) {
            state.set(v.first, static_cast<int16_t>(v.second));
        } else {
            state.set(v.first, v.second != 0);
        }
    }
    return state;
}

const char *const kDualSenseLine =
    "050000004c050000e60c000000810000,PS5 Controller,a:b0,b:b1,back:b8,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,"
    "dpup:h0.1,guide:b10,leftshoulder:b4,leftstick:b11,lefttrigger:a2,leftx:a0,lefty:a1,rightshoulder:b5,"
    "rightstick:b12,righttrigger:a5,rightx:a3,righty:a4,start:b9,x:b3,y:b2,platform:Linux,";

// the kernel's codes of a DualSense on the console (hid-playstation: KEY 7fdb0000 at 0x120, ABS 3003f)
constexpr int Cross = 0x130, Circle = 0x131, Triangle = 0x133, Square = 0x134, L1 = 0x136, R1 = 0x137, Create = 0x13a,
              Options = 0x13b, PsButton = 0x13c;
constexpr int AbsLX = 0x00, AbsLY = 0x01, AbsL2 = 0x02, AbsRX = 0x03, AbsRY = 0x04, AbsR2 = 0x05, HatX = 0x10,
              HatY = 0x11;

// a DualSense at rest, then these keys held and these axes/hat set to the kernel's values
ControllerState dualSense(initializer_list<pair<int, int>> abs, initializer_list<int> keys = {}) {
    static PadMapping mapping;
    static bool parsed = PadMapping::parseLine(kDualSenseLine, mapping);
    REQUIRE(parsed);
    EvdevPadState pad({0x130, 0x131, 0x133, 0x134, 0x136, 0x137, 0x138, 0x139, 0x13a, 0x13b, 0x13c, 0x13d, 0x13e},
                      {{AbsLX, 0, 255},
                       {AbsLY, 0, 255},
                       {AbsL2, 0, 255},
                       {AbsRX, 0, 255},
                       {AbsRY, 0, 255},
                       {AbsR2, 0, 255},
                       {HatX, -1, 1},
                       {HatY, -1, 1}});
    for (int code : {AbsLX, AbsLY, AbsRX, AbsRY}) {
        pad.setAbs(code, 128);
    }
    pad.setAbs(AbsL2, 0);
    pad.setAbs(AbsR2, 0);
    for (const auto &value : abs) {
        pad.setAbs(value.first, value.second);
    }
    for (int key : keys) {
        pad.setKey(key, 1);
    }
    return applyMapping(mapping, pad.raw());
}

// what each output gives for it
RawPadState pscRaw(const ControllerState &physical, MovementAid aid = MovementAid::Both) {
    return buildRawState(virtualLayout(VirtualPadKind::Psc), controllerView(VirtualPadKind::Psc, physical, aid));
}

RawPadState x360Raw(const ControllerState &physical, MovementAid aid = MovementAid::Both) {
    return buildRawState(virtualLayout(VirtualPadKind::X360), controllerView(VirtualPadKind::X360, physical, aid));
}

// the kernel device's value for an ABS code
int deviceValue(VirtualPadKind kind, const ControllerState &physical, int code, MovementAid aid = MovementAid::Both) {
    EvdevFrame frame = evdevFrame(kind, buildRawState(virtualLayout(kind), controllerView(kind, physical, aid)));
    for (const auto &abs : frame.abs) {
        if (abs.first == code) {
            return abs.second;
        }
    }
    return -999;
}

int deviceKey(VirtualPadKind kind, const ControllerState &physical, int code) {
    EvdevFrame frame =
        evdevFrame(kind, buildRawState(virtualLayout(kind), controllerView(kind, physical, MovementAid::Both)));
    for (const auto &key : frame.keys) {
        if (key.first == code) {
            return key.second;
        }
    }
    return -999;
}

bool anyButton(const RawPadState &raw) {
    for (bool pressed : raw.buttons) {
        if (pressed) {
            return true;
        }
    }
    return false;
}

} // namespace

TEST_CASE("the DualSense fixture reads as SDL reads the console's DualSense") {
    ControllerState idle = dualSense({});
    CHECK(idle.axis(Element::LeftX) == 128); // the centre, 128 of 0..255, as SDL corrects it
    CHECK(idle.axis(Element::LeftY) == 128);
    CHECK(idle.axis(Element::LeftTrigger) == 0);
    CHECK(dualSense({{AbsLX, 0}}).axis(Element::LeftX) == -32768);
    CHECK(dualSense({{AbsLX, 255}}).axis(Element::LeftX) == 32767);
    CHECK(dualSense({{AbsL2, 255}}).axis(Element::LeftTrigger) == 32767);
    CHECK(dualSense({{HatX, -1}}).button(Element::DpLeft));
    CHECK(dualSense({{HatY, 1}}).button(Element::DpDown));
    CHECK(dualSense({}, {Cross}).button(Element::A));
    CHECK(dualSense({}, {Triangle}).button(Element::Y));
}

TEST_CASE("psc output (shim): the console pad exactly as the original remap made a pad into it") {
    const VirtualLayout &layout = virtualLayout(VirtualPadKind::Psc);
    CHECK(layout.guid == "030000004c050000da0c000011010000");
    CHECK(layout.name == "Sony Interactive Entertainment Controller"); // the real pad's, which the mods test for
    CHECK(layout.vendor == 0x054c);
    CHECK(layout.product == 0x0cda);
    CHECK(layout.buttonCount == 10);
    CHECK(layout.axisCount == 2);
    CHECK(layout.hatCount == 0);

    SUBCASE("idle and small drift give no input") {
        for (const ControllerState &pad : {dualSense({}), dualSense({{AbsLX, 140}, {AbsLY, 116}}),
                                           dualSense({{AbsLX, 180}, {AbsLY, 70}, {AbsRX, 0}, {AbsL2, 30}})}) {
            RawPadState raw = pscRaw(pad);
            CHECK_FALSE(anyButton(raw));
            CHECK(raw.axes[0] == 0);
            CHECK(raw.axes[1] == 0);
        }
    }

    SUBCASE("the buttons keep the console's numbers: Triangle 0, Circle 1, Cross 2, Square 3, L2 4, R2 5, L1 6, R1 7, "
            "Select 8, Start 9; PS is nothing") {
        CHECK(pscRaw(dualSense({}, {Triangle})).buttons[0]);
        CHECK(pscRaw(dualSense({}, {Circle})).buttons[1]);
        CHECK(pscRaw(dualSense({}, {Cross})).buttons[2]);
        CHECK(pscRaw(dualSense({}, {Square})).buttons[3]);
        CHECK(pscRaw(dualSense({{AbsL2, 255}})).buttons[4]);
        CHECK(pscRaw(dualSense({{AbsR2, 255}})).buttons[5]);
        CHECK(pscRaw(dualSense({}, {L1})).buttons[6]);
        CHECK(pscRaw(dualSense({}, {R1})).buttons[7]);
        CHECK(pscRaw(dualSense({}, {Create})).buttons[8]);
        CHECK(pscRaw(dualSense({}, {Options})).buttons[9]);
        CHECK_FALSE(anyButton(pscRaw(dualSense({}, {PsButton}))));
    }

    SUBCASE("L2/R2 at full pull only (a half pull is nothing)") {
        CHECK_FALSE(pscRaw(dualSense({{AbsL2, 128}})).buttons[4]);
        CHECK_FALSE(pscRaw(dualSense({{AbsR2, 200}})).buttons[5]);
        CHECK(pscRaw(dualSense({{AbsL2, 254}})).buttons[4]);
    }

    SUBCASE("the d-pad is the two axes at their ends: -32768 / 32767") {
        CHECK(pscRaw(dualSense({{HatX, -1}})).axes[0] == -32768);
        CHECK(pscRaw(dualSense({{HatX, 1}})).axes[0] == 32767);
        CHECK(pscRaw(dualSense({{HatY, -1}})).axes[1] == -32768);
        CHECK(pscRaw(dualSense({{HatY, 1}})).axes[1] == 32767);
        CHECK(pscRaw(dualSense({{HatX, -1}, {HatY, 1}})).axes == vector<int16_t>{-32768, 32767});
    }

    SUBCASE("Analog2Dpad (the default): the stick feeds the same axes past half travel, with its own value") {
        CHECK(pscRaw(dualSense({{AbsLX, 0}})).axes[0] == -32768);
        CHECK(pscRaw(dualSense({{AbsLX, 255}})).axes[0] == 32767);
        CHECK(pscRaw(dualSense({{AbsLY, 0}})).axes[1] == -32768);
        CHECK(pscRaw(dualSense({{AbsLX, 224}})).axes[0] == 24800);              // 3/4 right: passed on as it is
        CHECK(pscRaw(dualSense({{AbsLX, 190}})).axes[0] == 0);                  // under half travel: nothing
        CHECK(pscRaw(dualSense({{AbsLX, 255}, {HatX, -1}})).axes[0] == -32768); // the d-pad wins over the stick
        CHECK(pscRaw(dualSense({{AbsLX, 0}}), MovementAid::StickToDpad).axes[0] == -32768);
    }

    SUBCASE("Analog2Dpad off: the stick is nothing, the d-pad still steers; Dpad2Analog adds nothing") {
        for (MovementAid aid : {MovementAid::AsIs, MovementAid::DpadToStick}) {
            CHECK(pscRaw(dualSense({{AbsLX, 0}, {AbsLY, 255}}), aid).axes == vector<int16_t>{0, 0});
            CHECK(pscRaw(dualSense({{HatX, 1}}), aid).axes[0] == 32767);
        }
    }

    SUBCASE("the game-controller view is the PE table's: the d-pad as the left stick, no d-pad buttons") {
        ControllerState view =
            controllerView(VirtualPadKind::Psc, dualSense({{HatY, -1}, {AbsRX, 255}, {AbsL2, 255}}), MovementAid::Both);
        CHECK(view.axis(Element::LeftY) == -32768);
        CHECK(view.axis(Element::LeftX) == 0);
        CHECK_FALSE(view.button(Element::DpUp));
        CHECK(view.axis(Element::RightX) == 0);
        CHECK(view.axis(Element::LeftTrigger) == 32767);
        CHECK(controllerView(VirtualPadKind::Psc, dualSense({{AbsLX, 150}}), MovementAid::Both) == ControllerState());
    }
}

TEST_CASE("psc output standing in for DraStic's own remap: the d-pad as hat 0, DraStic's button numbers") {
    CHECK(modRemapFromFileName("drastic_sdl_remap.so") == ModRemap::Drastic);
    CHECK(modRemapFromFileName("/media/Apps/pe-drastic/drastic_sdl_remap.so") == ModRemap::Drastic);
    CHECK(modRemapFromFileName("sdl_remap_arm.so") == ModRemap::None);
    // Triangle 3, Circle 1, Cross 0, Square 2, L2 8, R2 9, L1 4, R1 5, Select 7, Start 6 (drastic.cfg: A=1025 is
    // Circle)
    const int expected[10] = {3, 1, 0, 2, 8, 9, 4, 5, 7, 6};
    for (int b = 0; b < 10; ++b) {
        CHECK(drasticButton(b) == expected[b]);
    }
    CHECK(drasticButton(10) == -1);
    // from the console pad's axes as the psc output makes them
    CHECK(drasticHat(0, 0) == 0);
    RawPadState left = pscRaw(dualSense({{HatX, -1}}));
    CHECK(drasticHat(left.axes[0], left.axes[1]) == 8);
    RawPadState upRight = pscRaw(dualSense({{HatX, 1}, {HatY, -1}}));
    CHECK(drasticHat(upRight.axes[0], upRight.axes[1]) == (1 | 2));
    RawPadState drift = pscRaw(dualSense({{AbsLX, 150}, {AbsLY, 100}}));
    CHECK(drasticHat(drift.axes[0], drift.axes[1]) == 0);
}

TEST_CASE("psc output (kernel device): the console pad's 0..2 axes and BTN_A..BTN_TR2") {
    SUBCASE("idle and small drift: both axes in the middle, no key") {
        for (const ControllerState &pad : {dualSense({}), dualSense({{AbsLX, 140}, {AbsLY, 116}, {AbsL2, 40}})}) {
            CHECK(deviceValue(VirtualPadKind::Psc, pad, 0x00) == 1);
            CHECK(deviceValue(VirtualPadKind::Psc, pad, 0x01) == 1);
            for (int code = 0x130; code <= 0x139; ++code) {
                CHECK(deviceKey(VirtualPadKind::Psc, pad, code) == 0);
            }
        }
    }

    SUBCASE("the d-pad and (Analog2Dpad) the stick put the axes at their ends") {
        CHECK(deviceValue(VirtualPadKind::Psc, dualSense({{HatX, -1}}), 0x00) == 0);
        CHECK(deviceValue(VirtualPadKind::Psc, dualSense({{HatX, 1}}), 0x00) == 2);
        CHECK(deviceValue(VirtualPadKind::Psc, dualSense({{HatY, -1}}), 0x01) == 0);
        CHECK(deviceValue(VirtualPadKind::Psc, dualSense({{HatY, 1}}), 0x01) == 2);
        CHECK(deviceValue(VirtualPadKind::Psc, dualSense({{AbsLX, 255}}), 0x00) == 2);
        CHECK(deviceValue(VirtualPadKind::Psc, dualSense({{AbsLY, 0}}), 0x01) == 0);
        CHECK(deviceValue(VirtualPadKind::Psc, dualSense({{AbsLX, 224}}), 0x00) == 2);
        CHECK(deviceValue(VirtualPadKind::Psc, dualSense({{AbsLX, 255}}), 0x00, MovementAid::AsIs) == 1);
    }

    SUBCASE("the keys are the console's: BTN_A Triangle .. BTN_TR2 Start") {
        CHECK(deviceKey(VirtualPadKind::Psc, dualSense({}, {Triangle}), 0x130) == 1);
        CHECK(deviceKey(VirtualPadKind::Psc, dualSense({}, {Circle}), 0x131) == 1);
        CHECK(deviceKey(VirtualPadKind::Psc, dualSense({}, {Cross}), 0x132) == 1);
        CHECK(deviceKey(VirtualPadKind::Psc, dualSense({}, {Square}), 0x133) == 1);
        CHECK(deviceKey(VirtualPadKind::Psc, dualSense({{AbsL2, 255}}), 0x134) == 1);
        CHECK(deviceKey(VirtualPadKind::Psc, dualSense({{AbsR2, 255}}), 0x135) == 1);
        CHECK(deviceKey(VirtualPadKind::Psc, dualSense({}, {L1}), 0x136) == 1);
        CHECK(deviceKey(VirtualPadKind::Psc, dualSense({}, {R1}), 0x137) == 1);
        CHECK(deviceKey(VirtualPadKind::Psc, dualSense({}, {Create}), 0x138) == 1);
        CHECK(deviceKey(VirtualPadKind::Psc, dualSense({}, {Options}), 0x139) == 1);
    }
}

TEST_CASE("x360 output: the standard pad - six axes, the d-pad a hat, triggers at the bottom at rest") {
    const VirtualLayout &layout = virtualLayout(VirtualPadKind::X360);
    CHECK(layout.guid == "030000005e0400008e02000010010000");
    CHECK(layout.axisCount == 6);
    CHECK(layout.hatCount == 1);
    CHECK(layout.buttonCount == 11);
    CHECK(layout.vendor == 0x045e);
    CHECK(layout.controllerType == 1);

    // the order xpad reports: A B X Y LB RB Back Start Guide LS RS
    const Element order[] = {Element::A,
                             Element::B,
                             Element::X,
                             Element::Y,
                             Element::LeftShoulder,
                             Element::RightShoulder,
                             Element::Back,
                             Element::Start,
                             Element::Guide,
                             Element::LeftStick,
                             Element::RightStick};
    for (int i = 0; i < 11; ++i) {
        INFO("button " << i);
        RawPadState raw = x360Raw(pressing({order[i]}), MovementAid::AsIs);
        for (int b = 0; b < 11; ++b) {
            CHECK(raw.buttons[b] == (b == i));
        }
    }

    SUBCASE("rest: sticks in the middle, triggers at -32768, nothing pressed, no hat") {
        RawPadState raw = x360Raw(ControllerState());
        CHECK(raw.axes == vector<int16_t>{0, 0, -32768, 0, 0, -32768});
        CHECK(raw.hats[0] == 0);
    }

    SUBCASE(
        "a DualSense at rest or drifting: no button, no hat, triggers at the bottom, the sticks barely off centre") {
        for (const ControllerState &pad : {dualSense({}), dualSense({{AbsLX, 140}, {AbsLY, 116}})}) {
            RawPadState raw = x360Raw(pad);
            CHECK_FALSE(anyButton(raw));
            CHECK(raw.hats[0] == 0);
            CHECK(raw.axes[2] == -32768);
            CHECK(raw.axes[5] == -32768);
            CHECK(raw.axes[0] < 4000);
            CHECK(raw.axes[0] > -4000);
        }
    }

    SUBCASE("sticks and triggers on LX LY LT RX RY RT, the d-pad on the hat") {
        ControllerState physical = physicalPad({{Element::LeftX, 1000},
                                                {Element::LeftY, -2000},
                                                {Element::RightX, 3000},
                                                {Element::RightY, -4000},
                                                {Element::LeftTrigger, 32767},
                                                {Element::DpDown, 1},
                                                {Element::DpLeft, 1}});
        RawPadState raw = x360Raw(physical, MovementAid::AsIs);
        CHECK(raw.axes[0] == 1000);
        CHECK(raw.axes[1] == -2000);
        CHECK(raw.axes[2] == 32766);
        CHECK(raw.axes[3] == 3000);
        CHECK(raw.axes[4] == -4000);
        CHECK(raw.axes[5] == -32768);
        CHECK(raw.hats[0] == (4 | 8));
    }

    SUBCASE("the flags: Dpad2Analog - the d-pad moves the stick; Analog2Dpad - the stick presses the d-pad") {
        CHECK(x360Raw(dualSense({{HatX, -1}}), MovementAid::DpadToStick).axes[0] == -32767);
        CHECK(x360Raw(dualSense({{HatX, -1}}), MovementAid::AsIs).axes[0] == 128); // the stick at rest, as it is
        CHECK(x360Raw(dualSense({{AbsLX, 0}}), MovementAid::StickToDpad).hats[0] == 8);
        CHECK(x360Raw(dualSense({{AbsLX, 0}}), MovementAid::AsIs).hats[0] == 0);
        CHECK(x360Raw(dualSense({{AbsLY, 255}, {HatX, 1}}), MovementAid::Both).hats[0] == (2 | 4));
        CHECK(x360Raw(dualSense({{AbsLX, 150}}), MovementAid::Both).hats[0] == 0); // drift presses nothing
    }

    SUBCASE("the game-controller view is the physical pad as it is (as-is)") {
        ControllerState physical = physicalPad({{Element::RightX, 3000}, {Element::A, 1}});
        CHECK(controllerView(VirtualPadKind::X360, physical, MovementAid::AsIs) == physical);
    }
}

TEST_CASE("x360 output (kernel device): the triggers 0..255 at rest, the hat") {
    CHECK(deviceValue(VirtualPadKind::X360, dualSense({}), 0x02) == 0);
    CHECK(deviceValue(VirtualPadKind::X360, dualSense({}), 0x05) == 0);
    CHECK(deviceValue(VirtualPadKind::X360, dualSense({}), 0x10) == 0);
    CHECK(deviceValue(VirtualPadKind::X360, dualSense({{AbsL2, 255}}), 0x02) >= 254);
    CHECK(deviceValue(VirtualPadKind::X360, dualSense({{HatX, -1}}), 0x10) == -1);
    CHECK(deviceValue(VirtualPadKind::X360, dualSense({{AbsLX, 0}}), 0x10, MovementAid::StickToDpad) == -1);
    CHECK(deviceValue(VirtualPadKind::X360, dualSense({{AbsLX, 0}}), 0x10, MovementAid::AsIs) == 0);
}

TEST_CASE("a pad no database knows is given a mapping rather than left invisible") {
    // SDL only offers a pad as a GameController when it has a mapping for it, so the daemon makes one
    SUBCASE("a pad with a hat and two sticks") {
        PadMapping mapping = guessMapping("0300abcd", "Some Pad", 11, 4, 1);
        CHECK(mapping.guid == "0300abcd");
        CHECK(mapping.name == "Some Pad");
        CHECK(mapping[Element::A].index == 0);
        CHECK(mapping[Element::Y].index == 3);
        CHECK(mapping[Element::DpUp].kind == Binding::Kind::Hat);
        CHECK(mapping[Element::DpUp].hatMask == 1);
        CHECK(mapping[Element::LeftX].index == 0);
        CHECK(mapping[Element::RightY].index == 3);
        // and it has to come out as a line SDL will take
        CHECK(mapping.toLine().find("0300abcd,Some Pad,a:b0,") == 0);
        CHECK(mapping.toLine().find("platform:Linux,") != string::npos);
    }

    SUBCASE("a pad shaped like the PSC's: two axes, no hat, so they are the d-pad") {
        PadMapping mapping = guessMapping("0300", "", 10, 2, 0);
        CHECK(mapping.name == "Unmapped pad");
        CHECK(mapping[Element::DpLeft].kind == Binding::Kind::Axis);
        CHECK(mapping[Element::DpLeft].index == 0);
        CHECK(mapping[Element::DpLeft].half == -1);
        CHECK(mapping[Element::DpDown].index == 1);
        CHECK(mapping[Element::DpDown].half == 1);
        CHECK(mapping[Element::LeftX].bound() == false);
    }

    SUBCASE("a pad with fewer buttons than the guess has names for") {
        PadMapping mapping = guessMapping("0300", "Two", 2, 0, 0);
        CHECK(mapping[Element::A].bound());
        CHECK(mapping[Element::B].bound());
        CHECK(mapping[Element::X].bound() == false);
    }
}

TEST_CASE("the daemon's state reaches the shim through the shared block") {
    SharedState shared;
    initSharedState(shared);

    ControllerState pads[MaxPads];
    bool connected[MaxPads] = {true, false, false, false};
    pads[0] = pressing({Element::A, Element::DpUp});
    pads[0].set(Element::LeftX, static_cast<int16_t>(-30000));

    publishPads(shared, pads, connected, 1);
    publishPadIdentity(shared, 0, "Playstation Classic Controller", "030000004c050000da0c000011010000");

    Snapshot snapshot;
    REQUIRE(readSnapshot(shared, snapshot));
    CHECK(snapshot.padCount == 1);
    CHECK(snapshot.connected[0]);
    CHECK(snapshot.pads[0].button(Element::A));
    CHECK(snapshot.pads[0].button(Element::DpUp));
    CHECK(snapshot.pads[0].button(Element::B) == false);
    CHECK(snapshot.pads[0].axis(Element::LeftX) == -30000);
    CHECK(string(shared.pads[0].name) == "Playstation Classic Controller");

    SUBCASE("two pads at once, each its own player") {
        bool both[MaxPads] = {true, true, false, false};
        pads[1] = pressing({Element::Start});
        publishPads(shared, pads, both, 2);

        Snapshot two;
        REQUIRE(readSnapshot(shared, two));
        CHECK(two.padCount == 2);
        CHECK(two.pads[0].button(Element::A));
        CHECK(two.pads[0].button(Element::Start) == false);
        CHECK(two.pads[1].button(Element::Start));
        CHECK(two.pads[1].button(Element::A) == false);
    }

    SUBCASE("a pad unplugged leaves its slot empty and does not shuffle the others up") {
        bool second[MaxPads] = {false, true, false, false};
        pads[1] = pressing({Element::B});
        publishPads(shared, pads, second, 2);

        Snapshot gap;
        REQUIRE(readSnapshot(shared, gap));
        CHECK(gap.padCount == 2);
        CHECK(gap.connected[0] == false);
        CHECK(gap.pads[0].button(Element::A) == false); // its last state is not left behind
        CHECK(gap.connected[1]);
        CHECK(gap.pads[1].button(Element::B));
    }

    SUBCASE("a reader can tell a daemon that has stopped from one with nothing to say") {
        uint32_t first = snapshot.heartbeat;
        publishPads(shared, pads, connected, 1);
        Snapshot again;
        REQUIRE(readSnapshot(shared, again));
        CHECK(again.heartbeat == first + 1);
    }

    SUBCASE("a half-written block is not read") {
        shared.sequence.store(shared.sequence.load() + 1); // as the daemon leaves it mid-write
        Snapshot torn;
        CHECK(readSnapshot(shared, torn) == false);
    }

    SUBCASE("a block that is not ours is refused") {
        shared.magic = 0;
        Snapshot other;
        CHECK(readSnapshot(shared, other) == false);
        shared.magic = SharedMagic;
        shared.version = SharedVersion + 1;
        CHECK(readSnapshot(shared, other) == false);
    }

    SUBCASE("a quit request (the console's Reset button) reaches the reader as a new count") {
        uint32_t before = snapshot.quitRequests;
        requestQuit(shared);
        Snapshot asked;
        REQUIRE(readSnapshot(shared, asked));
        CHECK(asked.quitRequests == before + 1);
        // and the pads are untouched by it
        CHECK(asked.pads[0].button(Element::A));
        CHECK(shared.sequence.load() % 2 == 0);
    }

    SUBCASE("more pads than the block holds are cut off, not written past") {
        bool all[MaxPads] = {true, true, true, true};
        publishPads(shared, pads, all, 99);
        Snapshot many;
        REQUIRE(readSnapshot(shared, many));
        CHECK(many.padCount == MaxPads);
    }
}

TEST_CASE("the shared block is only useful if both sides agree on its shape") {
    // a change to either of these is a change to the protocol: bump SharedVersion with it
    CHECK(sizeof(SharedPad) == 120);
    CHECK(SharedVersion == 1);
    // quitRequests took one of the two reserved words: the block keeps its size, so an older shim
    // still reads a newer daemon's block (and ignores the count)
    CHECK(sizeof(SharedState) == 8 * sizeof(uint32_t) + MaxPads * sizeof(SharedPad));
    CHECK(ButtonElementCount <= 32); // the buttons are a bitmask in one word
    CHECK(MaxPads >= 2);             // two players is the point
}

TEST_CASE("a profile says what the shim is to do with the pad") {
    istringstream text("# wolf4sdl wants the keyboard\n"
                       "mode = keyboard\n"
                       "virtual = psc\n"
                       "players = 2\n"
                       "hotkey = start+select\n"
                       "key.a = Left Ctrl\n"
                       "key.dpup = Up   # trailing comments are not part of the value\n"
                       "key.triangle = Space\n"
                       "nonsense\n");
    Profile profile;
    profile.loadStream(text);

    CHECK(profile.mode == PadMode::Keyboard);
    CHECK(profile.wantsKeyboard());
    CHECK(profile.wantsJoystick() == false);
    CHECK(profile.virtualPad == VirtualPadKind::Psc);
    CHECK(profile.players == 2);
    CHECK(profile.keyFor(Element::A) == "Left Ctrl");
    CHECK(profile.keyFor(Element::DpUp) == "Up");
    CHECK(profile.keyFor(Element::Y) == "Space"); // named as the pad names it
    CHECK(profile.keyFor(Element::B).empty());
}

TEST_CASE("a profile's defaults are the ones an app that says nothing should get") {
    Profile profile;
    CHECK(profile.mode == PadMode::Joystick);
    CHECK(profile.virtualPad == VirtualPadKind::X360);
    CHECK(profile.players == 1);
    CHECK(profile.hotkey.empty());

    SUBCASE("and a second file is applied over the first") {
        istringstream first("mode = both\nplayers = 4\n");
        profile.loadStream(first);
        istringstream second("players = 1\n");
        profile.loadStream(second);
        CHECK(profile.mode == PadMode::Both);
        CHECK(profile.players == 1);
    }

    SUBCASE("an absurd player count is brought back into range") {
        istringstream text("players = 99\n");
        profile.loadStream(text);
        CHECK(profile.players == MaxPads);
        istringstream zero("players = 0\n");
        profile.loadStream(zero);
        CHECK(profile.players == 1);
    }
}

TEST_CASE("the hotkey is every named button held together") {
    vector<Element> hotkey = parseHotkey("start+select");
    REQUIRE(hotkey.size() == 2);
    CHECK(hotkey[0] == Element::Start);
    CHECK(hotkey[1] == Element::Back); // "select" is what the console's pad calls it

    ControllerState state;
    CHECK(hotkeyHeld(hotkey, state) == false);
    state.set(Element::Start, true);
    CHECK(hotkeyHeld(hotkey, state) == false); // one of the two is not enough
    state.set(Element::Back, true);
    CHECK(hotkeyHeld(hotkey, state));

    CHECK(hotkeyHeld(parseHotkey(""), state) == false); // no hotkey is never held
    CHECK(parseHotkey("start+nonsense").size() == 1);   // a typo costs that button, not the hotkey

    SUBCASE("a trigger counts as held when it is pulled") {
        vector<Element> triggers = parseHotkey("l2+r2");
        ControllerState pulled;
        pulled.set(Element::LeftTrigger, static_cast<int16_t>(32767));
        CHECK(hotkeyHeld(triggers, pulled) == false);
        pulled.set(Element::RightTrigger, static_cast<int16_t>(32767));
        CHECK(hotkeyHeld(triggers, pulled));
    }
}

TEST_CASE("the mode names a profile may use") {
    CHECK(padModeFromName("off") == PadMode::Off);
    CHECK(padModeFromName("KEYBOARD") == PadMode::Keyboard);
    CHECK(padModeFromName("both") == PadMode::Both);
    CHECK(padModeFromName("") == PadMode::Joystick);
    CHECK(string(padModeName(PadMode::Both)) == "both");
    CHECK(virtualPadKindFromName("psc") == VirtualPadKind::Psc);
    CHECK(virtualPadKindFromName("nonsense") == VirtualPadKind::X360);
    CHECK(string(virtualPadKindName(VirtualPadKind::X360)) == "x360");
}

TEST_CASE("a key a profile names, in both SDLs' numbers") {
    SUBCASE("the two SDLs disagree, which is the whole reason for the table") {
        KeyCode up = keyCodeFromName("Up");
        REQUIRE(up.valid());
        CHECK(up.sdl1Sym == 273);                  // SDL 1.2's SDLK_UP
        CHECK(up.sdl2Scancode == 82);              // SDL2's SDL_SCANCODE_UP
        CHECK(up.sdl2Keycode == (82 | (1 << 30))); // and its keycode, the masked scancode
    }

    SUBCASE("a key with a character is that character in both") {
        KeyCode a = keyCodeFromName("a");
        CHECK(a.sdl1Sym == 97);
        CHECK(a.sdl2Scancode == 4);
        CHECK(a.sdl2Keycode == 97);

        KeyCode space = keyCodeFromName("Space");
        CHECK(space.sdl1Sym == 32);
        CHECK(space.sdl2Scancode == 44);
        CHECK(space.sdl2Keycode == 32);
    }

    SUBCASE("the digits, whose scancodes do not run in the order you would guess") {
        CHECK(keyCodeFromName("1").sdl2Scancode == 30);
        CHECK(keyCodeFromName("9").sdl2Scancode == 38);
        CHECK(keyCodeFromName("0").sdl2Scancode == 39); // zero comes after nine, not before one
        CHECK(keyCodeFromName("0").sdl1Sym == 48);
    }

    SUBCASE("the function keys") {
        CHECK(keyCodeFromName("F1").sdl1Sym == 282);
        CHECK(keyCodeFromName("F1").sdl2Scancode == 58);
        CHECK(keyCodeFromName("F12").sdl1Sym == 293);
        CHECK(keyCodeFromName("F12").sdl2Scancode == 69);
        CHECK(keyCodeFromName("F13").valid() == false);
    }

    SUBCASE("however the profile spells a modifier") {
        for (const char *spelling : {"Left Ctrl", "left ctrl", "LCTRL", "left_ctrl", "ctrl"}) {
            KeyCode key = keyCodeFromName(spelling);
            CHECK(key.valid());
            CHECK(key.sdl1Sym == 306);
            CHECK(key.sdl2Scancode == 224);
        }
    }

    SUBCASE("a name nobody has is reported rather than silently sending nothing") {
        CHECK(keyCodeFromName("").valid() == false);
        CHECK(keyCodeFromName("wibble").valid() == false);
        CHECK(keyCodeFromName("F0").valid() == false);
    }
}

TEST_CASE("the cursor is hidden unless a profile asks to keep it") {
    Profile profile;
    CHECK(profile.hideCursor); // these machines have no mouse

    SUBCASE("a profile can ask for it back") {
        for (const char *spelling : {"keep", "show", "visible", "on"}) {
            Profile kept;
            istringstream text(string("cursor = ") + spelling + "\n");
            kept.loadStream(text);
            CHECK(kept.hideCursor == false);
        }
    }

    SUBCASE("and anything else means hide") {
        istringstream text("cursor = hide\n");
        profile.loadStream(text);
        CHECK(profile.hideCursor);
    }

    SUBCASE("it is independent of the mode, because an app needing no pad help may still show one") {
        istringstream text("mode = off\n");
        profile.loadStream(text);
        CHECK(profile.mode == PadMode::Off);
        CHECK(profile.hideCursor);
    }
}

TEST_CASE("the d-pad and the stick stand in for each other") {
    SUBCASE("a d-pad press moves the stick, which is what Doom reads") {
        ControllerState state = pressing({Element::DpLeft});
        applyMovementAid(state, MovementAid::DpadToStick);
        CHECK(state.axis(Element::LeftX) == -32767);
        CHECK(state.button(Element::DpLeft)); // and the d-pad still says so
        CHECK(state.button(Element::DpRight) == false);
    }

    SUBCASE("a pushed stick presses the d-pad, which is what a hat-only game reads") {
        ControllerState state;
        state.set(Element::LeftY, static_cast<int16_t>(-30000));
        applyMovementAid(state, MovementAid::StickToDpad);
        CHECK(state.button(Element::DpUp));
        CHECK(state.button(Element::DpDown) == false);
        CHECK(state.axis(Element::LeftY) == -30000); // and the stick is left alone
    }

    SUBCASE("a real stick beats a d-pad standing in for one") {
        ControllerState state = pressing({Element::DpRight});
        state.set(Element::LeftX, static_cast<int16_t>(-30000)); // a hand on the stick, going left
        applyMovementAid(state, MovementAid::Both);
        CHECK(state.axis(Element::LeftX) == -30000);
    }

    SUBCASE("both directions at once still work") {
        ControllerState state = pressing({Element::DpUp, Element::DpRight});
        applyMovementAid(state, MovementAid::Both);
        CHECK(state.axis(Element::LeftX) == 32767);
        CHECK(state.axis(Element::LeftY) == -32767);
    }

    SUBCASE("both is not a loop: feeding one from the other does not feed back") {
        ControllerState state = pressing({Element::DpUp});
        applyMovementAid(state, MovementAid::Both);
        ControllerState again = state;
        applyMovementAid(again, MovementAid::Both);
        CHECK(again == state);
    }

    SUBCASE("as-is leaves the pad exactly as the layout describes it") {
        ControllerState state = pressing({Element::DpLeft});
        ControllerState before = state;
        applyMovementAid(state, MovementAid::AsIs);
        CHECK(state == before);
    }

    SUBCASE("a resting pad stays resting") {
        ControllerState state;
        applyMovementAid(state, MovementAid::Both);
        CHECK(state == ControllerState());
    }

    SUBCASE("the names a profile may use") {
        CHECK(movementAidFromName("as-is") == MovementAid::AsIs);
        CHECK(movementAidFromName("dpad-to-stick") == MovementAid::DpadToStick);
        CHECK(movementAidFromName("stick-to-dpad") == MovementAid::StickToDpad);
        CHECK(movementAidFromName("") == MovementAid::Both);
        CHECK(string(movementAidName(MovementAid::DpadToStick)) == "dpad-to-stick");

        Profile profile;
        CHECK(profile.movement == MovementAid::Both); // the forgiving default
        istringstream text("movement = stick-to-dpad\n");
        profile.loadStream(text);
        CHECK(profile.movement == MovementAid::StickToDpad);
    }
}

namespace {

// a device with the plan's keys and axes, as the kernel would show it to a reader
EvdevPadState deviceOf(VirtualPadKind kind) {
    const UinputPlan &plan = uinputPlan(kind);
    vector<EvdevAbs> abs;
    for (const UinputAxis &axis : plan.abs) {
        abs.push_back({axis.code, axis.min, axis.max});
    }
    return EvdevPadState(plan.keys, abs);
}

// the frame a state puts on the virtual device, read back from the device the way abpadd reads a real one
ControllerState throughTheKernel(VirtualPadKind kind, const ControllerState &physical) {
    const VirtualLayout &layout = virtualLayout(kind);
    EvdevFrame frame = evdevFrame(kind, buildRawState(layout, controllerView(kind, physical, MovementAid::AsIs)));
    EvdevPadState device = deviceOf(kind);
    for (const auto &key : frame.keys) {
        device.setKey(key.first, key.second);
    }
    for (const auto &abs : frame.abs) {
        device.setAbs(abs.first, abs.second);
    }
    return applyMapping(layout.mapping, device.raw());
}
} // namespace

TEST_CASE("kernel pad: the virtual device is the layout's own pad - what an App reads from it is what the shim shows") {
    SUBCASE("the console pad: ten buttons from BTN_A, two axes of 0..2") {
        const UinputPlan &plan = uinputPlan(VirtualPadKind::Psc);
        CHECK(plan.name == "Sony Interactive Entertainment Controller");
        CHECK(plan.vendor == 0x054c);
        CHECK(plan.product == 0x0cda);
        CHECK(plan.version == 0x0111);
        REQUIRE(plan.keys.size() == 10);
        CHECK(plan.keys.front() == 0x130); // BTN_A
        CHECK(plan.keys.back() == 0x139);  // BTN_TR2
        REQUIRE(plan.abs.size() == 2);
        CHECK(plan.abs[0].max == 2);
        EvdevPadState device = deviceOf(VirtualPadKind::Psc);
        CHECK(device.buttonCount() == 10);
        CHECK(device.axisCount() == 2);
        CHECK(device.hatCount() == 0);
    }

    SUBCASE("the standard pad: eleven buttons, six axes, one hat") {
        const UinputPlan &plan = uinputPlan(VirtualPadKind::X360);
        CHECK(plan.vendor == 0x045e);
        CHECK(plan.product == 0x028e);
        EvdevPadState device = deviceOf(VirtualPadKind::X360);
        CHECK(device.buttonCount() == 11);
        CHECK(device.axisCount() == 6);
        CHECK(device.hatCount() == 1);
    }

    SUBCASE("every element survives the trip to the kernel and back, for both pads") {
        for (VirtualPadKind kind : {VirtualPadKind::Psc, VirtualPadKind::X360}) {
            for (Element element :
                 {Element::A, Element::B, Element::X, Element::Y, Element::Back, Element::Start, Element::LeftShoulder,
                  Element::RightShoulder, Element::DpUp, Element::DpDown, Element::DpLeft, Element::DpRight}) {
                INFO("pad " << virtualPadKindName(kind) << " element " << elementName(element));
                ControllerState physical = pressing({element});
                ControllerState read = throughTheKernel(kind, physical);
                ControllerState shown = controllerView(kind, physical, MovementAid::AsIs);
                CHECK(read == shown);
            }
            CHECK(throughTheKernel(kind, ControllerState()) ==
                  controllerView(kind, ControllerState(), MovementAid::AsIs));
        }
    }

    SUBCASE("the console pad: the stick is the d-pad, the right stick is nothing") {
        ControllerState read =
            throughTheKernel(VirtualPadKind::Psc, physicalPad({{Element::LeftX, 30000}, {Element::RightY, -30000}}));
        CHECK(read.axis(Element::LeftX) == 0); // as-is: the stick does not feed the d-pad
        CHECK(read.axis(Element::RightY) == 0);
        ControllerState fed = applyMapping(virtualLayout(VirtualPadKind::Psc).mapping, [] {
            EvdevPadState device = deviceOf(VirtualPadKind::Psc);
            EvdevFrame frame =
                evdevFrame(VirtualPadKind::Psc,
                           buildRawState(virtualLayout(VirtualPadKind::Psc),
                                         controllerView(VirtualPadKind::Psc, physicalPad({{Element::LeftX, 30000}}),
                                                        MovementAid::Both)));
            for (const auto &abs : frame.abs) {
                device.setAbs(abs.first, abs.second);
            }
            return device.raw();
        }());
        CHECK(fed.axis(Element::LeftX) == 32767);
    }

    SUBCASE("the standard pad keeps its sticks and triggers") {
        ControllerState physical = physicalPad(
            {{Element::LeftX, 12000}, {Element::RightY, -20000}, {Element::LeftTrigger, 32767}, {Element::DpLeft, 1}});
        ControllerState read = throughTheKernel(VirtualPadKind::X360, physical);
        CHECK(read.axis(Element::LeftX) > 11000);
        CHECK(read.axis(Element::LeftX) < 13000);
        CHECK(read.axis(Element::RightY) < -19000);
        CHECK(read.axis(Element::LeftTrigger) > 32000);
        CHECK(read.axis(Element::RightTrigger) == 0);
        CHECK(read.button(Element::DpLeft));
    }

    SUBCASE("a rest frame puts the console pad axes at their middle and the triggers at the bottom") {
        EvdevFrame psc = evdevFrame(VirtualPadKind::Psc, buildRawState(virtualLayout(VirtualPadKind::Psc), {}));
        CHECK(psc.abs[0].second == 1);
        CHECK(psc.abs[1].second == 1);
        EvdevFrame x360 = evdevFrame(VirtualPadKind::X360, buildRawState(virtualLayout(VirtualPadKind::X360), {}));
        CHECK(x360.abs[2].second == 0);
        CHECK(x360.abs[5].second == 0);
        CHECK(x360.abs[0].second == 0);
    }
}

TEST_CASE("kernel pad: a real pad read from its evdev node is numbered as SDL numbers it") {
    // a DualSense under hid-playstation: BTN_SOUTH.. in code order, X Y Z RX RY RZ and a hat
    const char *const line = "030000004c050000e60c000000810000,PS5 Controller,a:b0,b:b1,x:b3,y:b2,back:b8,guide:b10,"
                             "start:b9,leftstick:b11,rightstick:b12,leftshoulder:b4,rightshoulder:b5,"
                             "dpup:h0.1,dpdown:h0.4,dpleft:h0.8,dpright:h0.2,leftx:a0,lefty:a1,rightx:a2,"
                             "righty:a3,lefttrigger:a4,righttrigger:a5,platform:Linux,";
    PadMapping mapping;
    REQUIRE(PadMapping::parseLine(line, mapping));
    vector<int> keys = {0x130, 0x131, 0x132, 0x133, 0x134, 0x135, 0x136, 0x137, 0x138, 0x139, 0x13a, 0x13b, 0x13c};
    vector<EvdevAbs> abs = {{0x00, 0, 255}, {0x01, 0, 255}, {0x02, 0, 255}, {0x03, 0, 255},
                            {0x04, 0, 255}, {0x05, 0, 255}, {0x10, -1, 1},  {0x11, -1, 1}};
    EvdevPadState pad(keys, abs);
    CHECK(pad.buttonCount() == 13);
    CHECK(pad.axisCount() == 6);
    CHECK(pad.hatCount() == 1);

    pad.setKey(0x130, 1);
    CHECK(applyMapping(mapping, pad.raw()).button(Element::A));
    pad.setKey(0x130, 0);
    pad.setAbs(0x01, 0); // stick fully up
    CHECK(applyMapping(mapping, pad.raw()).axis(Element::LeftY) < -32000);
    pad.setAbs(0x01, 128);
    pad.setAbs(0x11, 1); // hat down
    ControllerState read = applyMapping(mapping, pad.raw());
    CHECK(read.button(Element::DpDown));
    CHECK_FALSE(read.button(Element::DpUp));
    pad.setAbs(0x11, 0);

    pad.setAbs(0x04, 0);
    CHECK(applyMapping(mapping, pad.raw()).axis(Element::LeftTrigger) == 0);
    pad.setAbs(0x04, 255);
    CHECK(applyMapping(mapping, pad.raw()).axis(Element::LeftTrigger) > 32000);
}

TEST_CASE("kernel pad: the console's own pad - two axes of 0..2 - reads as a d-pad") {
    PadMapping mapping;
    REQUIRE(PadMapping::parseLine(kPscLine, mapping));
    vector<int> keys;
    for (int code = 0x130; code <= 0x139; ++code) {
        keys.push_back(code);
    }
    EvdevPadState pad(keys, {{0, 0, 2}, {1, 0, 2}});
    pad.setAbs(0, 1);
    pad.setAbs(1, 1);
    ControllerState idle = applyMapping(mapping, pad.raw());
    CHECK_FALSE(idle.button(Element::DpLeft));
    CHECK_FALSE(idle.button(Element::DpRight));
    CHECK_FALSE(idle.button(Element::DpUp));
    CHECK_FALSE(idle.button(Element::DpDown));
    pad.setAbs(0, 0);
    CHECK(applyMapping(mapping, pad.raw()).button(Element::DpLeft));
    pad.setAbs(0, 2);
    CHECK(applyMapping(mapping, pad.raw()).button(Element::DpRight));
    pad.setAbs(0, 1);
    pad.setAbs(1, 0);
    CHECK(applyMapping(mapping, pad.raw()).button(Element::DpUp));
    pad.setKey(0x132, 1); // BTN_C = b2 = Cross = A
    CHECK(applyMapping(mapping, pad.raw()).button(Element::A));
}

namespace {
InputNodeFacts inputNode(const string &path, const string &group, bool gamepad, bool pointer, bool motion) {
    InputNodeFacts node;
    node.path = path;
    node.group = group;
    node.gamepad = gamepad;
    node.pointer = pointer;
    node.motion = motion;
    return node;
}
} // namespace

TEST_CASE("a pad's touchpad and motion sensors are held and hidden; its buttons, real mice and Reset are not") {
    const string dualSense = "/sys/devices/platform/usb/1-1/0003:054C:0CE6.0001";
    const string mouse = "/sys/devices/platform/usb/1-2/0003:046D:C077.0002";
    const string keyboard = "/sys/devices/platform/usb/1-3/0003:046D:C534.0003";
    // Reset; the pad's buttons, motion sensors and touchpad; a mouse; a keyboard's touchpad; our kernel pad; a pointer
    // of no known physical device
    const vector<InputNodeFacts> nodes = {
        inputNode("/dev/input/event0", "/sys/devices/platform/gpio-keys", false, false, false),
        inputNode("/dev/input/event1", dualSense, true, false, false),
        inputNode("/dev/input/event2", dualSense, false, false, true),
        inputNode("/dev/input/event3", dualSense, false, true, false),
        inputNode("/dev/input/event4", mouse, false, true, false),
        inputNode("/dev/input/event5", keyboard, false, true, false),
        inputNode("/dev/input/event6", "/sys/devices/virtual/input/input9", true, false, false),
        inputNode("/dev/input/event7", "", false, true, false),
    };
    CHECK(padPointerNodes(nodes) == vector<string>{"/dev/input/event2", "/dev/input/event3"});

    // a pad without a touchpad or sensors (the console's own): nothing
    CHECK(padPointerNodes({nodes[0], nodes[1]}).empty());
    // a touchpad that also reports buttons in the joystick range is still a pointer, and taken
    vector<InputNodeFacts> odd = {nodes[1], inputNode("/dev/input/event3", dualSense, true, true, false)};
    CHECK(padPointerNodes(odd) == vector<string>{"/dev/input/event3"});
    // no pad at all: a mouse stays the App's (and the compositor's)
    CHECK(padPointerNodes({nodes[4]}).empty());
}
