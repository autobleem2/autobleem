#include "daemon/kernel_pad_linux.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

// older kernel headers do not have it; the ioctl itself is in every kernel since 3.15
#ifndef UI_GET_SYSNAME
#define UI_GET_SYSNAME(len) _IOC(_IOC_READ, UINPUT_IOCTL_BASE, 44, len)
#endif

using namespace std;

namespace abpad {

namespace {

bool testBit(const unsigned char *bits, int bit) {
    return (bits[bit / 8] & (1 << (bit % 8))) != 0;
}

string readLine(const string &path) {
    string text;
    FILE *file = fopen(path.c_str(), "r");
    if (file) {
        char buffer[128] = {};
        if (fgets(buffer, sizeof(buffer), file)) {
            text = buffer;
        }
        fclose(file);
    }
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
        text.pop_back();
    }
    return text;
}

// the kernel makes the event node a moment after UI_DEV_CREATE, and udev gives it its properties (ID_INPUT_JOYSTICK
// and the rest) a moment after that. An App that starts straight away and enumerates with libudev must find both, so
// wait for the node, and for udev's database entry when udev runs at all.
vector<string> virtualInputs() {
    vector<string> names;
    DIR *directory = opendir("/sys/devices/virtual/input");
    if (directory) {
        while (dirent *entry = readdir(directory)) {
            if (strncmp(entry->d_name, "input", 5) == 0) {
                names.push_back(entry->d_name);
            }
        }
        closedir(directory);
    }
    return names;
}

// a kernel without UI_GET_SYSNAME (before 3.15): the device is the new input that carries our name
string findNewInput(const vector<string> &before, const string &name) {
    for (int waited = 0; waited < 100; ++waited) {
        for (const string &one : virtualInputs()) {
            bool known = false;
            for (const string &old : before) {
                known = known || old == one;
            }
            if (!known && readLine("/sys/devices/virtual/input/" + one + "/name") == name) {
                return one;
            }
        }
        usleep(10000);
    }
    return string();
}

string waitForNode(const string &sysName) {
    string base = "/sys/devices/virtual/input/" + sysName;
    for (int waited = 0; waited < 200; ++waited) { // 2 s
        DIR *directory = opendir(base.c_str());
        if (directory) {
            string event;
            while (dirent *entry = readdir(directory)) {
                if (strncmp(entry->d_name, "event", 5) == 0) {
                    event = entry->d_name;
                }
            }
            closedir(directory);
            if (!event.empty()) {
                string path = "/dev/input/" + event;
                struct stat facts;
                bool udev = stat("/run/udev", &facts) == 0;
                string devNumber = readLine(base + "/" + event + "/dev"); // "13:64"
                if (!udev || devNumber.empty() || stat(("/run/udev/data/c" + devNumber).c_str(), &facts) == 0) {
                    return path;
                }
                if (waited >= 150) {
                    return path; // udev is there but slow or not interested: do not hold the App up for it
                }
            }
        }
        usleep(10000);
    }
    return string();
}

} // namespace

