// padtest - what SDL tells a program about its joysticks, on a screen.
//
// The subject the shim is tested against. Run it twice, once plain and once with libabpad.so
// preloaded, and the difference between the two is what the shim does. It deliberately uses SDL's
// **joystick** API and nothing else, because that is the surface the shim stands in front of and the
// only one SDL 1.2 has.
//
//   padtest [--seconds N] [--plain]
//
//   ./padtest                                   what the app would see on its own
//   LD_PRELOAD=./libabpad.so ./padtest          what the shim shows it
//
// By default it draws a live panel with nothing but terminal escapes - no curses, no SDL video - so it
// runs on a console with the launcher stopped, on tty1 of a Pi, over ssh, anywhere. Press a button and
// see which one lights up: that is the whole test, and it is far quicker than reading a log and
// guessing what was pressed. --plain prints one line per event instead, for capturing into a report.
//
// The names in brackets are what an **Xbox 360 pad** calls each index - the layout the shim presents
// and the one nearly every Linux port assumes. With the shim in front of it, pressing Cross should
// light A(0). Without, it very likely will not, and the gap between those two is the bug this whole
// thing exists to close.
//
// Built against SDL2 here. The same source compiles against SDL 1.2 where a libSDL-1.2 is to hand,
// which is how the SDL1 half of the shim gets a subject of its own.

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#ifdef main
#undef main
#endif

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <string>

#ifndef _WIN32
#include <unistd.h>
#endif

using namespace std;

namespace {

constexpr int MaxPadsShown = 4;
constexpr int RecentLines = 6;

// what an Xbox 360 pad calls each index - the layout the shim presents
const char *const kButtonNames[] = {"A", "B", "X", "Y", "LB", "RB", "Back", "Start", "Guide", "LS", "RS"};
const char *const kAxisNames[] = {"LeftX", "LeftY", "LeftTrigger", "RightX", "RightY", "RightTrigger"};

const char *buttonName(int index) {
    int count = static_cast<int>(sizeof(kButtonNames) / sizeof(kButtonNames[0]));
    return (index >= 0 && index < count) ? kButtonNames[index] : "?";
}

const char *axisName(int index) {
    int count = static_cast<int>(sizeof(kAxisNames) / sizeof(kAxisNames[0]));
    return (index >= 0 && index < count) ? kAxisNames[index] : "?";
}

//*******************************
// Screen - the terminal, drawn over rather than scrolled
//*******************************
class Screen {
public:
    void begin() {
        printf("\x1b[?25l"); // the cursor would blink in the middle of the panel
        printf("\x1b[2J");
    }
    void end() {
        printf("\x1b[?25h\x1b[%d;1H\n", line_ + 2);
        fflush(stdout);
    }
    void home() {
        printf("\x1b[H");
        line_ = 0;
    }
    // every line is cleared to its end, so a shorter line never leaves the last frame's tail behind
    void put(const char *format, ...) {
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
        printf("\x1b[K\n");
        ++line_;
    }
    void flush() { fflush(stdout); }

private:
    int line_ = 0;
};

string bar(int value, int width) {
    // a bar from the middle, so a stick at rest is a single mark in the centre and a trigger at rest
    // is hard against the left - which is what they should look like
    int half = width / 2;
    int offset = static_cast<int>(static_cast<long>(value) * half / 32768);
    string text(static_cast<size_t>(width), ' ');
    text[static_cast<size_t>(half)] = '|';
    int from = offset < 0 ? half + offset : half;
    int to = offset < 0 ? half : half + offset;
    for (int i = from; i <= to && i < width; ++i) {
        if (i >= 0) {
            text[static_cast<size_t>(i)] = '=';
        }
    }
    return text;
}

const char *pressed(bool down) {
    return down ? "\x1b[7m" : ""; // reverse video: legible on any console, needs no colour support
}

} // namespace

