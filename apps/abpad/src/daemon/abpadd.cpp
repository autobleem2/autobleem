// abpadd - the virtual gamepad.
//
// The one program that reads the pads. It runs SDL2 with our gamecontrollerdb.txt and reads every pad
// through the GameController API, so a pad resolves here exactly as it resolves in the launcher and
// in pcsx - same code, same database, no second opinion - and publishes the result in shared memory
// for the preloaded shim in each App to answer that App's SDL with.
//
// It is a separate process because it has to be: an SDL 1.2 app cannot have a libSDL2 loaded beside
// its own SDL, both exporting SDL_Init, SDL_PollEvent and SDL_NumJoysticks.
//
//   abpadd [--shm PATH] [--db FILE] [--watch-pid N] [--rate HZ] [--kernel psc|x360] [--probe] [--exit-only]
//          [--verbose]
//
// --probe prints what SDL makes of every pad and exits, which is how to find out on a console whether
// a pad is mapped at all and what the launcher would call it. --exit-only watches the console's Reset
// button for the app and does nothing else (an App with VirtualPad=false - see ResetWatch).
//
// --kernel psc|x360 is the kernel pad (core/kernel_pad.h) for an App no preload reaches: the daemon makes a uinput
// device with that pad's identity before anything else, holds the real pads with EVIOCGRAB, reads them, and puts the
// result on the device - so the App finds exactly one pad per player, the virtual one, with no shim. The device and
// the grabs are file descriptors: they go when the daemon does, a SIGKILL included.

#include "core/kernel_pad.h"
#include "core/mapping.h"
#include "core/profile.h"
#include "core/shared_state.h"
#include "core/shm_block.h"
#include "core/virtual_pad.h"

#define SDL_MAIN_HANDLED // a console program: SDL must not rename our main
#include <SDL2/SDL.h>
#ifdef main
#undef main
#endif

#include <csignal>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#ifndef _WIN32
#include <sys/stat.h>
#include <unistd.h>
#ifdef __linux__
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#endif
#endif

#ifdef __linux__
#include "daemon/kernel_pad_linux.h"
#endif

using namespace std;
using namespace abpad;

namespace {

// Element's order is SDL's order, which is what lets the read below be a plain loop. Keep it that way.
static_assert(static_cast<int>(Element::A) == SDL_CONTROLLER_BUTTON_A, "Element must follow SDL's order");
static_assert(static_cast<int>(Element::DpRight) == SDL_CONTROLLER_BUTTON_DPAD_RIGHT, "same");
// SDL 2.30 grew paddles, misc1 and a touchpad button past DPAD_RIGHT; we forward the fifteen every
// pad has and every layout can place, so this is a floor, not an equality
static_assert(ButtonElementCount <= SDL_CONTROLLER_BUTTON_MAX, "SDL has fewer buttons than we forward");
static_assert(AxisElementCount == SDL_CONTROLLER_AXIS_MAX, "the axes too");

volatile sig_atomic_t g_stop = 0;
bool g_verbose = false;

void say(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
    fflush(stderr);
}

void chatter(const char *format, ...) {
    if (!g_verbose) {
        return;
    }
    va_list args;
    va_start(args, format);
    vfprintf(stderr, format, args);
    va_end(args);
    fputc('\n', stderr);
    fflush(stderr);
}

#ifndef _WIN32
void onSignal(int) {
    g_stop = 1;
}
#endif

//*******************************
// Slot - one player, kept across an unplug so player two stays player two
//*******************************
struct Slot {
    SDL_GameController *controller = nullptr;
    SDL_JoystickID instance = -1;
    string name;
    string guid;
#ifdef __linux__
    // kernel pad: a pad SDL reads through evdev is read here instead (SDL's own handle goes quiet under the grab)...
    unique_ptr<GrabbedPad> grabbed;
    // ...and a pad it reads through hidapi has its event nodes held for the App's sake
    Siblings siblings;
    string heldPath; // the event node the grab is on, for the next pad not to be given the same one
#endif

    bool occupied() const { return controller != nullptr; }
};

#ifdef __linux__
//*******************************
// Kernel - the kernel pad mode (--kernel), one virtual device per player
//*******************************
struct Kernel {
    bool enabled = false;
    VirtualPadKind kind = VirtualPadKind::Psc;
    unique_ptr<UinputPad> pads[MaxPads];
    // SDL lists our virtual pads as joysticks, and the console's own pad has exactly their identity, so a path or an
    // id cannot tell them apart (SDL 2.0.18 has no path call at all). What can: a joystick that was not there when a
    // virtual pad was made, has the virtual pad's ids and turns up while one is still unaccounted for, is that pad.
    vector<SDL_JoystickID> seenBefore;
    vector<SDL_JoystickID> ownIds;
    int pendingOwn = 0;
    bool snapshotTaken = false;

