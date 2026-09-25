#ifndef ABPAD_SHIM_STATE_H
#define ABPAD_SHIM_STATE_H

// The shim's whole mind, kept away from the SDL entry points so those stay a list of one-liners.
//
// It reads the daemon's shared block, turns each player's controller state into the raw pad the app
// is being shown, and works out what events that change is worth. It never blocks, never allocates in
// the steady state and never touches a device: everything hard already happened in abpadd.

#include "core/key_names.h"
#include "core/mapping.h"
#include "core/profile.h"
#include "core/shared_state.h"
#include "core/virtual_pad.h"
#include "core/shm_block.h"

#include <cstdio>
#include <deque>
#include <string>

namespace abpad {

//*******************************
// ShimEvent - something that happened, before it is written in either SDL's words
//*******************************
struct ShimEvent {
    enum class Kind {
        ButtonDown, // the joystick view
        ButtonUp,
        AxisMotion,
        HatMotion,
        ControllerButtonDown, // the game controller view of the same press
        ControllerButtonUp,
        ControllerAxisMotion,
        ControllerAdded,
        KeyDown,
        KeyUp,
        Quit
    };

    Kind kind = Kind::Quit;
    int pad = 0;
    int index = 0; // the button, axis or hat
    int value = 0; // the axis value or the hat mask
    KeyCode key;   // KeyDown/KeyUp
};

//*******************************
// ShimState
//*******************************
class ShimState {
public:
    static ShimState &get(); // made on first use: a constructor would run before we know we are wanted

    bool active() const { return active_; }
    const Profile &profile() const { return profile_; }
    const VirtualLayout &layout() const { return *layout_; }
    // what the app is told it has: fixed for the life of the process, so no app ever has to cope with
    // a joystick arriving or leaving (SDL 1.2 cannot, and plenty of SDL2 ports do it badly)
    int padCount() const { return active_ ? profile_.players : 0; }

    // read the daemon and work out the events; called from every entry point that could observe a
    // change, and cheap enough to be called from all of them
    void update();

    const RawPadState &raw(int pad);
    // the pad as SDL's GameController API describes it - what an app using that API is answered with,
    // straight from the daemon, since that view has no layout to it
    const ControllerState &controller(int pad);
    bool nextEvent(ShimEvent &out);
    bool wantsEvents() const { return eventsEnabled_; }
    void setWantsEvents(bool enabled) { eventsEnabled_ = enabled; }

    // true when this process's SDL is SDL2 - asked of the loaded library, not guessed
    bool isSdl2();
    void log(const char *format, ...);

private:
    ShimState();
    void loadProfile();
    void diff(int pad, const RawPadState &before, const RawPadState &after);
    void diffController(int pad, const ControllerState &before, const ControllerState &after);

    bool active_ = false;
    bool eventsEnabled_ = true;
    int sdl2_ = -1;
    Profile profile_;
    const VirtualLayout *layout_ = nullptr;
    KeyCode keys_[ElementCount];

    ShmBlock block_;
    const SharedState *shared_ = nullptr;
    std::string shmPath_;
    unsigned attemptsLeft_ = 600; // the daemon may still be starting; stop looking after a while

    RawPadState raw_[MaxPads];
    ControllerState controller_[MaxPads];
    bool lastHeld_[MaxPads][ElementCount] = {}; // keyboard mode: what was down last time round
    RawPadState rawBefore_[MaxPads];
    ControllerState controllerBefore_[MaxPads];
    bool announcedPads_ = false;
    // The hotkey has to be *held*, not merely pressed: Start and Select are both live buttons in
    // plenty of games, and a pad quitting the game because two of them were pressed together would
    // be worse than having no way out at all.
    unsigned hotkeyHeldCycles_ = 0;
    bool hotkeyQuitSent_ = false;
    // the daemon's quit requests (the console's Reset button): a count, so the first snapshot sets the
    // baseline and only a later, different count asks the app to quit
    uint32_t quitRequestsSeen_ = 0;
    bool quitBaselineTaken_ = false;
    std::deque<ShimEvent> events_;
    FILE *log_ = nullptr;
};

} // namespace abpad

#endif