//*******************************
// The guided mapper: --map
//*******************************
// A pad whose gamecontrollerdb line is wrong (or missing) is the commonest reason an App takes the pad
// badly, and reading a log of raw indices means remembering which button was pressed when. So the
// program asks for one input at a time and writes the line itself. Same idea as the pscbios wizard on
// the console, in the one place a Pi or a PC has no wizard.
namespace {

struct MapStep {
    const char *element; // the name a gamecontrollerdb line uses
    const char *ask;     // what to say, in the names on the pad itself
    bool axisElement;    // a stick or a trigger: binds the whole axis, not one half of it
};

const MapStep kSteps[] = {
    {"a", "Cross", false},
    {"b", "Circle", false},
    {"x", "Square", false},
    {"y", "Triangle", false},
    {"back", "Select", false},
    {"start", "Start", false},
    {"guide", "PS / Home", false},
    {"leftshoulder", "L1", false},
    {"rightshoulder", "R1", false},
    {"lefttrigger", "L2", true},
    {"righttrigger", "R2", true},
    {"leftstick", "L3 (press the left stick in)", false},
    {"rightstick", "R3 (press the right stick in)", false},
    {"dpup", "D-pad UP", false},
    {"dpdown", "D-pad DOWN", false},
    {"dpleft", "D-pad LEFT", false},
    {"dpright", "D-pad RIGHT", false},
    {"leftx", "left stick to the RIGHT", true},
    {"lefty", "left stick DOWN", true},
    {"rightx", "right stick to the RIGHT", true},
    {"righty", "right stick DOWN", true},
};

struct PadSnapshot {
    Uint8 buttons[32] = {};
    Sint16 axes[16] = {};
    Uint8 hats[4] = {};
    int buttonCount = 0, axisCount = 0, hatCount = 0;
};

PadSnapshot sample(SDL_Joystick *pad) {
    SDL_JoystickUpdate();
    PadSnapshot now;
    now.buttonCount = SDL_JoystickNumButtons(pad);
    now.axisCount = SDL_JoystickNumAxes(pad);
    now.hatCount = SDL_JoystickNumHats(pad);
    for (int i = 0; i < now.buttonCount && i < 32; ++i) {
        now.buttons[i] = SDL_JoystickGetButton(pad, i);
    }
    for (int i = 0; i < now.axisCount && i < 16; ++i) {
        now.axes[i] = SDL_JoystickGetAxis(pad, i);
    }
    for (int i = 0; i < now.hatCount && i < 4; ++i) {
        now.hats[i] = SDL_JoystickGetHat(pad, i);
    }
    return now;
}

// the first input that has moved away from where it was resting, as a mapping line writes it
string changedSince(const PadSnapshot &rest, const PadSnapshot &now, bool wholeAxis = false) {
    char text[32];
    for (int i = 0; i < now.buttonCount && i < 32; ++i) {
        if (now.buttons[i] && !rest.buttons[i]) {
            snprintf(text, sizeof(text), "b%d", i);
            return text;
        }
    }
    for (int i = 0; i < now.hatCount && i < 4; ++i) {
        Uint8 fresh = static_cast<Uint8>(now.hats[i] & ~rest.hats[i]);
        for (int mask = 1; mask <= 8; mask <<= 1) {
            if (fresh & mask) {
                snprintf(text, sizeof(text), "h%d.%d", i, mask);
                return text;
            }
        }
    }
    for (int i = 0; i < now.axisCount && i < 16; ++i) {
        int delta = now.axes[i] - rest.axes[i];
        if (delta > 14000 || delta < -14000) {
            if (wholeAxis) {
                // a stick or a trigger binds its whole travel: half of one would leave the other half
                // dead. Which way it went says only whether the axis runs backwards.
                snprintf(text, sizeof(text), "a%d%s", i, delta < 0 ? "~" : "");
            } else {
                // a d-pad direction on an axis is one half of it, and which half matters
                snprintf(text, sizeof(text), "%sa%d", rest.axes[i] < -20000 ? "" : (delta > 0 ? "+" : "-"), i);
            }
            return text;
        }
    }
    return "";
}

bool anythingMoved(const PadSnapshot &rest, const PadSnapshot &now) {
    return !changedSince(rest, now).empty();
}

int runMapper(SDL_Joystick *pad, const char *writePath, int holdSeconds) {
    SDL_JoystickGUID guid = SDL_JoystickGetGUID(pad);
    char guidText[64];
    SDL_JoystickGetGUIDString(guid, guidText, sizeof(guidText));
    const char *name = SDL_JoystickName(pad);

    printf("\n  Mapping \"%s\"\n  %s\n\n", name ? name : "(no name)", guidText);
    printf("  Press what is asked for, one at a time. If your pad has no such control,\n");
    printf("  press nothing and it moves on after a few seconds.\n\n");
    fflush(stdout);

    string line = string(guidText) + "," + (name ? name : "pad") + ",";
    int stepNumber = 0;
    const int stepCount = static_cast<int>(sizeof(kSteps) / sizeof(kSteps[0]));
    for (const MapStep &step : kSteps) {
        ++stepNumber;
        // wait for everything to come to rest, then take that as the baseline
        PadSnapshot rest = sample(pad);
        Uint32 settle = SDL_GetTicks();
        while (SDL_GetTicks() - settle < 300) {
            SDL_Event drain;
            while (SDL_PollEvent(&drain)) {
            }
            PadSnapshot now = sample(pad);
            if (anythingMoved(rest, now)) {
                rest = now;
                settle = SDL_GetTicks();
            }
            SDL_Delay(20);
        }

        printf("  %2d/%d  %-34s ", stepNumber, stepCount, step.ask);
        fflush(stdout);

        string found;
        Uint32 deadline = SDL_GetTicks() + static_cast<Uint32>(holdSeconds) * 1000;
        while (SDL_GetTicks() < deadline) {
            SDL_Event drain;
            while (SDL_PollEvent(&drain)) {
            }
            found = changedSince(rest, sample(pad), step.axisElement);
            if (!found.empty()) {
                break;
            }
            SDL_Delay(20);
        }

        if (found.empty()) {
            printf("- skipped\n");
        } else {
            printf("%s\n", found.c_str());
            line += string(step.element) + ":" + found + ",";
        }
        fflush(stdout);
    }
    line += "platform:Linux,";

    printf("\n  The line for this pad:\n\n%s\n\n", line.c_str());
    if (writePath && *writePath) {
        FILE *file = fopen(writePath, "a");
        if (file) {
            fprintf(file, "%s\n", line.c_str());
            fclose(file);
            printf("  appended to %s\n\n", writePath);
        } else {
            printf("  could not write to %s\n\n", writePath);
        }
    }
    fflush(stdout);
    return 0;
}

} // namespace