//*******************************
// UinputPad
//*******************************
unique_ptr<UinputPad> UinputPad::create(VirtualPadKind kind, string &error) {
    int fd = ::open("/dev/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        fd = ::open("/dev/input/uinput", O_WRONLY | O_NONBLOCK | O_CLOEXEC);
    }
    if (fd < 0) {
        error = string("cannot open /dev/uinput: ") + strerror(errno);
        return nullptr;
    }

    const UinputPlan &plan = uinputPlan(kind);
    const vector<string> inputsBefore = virtualInputs();
    bool ok = ioctl(fd, UI_SET_EVBIT, EV_KEY) == 0 && ioctl(fd, UI_SET_EVBIT, EV_ABS) == 0 &&
              ioctl(fd, UI_SET_EVBIT, EV_SYN) == 0;
    for (int code : plan.keys) {
        ok = ok && ioctl(fd, UI_SET_KEYBIT, code) == 0;
    }
    uinput_user_dev device;
    memset(&device, 0, sizeof(device));
    snprintf(device.name, UINPUT_MAX_NAME_SIZE, "%s", plan.name.c_str());
    device.id.bustype = plan.bus;
    device.id.vendor = plan.vendor;
    device.id.product = plan.product;
    device.id.version = plan.version;
    for (const UinputAxis &axis : plan.abs) {
        ok = ok && ioctl(fd, UI_SET_ABSBIT, axis.code) == 0;
        device.absmin[axis.code] = axis.min;
        device.absmax[axis.code] = axis.max;
        device.absfuzz[axis.code] = axis.fuzz;
        device.absflat[axis.code] = axis.flat;
    }
    if (!ok || write(fd, &device, sizeof(device)) != static_cast<ssize_t>(sizeof(device)) ||
        ioctl(fd, UI_DEV_CREATE) != 0) {
        error = string("cannot set up the uinput device: ") + strerror(errno);
        ::close(fd);
        return nullptr;
    }

    unique_ptr<UinputPad> pad(new UinputPad(kind, fd));
    char sysName[64] = {};
    string sysNameText;
    if (ioctl(fd, UI_GET_SYSNAME(sizeof(sysName)), sysName) >= 0) { // it returns the length of the name
        sysNameText = sysName;
    } else {
        sysNameText = findNewInput(inputsBefore, plan.name);
    }
    if (sysNameText.empty()) {
        error = "the kernel did not say which input the device is";
        return nullptr;
    }
    pad->eventPath_ = waitForNode(sysNameText);
    if (pad->eventPath_.empty()) {
        error = "no event node appeared for " + sysNameText;
        return nullptr;
    }
    // the pad at rest: the kernel starts every axis at 0, which is neither the console pad centre nor a trigger at rest
    pad->update(buildRawState(virtualLayout(kind), ControllerState()));
    return pad;
}

UinputPad::~UinputPad() {
    if (fd_ >= 0) {
        ioctl(fd_, UI_DEV_DESTROY);
        ::close(fd_);
    }
}

void UinputPad::send(int type, int code, int value) const {
    input_event event;
    memset(&event, 0, sizeof(event));
    event.type = static_cast<__u16>(type);
    event.code = static_cast<__u16>(code);
    event.value = value;
    ssize_t written = write(fd_, &event, sizeof(event));
    (void)written; // a full queue drops an event; the next change repeats the state
}

void UinputPad::update(const RawPadState &raw) {
    EvdevFrame frame = evdevFrame(kind_, raw);
    bool changed = false;
    for (size_t i = 0; i < frame.keys.size(); ++i) {
        if (!sentOnce_ || frame.keys[i] != last_.keys[i]) {
            send(EV_KEY, frame.keys[i].first, frame.keys[i].second);
            changed = true;
        }
    }
    for (size_t i = 0; i < frame.abs.size(); ++i) {
        if (!sentOnce_ || frame.abs[i] != last_.abs[i]) {
            send(EV_ABS, frame.abs[i].first, frame.abs[i].second);
            changed = true;
        }
    }
    if (changed) {
        send(EV_SYN, SYN_REPORT, 0);
    }
    last_ = frame;
    sentOnce_ = true;
}

//*******************************
// GrabbedPad
//*******************************
namespace {

void readState(int fd, EvdevPadState &shape, const vector<int> &keyCodes, const vector<EvdevAbs> &absCodes) {
    unsigned char keys[KEY_MAX / 8 + 1] = {};
    if (ioctl(fd, EVIOCGKEY(sizeof(keys)), keys) >= 0) {
        for (int code : keyCodes) {
            shape.setKey(code, testBit(keys, code) ? 1 : 0);
        }
    }
    for (const EvdevAbs &abs : absCodes) {
        input_absinfo info;
        if (ioctl(fd, EVIOCGABS(abs.code), &info) == 0) {
            shape.setAbs(abs.code, info.value);
        }
    }
}

} // namespace

