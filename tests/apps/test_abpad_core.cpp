// abpad_core: what the virtual gamepad is made of once SDL does the reading - the gamecontrollerdb
// line a layout is written as, the controller state the daemon publishes turned into the raw pad an
// app's SDL is answered with, the shared-memory handover between the two, the per-app profile and the
// key table keyboard mode sends.

#include "core/key_names.h"
#include "core/mapping.h"
#include "core/profile.h"
#include "core/shared_state.h"
#include "core/virtual_pad.h"
#include "doctest/doctest.h"

#include <initializer_list>
#include <sstream>
#include <string>

using namespace abpad;
using namespace std;

namespace {

// the real line for the console's own pad, as src/resources/gamecontrollerdb.txt carries it. The shim
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
        RawPadState raw = buildRawState(layout, pressing({Element::DpLeft}));
        CHECK(raw.axes[0] == -32767);
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
