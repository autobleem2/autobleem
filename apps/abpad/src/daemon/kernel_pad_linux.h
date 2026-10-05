#ifndef ABPAD_KERNEL_PAD_LINUX_H
#define ABPAD_KERNEL_PAD_LINUX_H

// The kernel side of the kernel pad (core/kernel_pad.h has the logic): the uinput device an App is shown, and the
// real pads held with EVIOCGRAB so the App sees only that one. Linux only - abpadd compiles it on __linux__.
//
// Everything here is a file descriptor, and a file descriptor is the clean-up: the kernel removes a uinput device and
// drops a grab when the last descriptor to it closes, whatever the way the process ends (a normal exit, a signal, a
// SIGKILL). So there is no state file to go stale and nothing a crash can leave behind, which is why there is no sweep
// at the next launch - all that is needed is that no descriptor leaks into a child (every one is O_CLOEXEC).

#include "core/kernel_pad.h"

#include <memory>
#include <string>
#include <vector>

namespace abpad {

//*******************************
// UinputPad - the virtual pad
//*******************************
class UinputPad {
public:
    // nullptr (and the reason in error) when /dev/uinput is not there or not ours to use
    static std::unique_ptr<UinputPad> create(VirtualPadKind kind, std::string &error);
    ~UinputPad();
    UinputPad(const UinputPad &) = delete;
    UinputPad &operator=(const UinputPad &) = delete;

    // put the pad's state on the device: only what changed since the last call
    void update(const RawPadState &raw);

    VirtualPadKind kind() const { return kind_; }
    // /dev/input/eventN of the device ("" until the kernel has made it), so the daemon knows its own pad
    const std::string &eventPath() const { return eventPath_; }

private:
    UinputPad(VirtualPadKind kind, int fd) : kind_(kind), fd_(fd) {}
    void send(int type, int code, int value) const;

    VirtualPadKind kind_;
    int fd_;
    std::string eventPath_;
    EvdevFrame last_;
    bool sentOnce_ = false;
};

//*******************************
// GrabbedPad - a real pad read and held by us alone
//*******************************
// For a pad SDL reads through evdev: its events go only to us from the moment of the grab, so SDL's own handle on
// it goes quiet and the pad is read here instead, numbered as SDL numbers it, through the mapping SDL resolved.
class GrabbedPad {
public:
    static std::unique_ptr<GrabbedPad> open(const std::string &eventPath, const PadMapping &mapping,
                                            std::string &error);
    ~GrabbedPad();
    GrabbedPad(const GrabbedPad &) = delete;
    GrabbedPad &operator=(const GrabbedPad &) = delete;

    // read whatever has arrived; false once the pad is gone
    bool poll();
    ControllerState state() const;

private:
    GrabbedPad(int fd, EvdevPadState shape, const PadMapping &mapping)
        : fd_(fd), shape_(std::move(shape)), mapping_(mapping) {}

    int fd_;
    EvdevPadState shape_;
    PadMapping mapping_;
    std::vector<int> keyCodes_;
    std::vector<EvdevAbs> absCodes_;
};

//*******************************
// findEventNode - the event node of a real pad, by its ids, for a node not in `exclude`
//*******************************
// SDL 2.0.18 cannot say which /dev/input/eventN a joystick is (the call came in 2.24), so the node is found by the
// vendor, product and version SDL reports, and it has to look like a pad (joystick buttons). "" when there is none.
std::string findEventNode(int vendor, int product, int version, const std::vector<std::string> &exclude);

//*******************************
// Siblings - the other nodes of a pad read some other way
//*******************************
// A pad SDL reads through hidapi is also an event node for any App that opens one. Those are grabbed too (nothing
// reads them here), found by the pad's vendor and product ids; our own devices and anything with a Reset key are
// never taken.
class Siblings {
public:
    ~Siblings();
    void grab(int vendor, int product, const std::vector<std::string> &ownPaths);
    void release();
    size_t count() const { return fds_.size(); }
    const std::vector<std::string> &paths() const { return paths_; }

private:
    std::vector<int> fds_;
    std::vector<std::string> paths_;
};

//*******************************
// PointerHold - a pad's touchpad and motion sensors, held in every App mode (core: padPointerNodes)
//*******************************
// inputNodeFacts: every /dev/input/eventN with what padPointerNodes needs to know of it. PointerHold grabs the nodes
// it is given (not our own, not one held already), so the compositor gets no pointer from a pad's touchpad, and
// pointerNodesToHide lists, for the App's start, each one's own nodes (the event node, the mouseN the kernel made for
// it, their udev entries) plus /dev/input/mice - not the pad's buttons, which the shim's App still reads.
std::vector<InputNodeFacts> inputNodeFacts();
class PointerHold {
public:
    ~PointerHold();
    void grab(const std::vector<std::string> &eventPaths, const std::vector<std::string> &ownPaths);
    const std::vector<std::string> &paths() const { return paths_; }

private:
    std::vector<int> fds_;
    std::vector<std::string> paths_;
};
std::vector<std::string> pointerNodesToHide(const std::vector<std::string> &eventPaths);

//*******************************
// Hiding the real pads from the App (the kernel pad mode)
//*******************************
// A grab silences a node but leaves it in view: the App still enumerates it - as joystick 0 ahead of the virtual pad,
// and with its siblings (a DualSense's motion sensors are a 6-axis joystick of their own). The original product never
// had that: its one pad was the console's. So the App is started in a mount namespace of its own where every node of
// the held pads is /dev/null (pad-mapping.md 0.2).
//
// hiddenNodes: for each held event node, every node of the same physical device - its event, js and mouse nodes, its
// hidraw nodes - and each one's udev database entry (/run/udev/data/c<major>:<minor>, where libudev reads
// ID_INPUT_JOYSTICK from). A device made through uinput (no physical parent) counts as its own; `own` (our virtual
// pads) never.
std::vector<std::string> hiddenNodes(const std::vector<std::string> &heldEventPaths,
                                     const std::vector<std::string> &own);
// runHidden: abpadd --hide-run LIST -- PROGRAM ARGS: a private mount namespace, /dev/null bound over every path
// LIST names (one per line; a missing or empty LIST hides nothing), then exec PROGRAM. A step that fails is said on
// stderr and skipped - the program always runs.
int runHidden(const std::string &listFile, char **argv);

} // namespace abpad

#endif