unique_ptr<GrabbedPad> GrabbedPad::open(const string &eventPath, const PadMapping &mapping, string &error) {
    int fd = ::open(eventPath.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        error = "cannot open " + eventPath + ": " + strerror(errno);
        return nullptr;
    }
    unsigned char keyBits[KEY_MAX / 8 + 1] = {};
    unsigned char absBits[ABS_MAX / 8 + 1] = {};
    ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keyBits)), keyBits);
    ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(absBits)), absBits);
    vector<int> keyCodes;
    for (int code = 0; code < KEY_MAX; ++code) {
        if (testBit(keyBits, code)) {
            keyCodes.push_back(code);
        }
    }
    vector<EvdevAbs> absCodes;
    for (int code = 0; code < ABS_MAX; ++code) {
        if (testBit(absBits, code)) {
            input_absinfo info;
            if (ioctl(fd, EVIOCGABS(code), &info) == 0) {
                absCodes.push_back({code, info.minimum, info.maximum});
            }
        }
    }

    if (ioctl(fd, EVIOCGRAB, 1) != 0) {
        error = "cannot grab " + eventPath + ": " + strerror(errno);
        ::close(fd);
        return nullptr;
    }
    unique_ptr<GrabbedPad> pad(new GrabbedPad(fd, EvdevPadState(keyCodes, absCodes), mapping));
    readState(fd, pad->shape_, keyCodes, absCodes);
    pad->keyCodes_ = keyCodes;
    pad->absCodes_ = absCodes;
    return pad;
}

GrabbedPad::~GrabbedPad() {
    if (fd_ >= 0) {
        ioctl(fd_, EVIOCGRAB, 0);
        ::close(fd_);
    }
}

bool GrabbedPad::poll() {
    input_event events[32];
    for (;;) {
        ssize_t bytes = read(fd_, events, sizeof(events));
        if (bytes < 0) {
            return errno == EAGAIN || errno == EINTR; // ENODEV: the pad was unplugged
        }
        if (bytes == 0) {
            return false;
        }
        for (size_t i = 0; i < static_cast<size_t>(bytes) / sizeof(input_event); ++i) {
            const input_event &event = events[i];
            if (event.type == EV_KEY) {
                shape_.setKey(event.code, event.value);
            } else if (event.type == EV_ABS) {
                shape_.setAbs(event.code, event.value);
            } else if (event.type == EV_SYN && event.code == SYN_DROPPED) {
                readState(fd_, shape_, keyCodes_, absCodes_); // the queue overflowed: start again from the device
            }
        }
    }
}

ControllerState GrabbedPad::state() const {
    return applyMapping(mapping_, shape_.raw());
}

//*******************************
// findEventNode
//*******************************
string findEventNode(int vendor, int product, int version, const vector<string> &exclude) {
    for (int i = 0; i < 64; ++i) {
        string path = "/dev/input/event" + to_string(i);
        bool excluded = false;
        for (const string &one : exclude) {
            excluded = excluded || one == path;
        }
        if (excluded) {
            continue;
        }
        int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            continue;
        }
        input_id id;
        unsigned char keys[KEY_MAX / 8 + 1] = {};
        bool found = ioctl(fd, EVIOCGID, &id) == 0 && id.vendor == vendor && id.product == product &&
                     id.version == version && ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) >= 0 &&
                     (testBit(keys, BTN_GAMEPAD) || testBit(keys, BTN_TRIGGER) || testBit(keys, BTN_JOYSTICK));
        ::close(fd);
        if (found) {
            return path;
        }
    }
    return string();
}

//*******************************
// Siblings
//*******************************
Siblings::~Siblings() {
    release();
}

void Siblings::release() {
    for (int fd : fds_) {
        ioctl(fd, EVIOCGRAB, 0);
        ::close(fd);
    }
    fds_.clear();
}

void Siblings::grab(int vendor, int product, const vector<string> &ownPaths) {
    for (int i = 0; i < 64; ++i) {
        string path = "/dev/input/event" + to_string(i);
        bool own = false;
        for (const string &mine : ownPaths) {
            own = own || mine == path;
        }
        if (own) {
            continue;
        }
        int fd = ::open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            continue;
        }
        input_id id;
        unsigned char keys[KEY_MAX / 8 + 1] = {};
        bool take = ioctl(fd, EVIOCGID, &id) == 0 && id.vendor == vendor && id.product == product;
        if (take && ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) >= 0 && testBit(keys, KEY_PLAYPAUSE)) {
            take = false; // the Reset key's device is the daemon's own business
        }
        if (take && ioctl(fd, EVIOCGRAB, 1) == 0) {
            fds_.push_back(fd);
        } else {
            ::close(fd);
        }
    }
}

} // namespace abpad
