#ifndef ABPAD_SHARED_STATE_H
#define ABPAD_SHARED_STATE_H

// What the daemon publishes and the shim reads: every pad's controller state, in shared memory.
//
// The daemon (abpadd) is the one program that reads the pads, through SDL2's GameController API with
// our gamecontrollerdb.txt, so a pad resolves in an App exactly as it resolves in the launcher. It
// lives in its own process because an SDL 1.2 app cannot have a libSDL2 loaded beside its own SDL -
// both export SDL_Init, SDL_PollEvent, SDL_NumJoysticks and the rest, and the dynamic linker would
// bind them to whichever came first.
//
// The handover is a seqlock rather than a mutex: the reader is inside somebody else's game loop and
// must never block or be left holding a lock if the daemon is killed mid-write. The writer makes the
// sequence odd, writes, makes it even; a reader that sees an odd sequence, or a different one
// afterwards, simply reads again.

#include "core/mapping.h"

#include <atomic>
#include <cstdint>

namespace abpad {

constexpr uint32_t SharedMagic = 0x41425044u; // "ABPD"
constexpr uint32_t SharedVersion = 1;
constexpr int MaxPads = 4;
constexpr int SharedNameSize = 64;
constexpr int SharedGuidSize = 33;

//*******************************
// SharedPad / SharedState - the shared memory block, byte for byte
//*******************************
struct SharedPad {
    uint32_t connected;
    uint32_t buttons; // one bit per button element, in Element order
    int16_t axes[AxisElementCount];
    char name[SharedNameSize]; // the *physical* pad's name, for the log - not what the app is told
    char guid[SharedGuidSize];
    char padding[3];
};

struct SharedState {
    uint32_t magic;
    uint32_t version;
    std::atomic<uint32_t> sequence; // even: settled. odd: being written
    uint32_t padCount;
    uint32_t daemonPid;
    uint32_t heartbeat; // bumped every cycle, so a reader can tell a stopped daemon from a still one
    // bumped each time the daemon wants the app gone (the console's Reset button): the shim sends the
    // app a quit event when it sees a new count. Was reserved[0], so an older shim simply ignores it.
    uint32_t quitRequests;
    uint32_t reserved[1];
    SharedPad pads[MaxPads];
};

static_assert(ATOMIC_INT_LOCK_FREE == 2, "the seqlock has to be lock-free to be shared between processes");

//*******************************
// Snapshot - what a reader gets, all pads at one instant
//*******************************
struct Snapshot {
    int padCount = 0;
    bool connected[MaxPads] = {};
    ControllerState pads[MaxPads];
    uint32_t heartbeat = 0;
    uint32_t quitRequests = 0;
};

// the daemon's side
void initSharedState(SharedState &state);
void publishPads(SharedState &state, const ControllerState *pads, const bool *connected, int padCount);
// the name and guid are written once when a pad arrives, not every cycle
void publishPadIdentity(SharedState &state, int index, const char *name, const char *guid);
// asks the app to quit - the shim turns a new quitRequests count into a quit event
void requestQuit(SharedState &state);

// the shim's side: false when the block is not ours, or never settled (the daemon died mid-write)
bool readSnapshot(const SharedState &state, Snapshot &out);

} // namespace abpad

#endif