    void markReal(SDL_JoystickID id) {
        if (!has(seenBefore, id)) {
            seenBefore.push_back(id);
        }
    }

    static bool has(const vector<SDL_JoystickID> &list, SDL_JoystickID id) {
        for (SDL_JoystickID one : list) {
            if (one == id) {
                return true;
            }
        }
        return false;
    }

    // true when this SDL device index is one of our virtual pads
    bool isOwn(int deviceIndex) {
        SDL_JoystickID id = SDL_JoystickGetDeviceInstanceID(deviceIndex);
        if (has(ownIds, id)) {
            return true;
        }
        const UinputPlan &plan = uinputPlan(kind);
        if (pendingOwn > 0 && !has(seenBefore, id) && SDL_JoystickGetDeviceVendor(deviceIndex) == plan.vendor &&
            SDL_JoystickGetDeviceProduct(deviceIndex) == plan.product) {
            ownIds.push_back(id);
            --pendingOwn;
            return true;
        }
        markReal(id);
        return false;
    }

    vector<string> ownPaths() const {
        vector<string> paths;
        for (const unique_ptr<UinputPad> &pad : pads) {
            if (pad && !pad->eventPath().empty()) {
                paths.push_back(pad->eventPath());
            }
        }
        return paths;
    }

    bool ensure(int player) {
        if (!enabled) {
            return false;
        }
        if (!pads[player]) {
            string error;
            if (!snapshotTaken) {
                // what is plugged in before our first virtual pad: the real ones, whatever their ids
                for (int i = 0; i < SDL_NumJoysticks(); ++i) {
                    markReal(SDL_JoystickGetDeviceInstanceID(i));
                }
                snapshotTaken = true;
            }
            pads[player] = UinputPad::create(kind, error);
            if (!pads[player]) {
                say("abpadd: kernel pad for player %d: %s", player + 1, error.c_str());
                return false;
            }
            ++pendingOwn;
            say("abpadd: kernel pad for player %d is %s at %s", player + 1, uinputPlan(kind).name.c_str(),
                pads[player]->eventPath().c_str());
        }
        return true;
    }
};

Kernel g_kernel;
#endif

string guidOf(SDL_JoystickGUID guid) {
    char text[64];
    SDL_JoystickGetGUIDString(guid, text, sizeof(text));
    return text;
}

//*******************************
// settle - give a pad that is about to re-enumerate the time to do it
//*******************************
// A multi-mode pad is taken over by SDL's hidapi driver a second or two after it is first opened: it
// disappears and comes back with a different GUID and a different layout. SDL stamps the driver into
// the GUID, so a trailing 'h' (0x68) means hidapi took it. Reporting before that settles describes a
// device that is about to stop existing - which is exactly how the evdev view of a DualShock gets
// mistaken for the real pad, and why the launcher (which runs on) and a tool that exits at once can
// disagree about the same pad.
void settle(int milliseconds) {
    Uint32 until = SDL_GetTicks() + static_cast<Uint32>(milliseconds);
    while (SDL_GetTicks() < until && !g_stop) {
        SDL_Event drain;
        while (SDL_PollEvent(&drain)) {
        }
        SDL_Delay(20);
    }
}

// the driver SDL resolved the pad through, as its GUID records it
const char *driverOf(const string &guid) {
    if (guid.size() < 32) {
        return "?";
    }
    switch (guid[28]) {
    case '6':
        return (guid[29] == '8') ? "hidapi" : "?";
    default:
        break;
    }
    return "evdev";
}

//*******************************
// ensureMapping - a pad nobody has mapped is given a mapping rather than left invisible
//*******************************
// SDL only offers a pad as a GameController when its GUID is in the database, so a pad the database
// does not know would simply never appear. Rather than let that happen, the daemon guesses a mapping
// from the pad's shape and hands it to SDL, which then treats it like any other - one path for
// everything downstream.
bool ensureMapping(int deviceIndex) {
    if (SDL_IsGameController(deviceIndex)) {
        return true;
    }
    SDL_Joystick *joystick = SDL_JoystickOpen(deviceIndex);
    if (!joystick) {
        return false;
    }
    string guid = guidOf(SDL_JoystickGetGUID(joystick));
    const char *name = SDL_JoystickName(joystick);
    PadMapping guess = guessMapping(guid, name ? name : "", SDL_JoystickNumButtons(joystick),
                                    SDL_JoystickNumAxes(joystick), SDL_JoystickNumHats(joystick));
    SDL_JoystickClose(joystick);

    string line = guess.toLine();
    if (SDL_GameControllerAddMapping(line.c_str()) < 0) {
        say("abpadd: SDL would not take the guessed mapping for %s: %s", guid.c_str(), SDL_GetError());
        return false;
    }
    say("abpadd: %s (%s) is in no database - guessed %s", guess.name.c_str(), guid.c_str(), line.c_str());
    return SDL_IsGameController(deviceIndex) == SDL_TRUE;
}

#ifdef __linux__
//*******************************
// takeOverForKernel - the real pad is held, the App sees the virtual one
//*******************************
void takeOverForKernel(Slot *slots, int player, SDL_Joystick *joystick) {
    Slot &slot = slots[player];
    if (!g_kernel.ensure(player)) {
        return; // no uinput: the App is left with the real pad rather than none, and the log says so
    }
    if (string(driverOf(slot.guid)) == "evdev") {
        // the node SDL reads, found by the pad's ids (SDL 2.0.18 cannot say which it is); two pads with the same ids
        // are told apart by taking the nodes in order - they are the same pad to everything after this
        vector<string> taken = g_kernel.ownPaths();
        for (int i = 0; i < MaxPads; ++i) {
            if (!slots[i].heldPath.empty()) {
                taken.push_back(slots[i].heldPath);
            }
        }
        string path = findEventNode(SDL_JoystickGetVendor(joystick), SDL_JoystickGetProduct(joystick),
                                    SDL_JoystickGetProductVersion(joystick), taken);
        char *text = SDL_GameControllerMapping(slot.controller);
        PadMapping mapping;
        if (path.empty()) {
            say("abpadd: player %d: no event node found for it - the App will see this pad as well as the virtual one",
                player + 1);
        } else if (text != nullptr && PadMapping::parseLine(text, mapping)) {
            string error;
            slot.grabbed = GrabbedPad::open(path, mapping, error);
            if (slot.grabbed) {
                slot.heldPath = path;
                say("abpadd: player %d is held at %s", player + 1, path.c_str());
            } else {
                say("abpadd: %s - the App will see this pad as well as the virtual one", error.c_str());
            }
        }
        if (text != nullptr) {
            SDL_free(text);
        }
    } else {
        // hidapi: SDL reads the hid report, the event nodes are only what an App could open
        slot.siblings.grab(SDL_JoystickGetVendor(joystick), SDL_JoystickGetProduct(joystick), g_kernel.ownPaths());
        say("abpadd: player %d: %d event node(s) held", player + 1, static_cast<int>(slot.siblings.count()));
    }
}
#endif

//*******************************
// addPad / removePad
//*******************************
void addPad(Slot *slots, int deviceIndex) {
#ifdef __linux__
    if (g_kernel.enabled && g_kernel.isOwn(deviceIndex)) {
        // our own virtual pads are joysticks to SDL too: never one of the players
        chatter("abpadd: device %d is our own kernel pad", deviceIndex);
        return;
    }
#endif
    if (!ensureMapping(deviceIndex)) {
        chatter("abpadd: device %d is not a pad we can use", deviceIndex);
        return;
    }
    SDL_GameController *controller = SDL_GameControllerOpen(deviceIndex);
    if (!controller) {
        say("abpadd: cannot open device %d: %s", deviceIndex, SDL_GetError());
        return;
    }

    SDL_Joystick *joystick = SDL_GameControllerGetJoystick(controller);
    SDL_JoystickID instance = SDL_JoystickInstanceID(joystick);
    for (int i = 0; i < MaxPads; ++i) {
        if (slots[i].occupied() && slots[i].instance == instance) {
            SDL_GameControllerClose(controller); // already ours, SDL told us twice
            return;
        }
    }

    // the lowest free slot: player one stays player one while player two comes and goes
    for (int i = 0; i < MaxPads; ++i) {
        if (!slots[i].occupied()) {
            const char *name = SDL_GameControllerName(controller);
            slots[i].controller = controller;
            slots[i].instance = instance;
            slots[i].name = name ? name : "pad";
            slots[i].guid = guidOf(SDL_JoystickGetGUID(joystick));
            say("abpadd: player %d is %s (%s, through %s)", i + 1, slots[i].name.c_str(), slots[i].guid.c_str(),
                driverOf(slots[i].guid));
#ifdef __linux__
            takeOverForKernel(slots, i, joystick);
#endif
            return;
        }
    }
    SDL_GameControllerClose(controller);
    say("abpadd: more than %d pads are plugged in; the rest are ignored", MaxPads);
}

void removePad(Slot *slots, SDL_JoystickID instance) {
    for (int i = 0; i < MaxPads; ++i) {
        if (slots[i].occupied() && slots[i].instance == instance) {
            say("abpadd: player %d (%s) was unplugged", i + 1, slots[i].name.c_str());
#ifdef __linux__
            slots[i].grabbed.reset();
            slots[i].heldPath.clear();
            slots[i].siblings.release();
            if (i > 0) {
                g_kernel.pads[i].reset(); // player one keeps its device; a second goes with its pad
            }
#endif
            SDL_GameControllerClose(slots[i].controller);
            slots[i].controller = nullptr;
            slots[i].instance = -1;
            slots[i].name.clear();
            slots[i].guid.clear();
            return;
        }
    }
}

//*******************************
// writeMappings - the mapping the daemon actually resolved, for apps the shim cannot reach
//*******************************
// An SDL2 app that uses the GameController API never calls the joystick entry points, so the shim is
// invisible to it - but it still wants our pad. SDL_GAMECONTROLLERCONFIG_FILE is how to give it one,
// and what goes in that file matters: pointing an app at gamecontrollerdb.txt would be wrong, because
// a file entry *overrides* SDL's built-in table, and for a pad SDL already knows (a DualShock through
// hidapi) the built-in entry is the right one and ours may be a stale line for another of its modes.
//
// So we write back what SDL resolved for the pads that are actually here - built-in, from our
// database, or the guess made for a pad nobody has mapped - and an app loading that file ends up with
// exactly the mapping the launcher is using. Rewritten whenever the pads change.
void writeMappings(const string &path, Slot *slots) {
    if (path.empty()) {
        return;
    }
    string text = "# written by abpadd: the mapping in use for each pad that is plugged in now.\n"
                  "# Point an app at this with SDL_GAMECONTROLLERCONFIG_FILE.\n";
    int written = 0;
    for (int i = 0; i < MaxPads; ++i) {
        if (!slots[i].occupied()) {
            continue;
        }
        char *mapping = SDL_GameControllerMapping(slots[i].controller);
        if (!mapping) {
            continue;
        }
        text += mapping;
        text += "\n";
        SDL_free(mapping);
        ++written;
    }

    string temporary = path + ".tmp";
    FILE *file = fopen(temporary.c_str(), "w");
    if (!file) {
        say("abpadd: cannot write %s", temporary.c_str());
        return;
    }
    fwrite(text.data(), 1, text.size(), file);
    fclose(file);
#ifndef _WIN32
    chmod(temporary.c_str(), 0644); // an App may not be us
#endif
    if (rename(temporary.c_str(), path.c_str()) != 0) {
        say("abpadd: cannot replace %s", path.c_str());
        return;
    }
    chatter("abpadd: %d mapping(s) written to %s", written, path.c_str());
}

//*******************************
// QuitWatch - the way out that does not depend on the app agreeing
//*******************************
// The shim asks the app to quit when the hotkey is held, which is enough for an app that listens.
// Some do not: they ignore SDL_QUIT, or they are wedged, or they have taken the screen and the
// keyboard and there is nothing else to press. The daemon is the one part of this that is outside
// the app and knows its pid, so it is the part that can end it - politely first, then not.
class QuitWatch {
public:
    QuitWatch(const vector<Element> &hotkey, long pid) : hotkey_(hotkey), pid_(pid) {}

