// abpad_core: SDL's evdev numbering, the GUID, the gamecontrollerdb line parser, and the two
// translations the shim is made of - a physical pad read through its mapping, and the result written
// back out as the raw state of a well-known pad.

#include "core/evdev_codes.h"
#include "core/evdev_order.h"
#include "core/mapping.h"
#include "core/virtual_pad.h"
#include "doctest/doctest.h"

#include <fstream>
#include <sstream>

using namespace abpad;
using namespace std;

namespace {

// the real line for the console's own pad, as src/resources/gamecontrollerdb.txt carries it
const char *const kPscLine = "030000004c050000da0c000011010000,Playstation Classic Controller,"
                             "a:b2,b:b1,back:b8,leftshoulder:b6,lefttrigger:b4,rightshoulder:b7,"
                             "righttrigger:b5,start:b9,x:b3,y:b0,dpdown:+a1,dpleft:-a0,dpright:+a0,"
                             "dpup:-a1,platform:Linux,";

// the PSC pad as the kernel describes it: ten buttons from BTN_TRIGGER up, two axes, no hat
EvdevCaps pscPadCaps() {
    EvdevCaps caps;
    for (int code = BTN_TRIGGER_; code < BTN_TRIGGER_ + 10; ++code) {
        caps.keys.push_back(code);
    }
    caps.axes.push_back(ABS_X_);
    caps.axes.push_back(ABS_Y_);
    return caps;
}

// an Xbox 360 pad as xpad presents it: BTN_GAMEPAD-range buttons, six axes and one hat
EvdevCaps x360PadCaps() {
    EvdevCaps caps;
    caps.keys = {BTN_SOUTH_,  BTN_EAST_,  BTN_NORTH_, BTN_WEST_,   BTN_TL_,    BTN_TR_,
                 BTN_SELECT_, BTN_START_, BTN_MODE_,  BTN_THUMBL_, BTN_THUMBR_};
    caps.axes = {ABS_X_, ABS_Y_, ABS_Z_, ABS_RX_, ABS_RY_, ABS_RZ_, ABS_HAT0X_, ABS_HAT0Y_};
    return caps;
}

PhysicalState stateFor(const SdlJoystickLayout &layout) {
    PhysicalState state;
    state.buttons.assign(layout.buttons.size(), false);
    state.axes.assign(layout.axes.size(), 0);
    state.hats.assign(layout.hats.size(), 0);
    return state;
}

} // namespace

TEST_CASE("SDL numbers the joystick/gamepad key codes before the BTN_MISC range") {
    EvdevCaps caps;
    // a pad that reports one key below BTN_JOYSTICK and two above it, listed in code order
    caps.keys = {0x101, BTN_TRIGGER_, BTN_THUMB_};

    SdlJoystickLayout layout = enumerateLikeSdl(caps);

    REQUIRE(layout.buttons.size() == 3);
    CHECK(layout.buttons[0] == BTN_TRIGGER_); // the joystick range first...
    CHECK(layout.buttons[1] == BTN_THUMB_);
    CHECK(layout.buttons[2] == 0x101); // ...then what is below it
    CHECK(layout.buttonForCode(BTN_THUMB_) == 1);
    CHECK(layout.buttonForCode(BTN_BASE_) == -1);
}

TEST_CASE("the hat axes are pulled out of the axis numbering") {
    SdlJoystickLayout layout = enumerateLikeSdl(x360PadCaps());

    REQUIRE(layout.axes.size() == 6);
    CHECK(layout.axes[0] == ABS_X_);
    CHECK(layout.axes[2] == ABS_Z_);
    CHECK(layout.axes[5] == ABS_RZ_);
    REQUIRE(layout.hats.size() == 1);
    CHECK(layout.hats[0].first == ABS_HAT0X_);
    CHECK(layout.hats[0].second == ABS_HAT0Y_);
    CHECK(layout.axisForCode(ABS_HAT0X_) == -1); // a hat is not an axis
    CHECK(layout.hatForCode(ABS_HAT0Y_) == pair<int, int>(0, 1));
}

TEST_CASE("a hat exists when the device reports only one half of it") {
    EvdevCaps caps;
    caps.axes = {ABS_HAT0X_};
    CHECK(enumerateLikeSdl(caps).hats.size() == 1);
}

TEST_CASE("the PSC pad numbers out as its gamecontrollerdb line says") {
    SdlJoystickLayout layout = enumerateLikeSdl(pscPadCaps());

    CHECK(layout.buttons.size() == 10);
    CHECK(layout.axes.size() == 2);
    CHECK(layout.hats.empty());
    // the line says y:b0 and a:b2 - so the first button the kernel reports is Triangle, which is the
    // whole reason an app that hardcodes "button 0" fires on the wrong one
    CHECK(layout.buttonForCode(BTN_TRIGGER_) == 0);
    CHECK(layout.buttonForCode(BTN_THUMB2_) == 2);
}

