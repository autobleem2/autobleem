#include "core/shared_state.h"

#include <cstring>

using namespace std;

namespace abpad {

namespace {

constexpr int MaxReadAttempts = 8; // a reader that keeps losing gives up for this frame, never blocks

void copyName(char *destination, int size, const char *source) {
    if (!source) {
        destination[0] = '\0';
        return;
    }
    strncpy(destination, source, static_cast<size_t>(size) - 1);
    destination[size - 1] = '\0';
}

} // namespace

//*******************************
// initSharedState
//*******************************
void initSharedState(SharedState &state) {
    // field by field rather than one memset: the sequence is an atomic, and blanking it as bytes is
    // both undefined and the one field that must be written last
    state.padCount = 0;
    state.daemonPid = 0;
    state.heartbeat = 0;
    memset(state.reserved, 0, sizeof(state.reserved));
    memset(state.pads, 0, sizeof(state.pads));
    state.magic = SharedMagic;
    state.version = SharedVersion;
    state.sequence.store(0, memory_order_release);
}

//*******************************
// publishPadIdentity
//*******************************
void publishPadIdentity(SharedState &state, int index, const char *name, const char *guid) {
    if (index < 0 || index >= MaxPads) {
        return;
    }
    uint32_t sequence = state.sequence.load(memory_order_relaxed);
    state.sequence.store(sequence + 1, memory_order_release); // odd: a write is in progress
    copyName(state.pads[index].name, SharedNameSize, name);
    copyName(state.pads[index].guid, SharedGuidSize, guid);
    state.sequence.store(sequence + 2, memory_order_release);
}

//*******************************
// publishPads
//*******************************
void publishPads(SharedState &state, const ControllerState *pads, const bool *connected, int padCount) {
    if (padCount < 0) {
        padCount = 0;
    }
    if (padCount > MaxPads) {
        padCount = MaxPads;
    }

    uint32_t sequence = state.sequence.load(memory_order_relaxed);
    state.sequence.store(sequence + 1, memory_order_release);

    state.padCount = static_cast<uint32_t>(padCount);
    for (int i = 0; i < MaxPads; ++i) {
        SharedPad &shared = state.pads[i];
        bool live = i < padCount && connected[i];
        shared.connected = live ? 1u : 0u;
        shared.buttons = 0;
        if (!live) {
            memset(shared.axes, 0, sizeof(shared.axes));
            continue;
        }
        const ControllerState &pad = pads[i];
        for (int button = 0; button < ButtonElementCount; ++button) {
            if (pad.button(static_cast<Element>(button))) {
                shared.buttons |= (1u << button);
            }
        }
        for (int axis = 0; axis < AxisElementCount; ++axis) {
            shared.axes[axis] = pad.axis(static_cast<Element>(FirstAxisElement + axis));
        }
    }
    ++state.heartbeat;

    state.sequence.store(sequence + 2, memory_order_release); // even again: settled
}

//*******************************
// readSnapshot
//*******************************
bool readSnapshot(const SharedState &state, Snapshot &out) {
    if (state.magic != SharedMagic || state.version != SharedVersion) {
        return false;
    }

    for (int attempt = 0; attempt < MaxReadAttempts; ++attempt) {
        uint32_t before = state.sequence.load(memory_order_acquire);
        if (before & 1u) {
            continue; // the daemon is part way through a write
        }

        Snapshot snapshot;
        uint32_t padCount = state.padCount;
        snapshot.padCount = static_cast<int>(padCount > MaxPads ? MaxPads : padCount);
        snapshot.heartbeat = state.heartbeat;
        for (int i = 0; i < snapshot.padCount; ++i) {
            const SharedPad &shared = state.pads[i];
            snapshot.connected[i] = shared.connected != 0;
            for (int button = 0; button < ButtonElementCount; ++button) {
                snapshot.pads[i].set(static_cast<Element>(button), (shared.buttons & (1u << button)) != 0);
            }
            for (int axis = 0; axis < AxisElementCount; ++axis) {
                snapshot.pads[i].set(static_cast<Element>(FirstAxisElement + axis), shared.axes[axis]);
            }
        }

        if (state.sequence.load(memory_order_acquire) == before) {
            out = snapshot;
            return true;
        }
    }
    return false;
}

} // namespace abpad