    bool enabled() const { return pid_ > 0 && !hotkey_.empty(); }

    // called once per cycle with every pad's state; true when the app is gone and we should stop
    void check(const ControllerState *pads, const bool *connected, int padCount, int rate) {
        if (!enabled()) {
            return;
        }
        bool held = false;
        for (int i = 0; i < padCount; ++i) {
            held = held || (connected[i] && hotkeyHeld(hotkey_, pads[i]));
        }
        if (!held) {
            cycles_ = 0;
            return;
        }

        ++cycles_;
        // the shim has had its go by now (it asks after about a second and a half); these are the
        // two steps after that, so a single unbroken hold goes ask -> terminate -> kill
        if (cycles_ == rate * 3) {
            say("abpadd: the hotkey has been held - stopping the app (pid %ld)", pid_);
#ifndef _WIN32
            kill(static_cast<pid_t>(pid_), SIGTERM);
#endif
        } else if (cycles_ == rate * 5) {
            say("abpadd: it did not stop - killing it");
#ifndef _WIN32
            kill(static_cast<pid_t>(pid_), SIGKILL);
#endif
        }
    }

private:
    vector<Element> hotkey_;
    long pid_;
    int cycles_ = 0;
};

//*******************************
// ResetWatch - the console's Reset button ends the app
//*******************************
// The PlayStation Classic's Reset button is an input device's KEY_PLAYPAUSE - what SDL 2.0.14 calls
// SDL_SCANCODE_AUDIOPLAY, which is how pcsx-ab and pcsx-abnxt see it and leave the game. A third-party
// app sees it as a key it does not know, and ignores it; we have no window and so no key events, so we
// read the devices that can send it ourselves (we run as root, for hidraw, which covers /dev/input too).
// A press asks the app to quit through the shim (quitRequests), then ends it by its pid: SIGTERM after
// a second and a half, SIGKILL after three. In --exit-only mode (an App with VirtualPad=false - no shim
// to ask) the SIGTERM comes at once. Nothing on a Pi or a PC has the key, so there it watches nothing.
class ResetWatch {
public:
    ResetWatch(long pid, SharedState *shared) : pid_(pid), shared_(shared) { open(); }
    ~ResetWatch() {
#ifdef __linux__
        for (int fd : fds_) {
            ::close(fd);
        }
#endif
    }
    ResetWatch(const ResetWatch &) = delete;
    ResetWatch &operator=(const ResetWatch &) = delete;