TEST_CASE("the GUID is formed the way SDL forms it") {
    // 030000004c050000da0c000011010000 - the PSC pad: USB, Sony, product 0cda, version 0111
    CHECK(guidFromIds(0x0003, 0x054c, 0x0cda, 0x0111) == "030000004c050000da0c000011010000");
    // 030000005e0400008e02000010010000 - the wired Xbox 360 pad
    CHECK(guidFromIds(0x0003, 0x045e, 0x028e, 0x0110) == "030000005e0400008e02000010010000");
}

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

    // the line it writes back is one SDL would take, and parses to the same thing
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

TEST_CASE("the db is looked up by GUID and the last line for one wins") {
    istringstream text(string("# a comment\n") + kPscLine + "\n" +
                       "030000004c050000da0c000011010000,Renamed,a:b5,platform:Linux,\n"
                       "030000005e0400008e02000010010000,X360,a:b0,platform:Linux,\n");
    MappingDb db;
    db.loadStream(text);

    CHECK(db.size() == 2);
    PadMapping found;
    REQUIRE(db.find("030000004c050000da0c000011010000", found));
    CHECK(found.name == "Renamed");
    CHECK(found[Element::A].index == 5);
    CHECK(db.find("00000000000000000000000000000000", found) == false);
}

TEST_CASE("a physical pad read through its mapping") {
    PadMapping mapping;
    REQUIRE(PadMapping::parseLine(kPscLine, mapping));
    SdlJoystickLayout layout = enumerateLikeSdl(pscPadCaps());
    PhysicalState state = stateFor(layout);

    SUBCASE("a face button") {
        state.buttons[2] = true; // b2 is Cross
        ControllerState controller = applyMapping(mapping, state);
        CHECK(controller.button(Element::A));
        CHECK(controller.button(Element::B) == false);
        CHECK(controller.button(Element::Y) == false);
    }

    SUBCASE("the d-pad on an axis") {
        state.axes[1] = -32768; // the negative half of axis 1 is dpup
        ControllerState controller = applyMapping(mapping, state);
        CHECK(controller.button(Element::DpUp));
        CHECK(controller.button(Element::DpDown) == false);
    }

    SUBCASE("an axis barely off centre is not a press") {
        state.axes[0] = 1000;
        CHECK(applyMapping(mapping, state).button(Element::DpRight) == false);
        state.axes[0] = AxisButtonThreshold;
        CHECK(applyMapping(mapping, state).button(Element::DpRight));
    }

    SUBCASE("a trigger the pad reports as a button reads as a full pull") {
        state.buttons[4] = true; // lefttrigger:b4
        CHECK(applyMapping(mapping, state).axis(Element::LeftTrigger) == 32767);
    }

    SUBCASE("nothing pressed is nothing set") {
        CHECK(applyMapping(mapping, state) == ControllerState());
    }
}

TEST_CASE("a stick read through a mapping keeps its value and its sign") {
    PadMapping mapping;
    REQUIRE(PadMapping::parseLine("0300,Pad,leftx:a0,lefty:a1~,lefttrigger:a2,", mapping));
    PhysicalState state;
    state.axes = {-20000, 20000, 30000};

    ControllerState controller = applyMapping(mapping, state);
    CHECK(controller.axis(Element::LeftX) == -20000);
    CHECK(controller.axis(Element::LeftY) == -20000); // the '~' turns it over
    CHECK(controller.axis(Element::LeftTrigger) == 30000);
}

TEST_CASE("the PSC pad shown to an app as an Xbox 360 pad") {
    PadMapping mapping;
    REQUIRE(PadMapping::parseLine(kPscLine, mapping));
    const VirtualLayout &layout = virtualLayout(VirtualPadKind::X360);
    SdlJoystickLayout physical = enumerateLikeSdl(pscPadCaps());
    PhysicalState state = stateFor(physical);

    CHECK(layout.name == "Microsoft X-Box 360 pad");
    CHECK(layout.buttonCount == 11);
    CHECK(layout.axisCount == 6);
    CHECK(layout.hatCount == 1);

    SUBCASE("Cross becomes button 0, which is what the app calls A") {
        state.buttons[2] = true;
        PhysicalState raw = buildRawState(layout, applyMapping(mapping, state));
        REQUIRE(raw.buttons.size() == 11);
        CHECK(raw.buttons[0]);
        CHECK(raw.buttons[1] == false);
    }

    SUBCASE("Triangle stops being button 0") {
        state.buttons[0] = true; // y:b0 on the PSC pad
        PhysicalState raw = buildRawState(layout, applyMapping(mapping, state));
        CHECK(raw.buttons[0] == false);
        CHECK(raw.buttons[3]); // y:b3 on the X360 pad
    }

    SUBCASE("the d-pad arrives on the hat the app reads") {
        state.axes[1] = -32768;
        PhysicalState raw = buildRawState(layout, applyMapping(mapping, state));
        REQUIRE(raw.hats.size() == 1);
        CHECK(raw.hats[0] == 1); // up
        CHECK(raw.axes[0] == 0); // and not on the left stick
    }

    SUBCASE("two d-pad directions at once are one hat value") {
        state.axes[1] = -32768;
        state.axes[0] = 32767;
        PhysicalState raw = buildRawState(layout, applyMapping(mapping, state));
        CHECK(raw.hats[0] == (1 | 2)); // up and right
    }

    SUBCASE("an untouched trigger rests at the bottom of its travel") {
        PhysicalState raw = buildRawState(layout, applyMapping(mapping, state));
        CHECK(raw.axes[2] == -32768);
        CHECK(raw.axes[5] == -32768);
        CHECK(raw.axes[0] == 0); // a stick still rests in the middle
    }

    SUBCASE("a pulled trigger reaches the top of it") {
        state.buttons[4] = true; // the PSC pad's L2, a button
        PhysicalState raw = buildRawState(layout, applyMapping(mapping, state));
        CHECK(raw.axes[2] == 32766);
    }
}