int main(int argc, char *argv[]) {
    int seconds = 0; // 0 = until the hotkey or ctrl-c
    bool plain = false;
    bool mapping = false;
    int settleMs = 3000;
    int holdSeconds = 15;
    const char *writePath = nullptr;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
            seconds = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--plain") == 0) {
            plain = true;
        } else if (strcmp(argv[i], "--map") == 0) {
            mapping = true;
        } else if (strcmp(argv[i], "--write") == 0 && i + 1 < argc) {
            writePath = argv[++i];
        } else if (strcmp(argv[i], "--settle") == 0 && i + 1 < argc) {
            settleMs = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--hold") == 0 && i + 1 < argc) {
            holdSeconds = atoi(argv[++i]);
        }
    }
    if (mapping) {
        plain = true; // the mapper talks in lines, not a panel
    }
#ifndef _WIN32
    if (!isatty(1)) {
        plain = true; // being piped into a file: the panel would be a mess of escapes
    }
#endif

    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_JOYSTICK) != 0) {
        printf("SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    int count = SDL_NumJoysticks();
    SDL_Joystick *pads[MaxPadsShown] = {};
    for (int i = 0; i < count && i < MaxPadsShown; ++i) {
        pads[i] = SDL_JoystickOpen(i);
    }
    if (count <= 0) {
        printf("SDL_NumJoysticks() = 0 - is a pad plugged in, and is abpadd running?\n");
        SDL_Quit();
        return 1;
    }

    if (mapping) {
        int result = pads[0] ? runMapper(pads[0], writePath, holdSeconds) : 1;
        for (SDL_Joystick *pad : pads) {
            if (pad) {
                SDL_JoystickClose(pad);
            }
        }
        SDL_Quit();
        return result;
    }

    const char *preload = getenv("LD_PRELOAD");
    deque<string> recent;
    Screen screen;
    if (!plain) {
        screen.begin();
    } else {
        printf("SDL_NumJoysticks() = %d\n", count);
        for (int i = 0; i < count && i < MaxPadsShown; ++i) {
            if (pads[i]) {
                const char *name = SDL_JoystickName(pads[i]);
                printf("[%d] %s\n", i, name ? name : "(no name)");
                printf("     %d buttons, %d axes, %d hats\n", SDL_JoystickNumButtons(pads[i]),
                       SDL_JoystickNumAxes(pads[i]), SDL_JoystickNumHats(pads[i]));
            }
        }
        printf("\nreading - press things\n");
    }

    bool running = true;
    Uint32 until = seconds > 0 ? SDL_GetTicks() + static_cast<Uint32>(seconds) * 1000 : 0;
    while (running && (until == 0 || SDL_GetTicks() < until)) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            char line[128] = {};
            switch (event.type) {
            case SDL_JOYBUTTONDOWN:
            case SDL_JOYBUTTONUP:
                snprintf(line, sizeof(line), "pad %d  button %d (%s) %s", event.jbutton.which, event.jbutton.button,
                         buttonName(event.jbutton.button), event.type == SDL_JOYBUTTONDOWN ? "down" : "up");
                break;
            case SDL_JOYHATMOTION:
                snprintf(line, sizeof(line), "pad %d  hat %d -> %d", event.jhat.which, event.jhat.hat,
                         event.jhat.value);
                break;
            case SDL_JOYAXISMOTION:
                if (event.jaxis.value > 16000 || event.jaxis.value < -16000) {
                    snprintf(line, sizeof(line), "pad %d  axis %d (%s) %d", event.jaxis.which, event.jaxis.axis,
                             axisName(event.jaxis.axis), event.jaxis.value);
                }
                break;
            case SDL_QUIT:
                snprintf(line, sizeof(line), "SDL_QUIT - the shim's hotkey");
                running = false;
                break;
            default:
                break;
            }
            if (line[0]) {
                if (plain) {
                    printf("%s\n", line);
                    fflush(stdout);
                } else {
                    recent.push_back(line);
                    while (static_cast<int>(recent.size()) > RecentLines) {
                        recent.pop_front();
                    }
                }
            }
        }

        if (!plain) {
            screen.home();
            screen.put("\x1b[1m AutoBleem padtest\x1b[0m   what SDL tells this program about its joysticks");
            screen.put(" LD_PRELOAD: %s", (preload && *preload) ? preload : "(none - this is the pad as it really is)");
            screen.put("");

            for (int pad = 0; pad < count && pad < MaxPadsShown; ++pad) {
                if (!pads[pad]) {
                    continue;
                }
                const char *name = SDL_JoystickName(pads[pad]);
                int buttons = SDL_JoystickNumButtons(pads[pad]);
                int axes = SDL_JoystickNumAxes(pads[pad]);
                int hats = SDL_JoystickNumHats(pads[pad]);
                screen.put(" \x1b[1mjoystick %d\x1b[0m  %s", pad, name ? name : "(no name)");
                screen.put("             %d buttons, %d axes, %d hats", buttons, axes, hats);
                screen.put("");

                string names = " buttons  ";
                string marks = "          ";
                for (int i = 0; i < buttons && i < 16; ++i) {
                    char cell[64];
                    snprintf(cell, sizeof(cell), "%-6s", buttonName(i));
                    names += cell;
                    bool down = SDL_JoystickGetButton(pads[pad], i) != 0;
                    snprintf(cell, sizeof(cell), "%s%d:%s\x1b[0m   ", pressed(down), i, down ? "##" : "..");
                    marks += cell;
                }
                screen.put("%s", names.c_str());
                screen.put("%s", marks.c_str());
                screen.put("");

                for (int i = 0; i < hats && i < 2; ++i) {
                    Uint8 value = SDL_JoystickGetHat(pads[pad], i);
                    screen.put(" hat %d    %sup\x1b[0m  %sright\x1b[0m  %sdown\x1b[0m  %sleft\x1b[0m   (value %d)", i,
                               pressed((value & 1) != 0), pressed((value & 2) != 0), pressed((value & 4) != 0),
                               pressed((value & 8) != 0), value);
                }
                screen.put("");

                for (int i = 0; i < axes && i < 8; ++i) {
                    Sint16 value = SDL_JoystickGetAxis(pads[pad], i);
                    screen.put(" axis %d   %-13s [%s] %7d", i, axisName(i), bar(value, 32).c_str(), value);
                }
                screen.put("");
            }

            screen.put(" \x1b[1mrecent\x1b[0m");
            for (const string &line : recent) {
                screen.put("   %s", line.c_str());
            }
            for (int i = static_cast<int>(recent.size()); i < RecentLines; ++i) {
                screen.put("");
            }
            screen.put("");
            screen.put(
                " With the shim in front, Cross should light \x1b[1mA(0)\x1b[0m and the d-pad should move the hat.");
            screen.put(" ctrl-c to quit%s", seconds > 0 ? ", or it stops on its own" : "");
            screen.flush();
        }
        SDL_Delay(40);
    }

    if (!plain) {
        screen.end();
    }
    for (SDL_Joystick *pad : pads) {
        if (pad) {
            SDL_JoystickClose(pad);
        }
    }
    SDL_Quit();
    return 0;
}