    bool enabled() const { return pid_ > 0 && !fds_.empty(); }

    // called once per cycle
    void check(int rate) {
        if (!enabled()) {
            return;
        }
        if (cycles_ < 0 && pressed()) {
            say("abpadd: Reset was pressed - stopping the app (pid %ld)", pid_);
            cycles_ = 0;
            if (shared_ != nullptr) {
                requestQuit(*shared_);
            } else {
                stop(false);
            }
        }
        if (cycles_ < 0) {
            return;
        }
        ++cycles_;
        if (shared_ != nullptr && cycles_ == rate * 3 / 2) {
            stop(false);
        } else if (cycles_ == rate * 3) {
            say("abpadd: it did not stop - killing it");
            stop(true);
        }
    }

private:
    void open() {
#ifdef __linux__
        if (pid_ <= 0) {
            return;
        }
        for (int i = 0; i < 32; ++i) {
            string path = "/dev/input/event" + to_string(i);
            int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (fd < 0) {
                continue;
            }
            unsigned char keys[KEY_MAX / 8 + 1] = {};
            if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) >= 0 &&
                (keys[KEY_PLAYPAUSE / 8] & (1 << (KEY_PLAYPAUSE % 8))) != 0) {
                char name[128] = {};
                ioctl(fd, EVIOCGNAME(sizeof(name) - 1), name);
                say("abpadd: watching %s (%s) for Reset", path.c_str(), name);
                fds_.push_back(fd);
            } else {
                ::close(fd);
            }
        }
#endif
    }

    bool pressed() {
        bool any = false;
#ifdef __linux__
        for (int fd : fds_) {
            input_event events[16];
            ssize_t bytes;
            while ((bytes = read(fd, events, sizeof(events))) > 0) {
                for (size_t i = 0; i < static_cast<size_t>(bytes) / sizeof(input_event); ++i) {
                    any = any || (events[i].type == EV_KEY && events[i].code == KEY_PLAYPAUSE && events[i].value == 1);
                }
            }
        }
#endif
        return any;
    }

    // SIGTERM, or SIGKILL when hard
    void stop(bool hard) const {
#ifndef _WIN32
        kill(static_cast<pid_t>(pid_), hard ? SIGKILL : SIGTERM);
#else
        (void)hard;
#endif
    }

    long pid_;
    SharedState *shared_;
    vector<int> fds_;
    int cycles_ = -1; // -1: not pressed yet; then counting towards SIGTERM / SIGKILL
};