TEST_CASE("an Xbox pad shown to an app that was ported for the PSC pad") {
    PadMapping mapping;
    REQUIRE(PadMapping::parseLine("030000005e0400008e02000010010000,Microsoft X-Box 360 pad,"
                                  "a:b0,b:b1,x:b2,y:b3,dpup:h0.1,dpleft:h0.8,leftx:a0,lefty:a1,"
                                  "lefttrigger:a2,platform:Linux,",
                                  mapping));
    const VirtualLayout &layout = virtualLayout(VirtualPadKind::Psc);
    PhysicalState state;
    state.buttons.assign(11, false);
    state.axes.assign(6, 0);
    state.hats.assign(1, 0);

    SUBCASE("A becomes the PSC pad's Cross, button 2") {
        state.buttons[0] = true;
        PhysicalState raw = buildRawState(layout, applyMapping(mapping, state));
        REQUIRE(raw.buttons.size() == 10);
        CHECK(raw.buttons[2]);
        CHECK(raw.axes.size() == 2);
        CHECK(raw.hats.empty()); // the PSC pad has no hat to put anything on
    }

    SUBCASE("the hat becomes the d-pad on the axes") {
        state.hats[0] = 8; // left
        PhysicalState raw = buildRawState(layout, applyMapping(mapping, state));
        CHECK(raw.axes[0] == -32767);
        CHECK(raw.axes[1] == 0);
    }

    SUBCASE("a trigger on an axis becomes a button") {
        state.axes[2] = 32767;
        PhysicalState raw = buildRawState(layout, applyMapping(mapping, state));
        CHECK(raw.buttons[4]);
    }
}

TEST_CASE("an unmapped pad is guessed at rather than left dead") {
    SUBCASE("a pad with a hat and two sticks") {
        PadMapping mapping = guessMapping(11, 4, 1);
        CHECK(mapping[Element::A].index == 0);
        CHECK(mapping[Element::Y].index == 3);
        CHECK(mapping[Element::DpUp].kind == Binding::Kind::Hat);
        CHECK(mapping[Element::DpUp].hatMask == 1);
        CHECK(mapping[Element::LeftX].index == 0);
        CHECK(mapping[Element::RightY].index == 3);
    }

    SUBCASE("a pad shaped like the PSC's: two axes, no hat, so they are the d-pad") {
        PadMapping mapping = guessMapping(10, 2, 0);
        CHECK(mapping[Element::DpLeft].kind == Binding::Kind::Axis);
        CHECK(mapping[Element::DpLeft].index == 0);
        CHECK(mapping[Element::DpLeft].half == -1);
        CHECK(mapping[Element::DpDown].index == 1);
        CHECK(mapping[Element::DpDown].half == 1);
        CHECK(mapping[Element::LeftX].bound() == false);
    }

    SUBCASE("a pad with fewer buttons than the guess has names for") {
        PadMapping mapping = guessMapping(2, 0, 0);
        CHECK(mapping[Element::A].bound());
        CHECK(mapping[Element::B].bound());
        CHECK(mapping[Element::X].bound() == false);
    }
}

TEST_CASE("the shipped gamecontrollerdb.txt parses, and the pads we care about are in it") {
    MappingDb db;
    REQUIRE(db.loadFile(string(AB_RESOURCES_DIR) + "/gamecontrollerdb.txt"));
    CHECK(db.size() > 100);

    PadMapping psc;
    REQUIRE(db.find("030000004c050000da0c000011010000", psc));
    CHECK(psc[Element::A].index == 2); // Cross, as the line at the top of this file says

    // the layouts the shim can present must be describable as real lines
    CHECK(virtualLayout(VirtualPadKind::X360).mapping[Element::A].index == 0);
    CHECK(virtualPadKindFromName("psc") == VirtualPadKind::Psc);
    CHECK(virtualPadKindFromName("nonsense") == VirtualPadKind::X360);
    CHECK(string(virtualPadKindName(VirtualPadKind::X360)) == "x360");
}