//*******************************
// exitOnly - Reset and nothing else
//*******************************
// For an App that reads the pads itself (VirtualPad=false): no SDL, no shared memory, only the way out.
int exitOnly(long watchPid) {
    ResetWatch reset(watchPid, nullptr);
    if (!reset.enabled()) {
        chatter("abpadd: no Reset button to watch");
        return 0;
    }
    constexpr int rate = 50;
    while (!g_stop) {
        reset.check(rate);
#ifndef _WIN32
        if (kill(static_cast<pid_t>(watchPid), 0) != 0) {
            chatter("abpadd: the app (pid %ld) is gone", watchPid);
            break;
        }
        usleep(1000000 / rate);
#endif
    }
    return 0;
}

//*******************************
// readPad
//*******************************
ControllerState readPad(SDL_GameController *controller) {
    ControllerState state;
    for (int button = 0; button < ButtonElementCount; ++button) {
        if (SDL_GameControllerGetButton(controller, static_cast<SDL_GameControllerButton>(button))) {
            state.set(static_cast<Element>(button), true);
        }
    }
    for (int axis = 0; axis < AxisElementCount; ++axis) {
        Sint16 value = SDL_GameControllerGetAxis(controller, static_cast<SDL_GameControllerAxis>(axis));
        state.set(static_cast<Element>(FirstAxisElement + axis), value);
    }
    return state;
}

//*******************************
// loadMappings - the launcher's own gamecontrollerdb.txt, the kernel's first where there is one
//*******************************
void loadMappings(const string &paths) {
    size_t start = 0;
    while (start <= paths.size()) {
        size_t separator = paths.find(':', start);
        string path = (separator == string::npos) ? paths.substr(start) : paths.substr(start, separator - start);
        if (!path.empty()) {
            int loaded = SDL_GameControllerAddMappingsFromFile(path.c_str());
            if (loaded >= 0) {
                say("abpadd: %d mappings from %s", loaded, path.c_str());
            } else {
                chatter("abpadd: no mappings at %s", path.c_str());
            }
        }
        if (separator == string::npos) {
            break;
        }
        start = separator + 1;
    }
}

//*******************************
// warnIfNotPrivileged - the daemon has to see the pad the way the launcher sees it
//*******************************
// SDL reaches a modern pad through its hidapi driver, which needs /dev/hidraw*, and those are
// root-only. Without them SDL silently falls back to evdev - where the same physical pad has a
// different GUID, a different button layout and quite possibly a stale gamecontrollerdb line written
// for some other mode of it. The launcher runs as root, so an unprivileged daemon would resolve the
// pad differently from the launcher, which is the one thing this program exists not to do.
void warnIfNotPrivileged() {
#ifndef _WIN32
    if (geteuid() == 0) {
        return;
    }
    if (access("/dev/hidraw0", R_OK) == 0) {
        return; // udev has opened them up; hidapi will work anyway
    }
    struct stat facts;
    if (stat("/dev/hidraw0", &facts) != 0) {
        return; // no hidraw devices at all - nothing to miss
    }
    say("abpadd: WARNING - not running as root and /dev/hidraw* is not readable, so SDL will fall");
    say("        back to evdev. A pad may then resolve differently here than in the launcher.");
#endif
}

//*******************************
// probe - what SDL makes of every pad, and out
//*******************************
int probe() {
    warnIfNotPrivileged();
    settle(3000);
    say("abpadd: SDL sees %d joystick(s)", SDL_NumJoysticks());
    for (int i = 0; i < SDL_NumJoysticks(); ++i) {
        SDL_Joystick *joystick = SDL_JoystickOpen(i);
        if (!joystick) {
            say("  [%d] cannot be opened: %s", i, SDL_GetError());
            continue;
        }
        string guid = guidOf(SDL_JoystickGetGUID(joystick));
        const char *name = SDL_JoystickName(joystick);
        say("  [%d] %s", i, name ? name : "(no name)");
        say("       guid      %s  (through %s)", guid.c_str(), driverOf(guid));
        say("       shape     %d buttons, %d axes, %d hats", SDL_JoystickNumButtons(joystick),
            SDL_JoystickNumAxes(joystick), SDL_JoystickNumHats(joystick));
        char *mapping = SDL_GameControllerMappingForGUID(SDL_JoystickGetGUID(joystick));
        if (mapping) {
            say("       mapping   %s", mapping);
            SDL_free(mapping);
        } else {
            PadMapping guess = guessMapping(guid, name ? name : "", SDL_JoystickNumButtons(joystick),
                                            SDL_JoystickNumAxes(joystick), SDL_JoystickNumHats(joystick));
            say("       mapping   NONE in the database - would guess %s", guess.toLine().c_str());
        }
        SDL_JoystickClose(joystick);
    }
    return 0;
}

} // namespace

//*******************************
// watch - print what the running daemon is publishing, and keep printing as it changes
//*******************************
// The other half of --probe: --probe says what SDL makes of the pads, --watch says what actually
// reaches the shim. Between them they answer "is it the daemon or the app?" without a debugger.
int watch(const string &shmPath) {
    ShmBlock block;
    if (!block.openReadOnly(shmPath, sizeof(SharedState))) {
        say("abpadd: %s (%s)", block.error().c_str(), shmPath.c_str());
        return 1;
    }
    const SharedState *shared = static_cast<const SharedState *>(block.data());
    say("abpadd: watching %s", shmPath.c_str());

    Snapshot last;
    bool first = true;
    uint32_t quiet = 0;
    while (!g_stop) {
        Snapshot now;
        if (!readSnapshot(*shared, now)) {
            say("abpadd: the block is not readable");
            return 1;
        }
        if (now.heartbeat == last.heartbeat && !first) {
            if (++quiet == 100) {
                say("(the daemon has stopped publishing)");
            }
        } else {
            quiet = 0;
        }

        for (int pad = 0; pad < now.padCount; ++pad) {
            if (!first && now.pads[pad] == last.pads[pad] && now.connected[pad] == last.connected[pad]) {
                continue;
            }
            string line = "player " + to_string(pad + 1) + (now.connected[pad] ? ": " : ": (unplugged) ");
            for (int i = 0; i < ButtonElementCount; ++i) {
                if (now.pads[pad].button(static_cast<Element>(i))) {
                    line += elementName(static_cast<Element>(i));
                    line += " ";
                }
            }
            for (int i = 0; i < AxisElementCount; ++i) {
                Element element = static_cast<Element>(FirstAxisElement + i);
                int16_t value = now.pads[pad].axis(element);
                if (value > 6000 || value < -6000) {
                    line += string(elementName(element)) + "=" + to_string(value) + " ";
                }
            }
            say("%s", line.c_str());
        }
        last = now;
        first = false;
        SDL_Delay(16);
    }
    return 0;
}

//*******************************
// main
//*******************************
int main(int argc, char *argv[]) {
    string shmPath = defaultShmPath();
    const char *dbFromEnvironment = getenv("AB_PAD_DB");
    string dbPaths = dbFromEnvironment ? dbFromEnvironment : "";
    string mappingsPath;
    string quitHotkey = "start+select";
    int rate = 250; // Hz - far more than any game reads its pad at, and a rounding error of a core
    long watchPid = 0;
    bool probeOnly = false;
    bool watchOnly = false;
    bool exitOnlyMode = false;
    string kernelMode; // "psc" / "x360": the kernel pad

    for (int i = 1; i < argc; ++i) {
        string argument = argv[i];
        bool hasNext = i + 1 < argc;
        if (argument == "--shm" && hasNext) {
            shmPath = argv[++i];
        } else if (argument == "--db" && hasNext) {
            dbPaths = dbPaths.empty() ? argv[++i] : dbPaths + ":" + argv[++i];
        } else if (argument == "--quit-hotkey" && hasNext) {
            quitHotkey = argv[++i];
        } else if (argument == "--mappings" && hasNext) {
            mappingsPath = argv[++i];
        } else if (argument == "--rate" && hasNext) {
            rate = atoi(argv[++i]);
        } else if (argument == "--watch-pid" && hasNext) {
            watchPid = atol(argv[++i]);
        } else if (argument == "--kernel" && hasNext) {
            kernelMode = argv[++i];
        } else if (argument == "--probe") {
            probeOnly = true;
        } else if (argument == "--watch") {
            watchOnly = true;
        } else if (argument == "--exit-only") {
            exitOnlyMode = true;
        } else if (argument == "--verbose") {
            g_verbose = true;
        } else {
            say("usage: abpadd [--shm PATH] [--db FILE] [--mappings FILE] [--watch-pid N]");
            say("              [--quit-hotkey a+b] [--rate HZ] [--kernel psc|x360] [--probe] [--watch]");
            say("              [--exit-only] [--verbose]");
            return argument == "--help" ? 0 : 2;
        }
    }
    if (mappingsPath.empty()) {
        const char *fromEnvironment = getenv("AB_PAD_MAPPINGS");
        mappingsPath = (fromEnvironment && *fromEnvironment) ? fromEnvironment : shmPath + ".mappings";
    }
    if (rate < 20) {
        rate = 20;
    }
    if (rate > 1000) {
        rate = 1000;
    }

    // no window, no focus, so nothing may gate the pad on having one; and we handle our own signals
    SDL_SetMainReady();
    if (watchOnly) {
#ifndef _WIN32
        signal(SIGINT, onSignal);
        signal(SIGTERM, onSignal);
#endif
        return watch(shmPath);
    }
    if (exitOnlyMode) {
        // an App that reads the pads itself: only the console's Reset button, no SDL, no shared memory
#ifndef _WIN32
        signal(SIGINT, onSignal);
        signal(SIGTERM, onSignal);
        signal(SIGHUP, onSignal);
#ifdef __linux__
        prctl(PR_SET_PDEATHSIG, SIGTERM);
#endif
#endif
        return exitOnly(watchPid);
    }

    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    SDL_SetHint(SDL_HINT_NO_SIGNAL_HANDLERS, "1");
    if (SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0) {
        say("abpadd: SDL_Init failed: %s", SDL_GetError());
        return 1;
    }
    if (!dbPaths.empty()) {
        loadMappings(dbPaths);
    }

    if (probeOnly) {
        int result = probe();
        SDL_Quit();
        return result;
    }

#ifndef _WIN32
    signal(SIGINT, onSignal);
    signal(SIGTERM, onSignal);
    signal(SIGHUP, onSignal);
#ifdef __linux__
    // if whoever started us dies without tidying up, go with them rather than linger holding the lock
    prctl(PR_SET_PDEATHSIG, SIGTERM);
#endif
#endif

    warnIfNotPrivileged();

#ifdef __linux__
    // the kernel pad exists before the App can look for one: before the state block that the App's start waits for
    if (!kernelMode.empty()) {
        g_kernel.enabled = true;
        g_kernel.kind = virtualPadKindFromName(kernelMode);
        if (!g_kernel.ensure(0)) {
            say("abpadd: no kernel pad - the App gets the real pads as they are");
            g_kernel.enabled = false;
        }
    }
#else
    (void)kernelMode;
#endif

    Slot slots[MaxPads];
    bool kernelScanned = false;
#ifdef __linux__
    if (g_kernel.enabled) {
        // the real pads are held before the state block exists: the App's start waits for that block, so it never
        // gets a moment with both the real pad and the virtual one in view
        settle(3000);
        kernelScanned = true;
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            addPad(slots, i);
        }
    }
#endif

    ShmBlock block;
    if (!block.create(shmPath, sizeof(SharedState))) {
        say("abpadd: %s", block.error().c_str());
        SDL_Quit();
        return 1;
    }
    SharedState *shared = static_cast<SharedState *>(block.data());
    initSharedState(*shared);
#ifndef _WIN32
    shared->daemonPid = static_cast<uint32_t>(getpid());
#endif
    say("abpadd: publishing to %s", shmPath.c_str());

    QuitWatch quitWatch(parseHotkey(quitHotkey), watchPid);
    if (quitWatch.enabled()) {
        say("abpadd: holding %s will stop the app (pid %ld)", quitHotkey.c_str(), watchPid);
    }
    ResetWatch resetWatch(watchPid, shared);

    // whatever is already plugged in when we start, once it has stopped changing shape under us;
    // everything after that arrives as an event
    if (!kernelScanned) {
        settle(3000);
        for (int i = 0; i < SDL_NumJoysticks(); ++i) {
            addPad(slots, i);
        }
    }
    for (int i = 0; i < MaxPads; ++i) {
        if (slots[i].occupied()) {
            publishPadIdentity(*shared, i, slots[i].name.c_str(), slots[i].guid.c_str());
        }
    }
    writeMappings(mappingsPath, slots);
    say("abpadd: mappings for the pads in use are in %s", mappingsPath.c_str());

    const Uint32 interval = static_cast<Uint32>(1000 / rate);
    while (!g_stop) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_JOYDEVICEADDED:
                // every joystick arrives as this, mapped or not, which is where an unknown pad is
                // given a mapping; SDL_CONTROLLERDEVICEADDED only ever speaks of mapped ones
                addPad(slots, event.jdevice.which);
                for (int i = 0; i < MaxPads; ++i) {
                    if (slots[i].occupied()) {
                        publishPadIdentity(*shared, i, slots[i].name.c_str(), slots[i].guid.c_str());
                    }
                }
                writeMappings(mappingsPath, slots);
                break;
            case SDL_JOYDEVICEREMOVED:
                removePad(slots, event.jdevice.which);
                writeMappings(mappingsPath, slots);
                break;
            case SDL_QUIT:
                g_stop = 1;
                break;
            default:
                break;
            }
        }

        ControllerState pads[MaxPads];
        bool connected[MaxPads] = {};
        int highest = 0;
        for (int i = 0; i < MaxPads; ++i) {
            if (slots[i].occupied()) {
#ifdef __linux__
                if (slots[i].grabbed && !slots[i].grabbed->poll()) {
                    slots[i].grabbed.reset(); // gone; SDL says so in a moment
                }
                pads[i] = slots[i].grabbed ? slots[i].grabbed->state() : readPad(slots[i].controller);
#else
                pads[i] = readPad(slots[i].controller);
#endif
                connected[i] = true;
                highest = i + 1;
            }
        }
#ifdef __linux__
        if (g_kernel.enabled) {
            const VirtualLayout &layout = virtualLayout(g_kernel.kind);
            for (int i = 0; i < MaxPads; ++i) {
                if (g_kernel.pads[i]) {
                    // a slot with no pad is a pad at rest
                    g_kernel.pads[i]->update(buildRawState(layout, controllerView(g_kernel.kind, pads[i])));
                }
            }
        }
#endif
        publishPads(*shared, pads, connected, highest);
        quitWatch.check(pads, connected, highest, rate);
        resetWatch.check(rate);

#ifndef _WIN32
        // the App we were started for has gone: nothing left to serve
        if (watchPid > 0 && kill(static_cast<pid_t>(watchPid), 0) != 0) {
            chatter("abpadd: the app (pid %ld) is gone", watchPid);
            break;
        }
#endif
        SDL_Delay(interval);
    }

    for (Slot &slot : slots) {
#ifdef __linux__
        slot.grabbed.reset(); // the grab goes first, then the virtual pad
        slot.siblings.release();
#endif
        if (slot.occupied()) {
            SDL_GameControllerClose(slot.controller);
        }
    }
#ifdef __linux__
    for (unique_ptr<UinputPad> &pad : g_kernel.pads) {
        pad.reset();
    }
#endif
    block.close();
    SDL_Quit();
    say("abpadd: stopped");
    return 0;
}
