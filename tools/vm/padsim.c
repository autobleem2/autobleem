/* padsim - a test-only virtual gamepad for the test VMs (never on a real image).
 *
 * Reads line commands from a virtio-serial port and drives a uinput device that looks to SDL exactly like a real
 * pad on Linux - the name, the ids, the version (SDL's GUID, so the same line of its database is found) and the
 * buttons and axes in the kernel driver's order. So the launcher, the emulators, RetroArch and the Apps see the pad a
 * tester would plug in, and a raw-joystick screen (PSC-Bios's mapping wizard) sees real axes, hats and buttons.
 *
 * Profiles (`profile <name> [usb|bt]`; the pad is unplugged and plugged back as the new one):
 *   x360     a wired Xbox 360 pad on xpad (GUID 030000005e0400008e02000014010000) - the default; USB only
 *   ds4      a DualShock 4 v2 on hid-playstation/hid-sony: USB 030000004c050000cc09000011810000,
 *            Bluetooth 050000004c050000cc09000000810000 ("PS4 Controller" in SDL's database)
 *   generic  a pad SDL knows nothing about (no mapping - what makes the wizard start by itself); USB or BT
 * The button names are always the Xbox ones for the face buttons (a b x y = Cross Circle Square Triangle on a DS4)
 * so a test script works with every profile.
 *
 * Battery (`battery <0..100> [charging|discharging|full]`, `battery off`): a power_supply node like the Sony
 * drivers' - <dir>/ps-controller-battery-<mac>/{capacity,status,type,scope} - in /run/padsim/power_supply, which
 * the launcher reads when started with AB_PAD_BATTERY_DIR pointing there (abvm.py padsim-install sets that up).
 * uinput cannot give the pad a serial, so the launcher cannot tie the battery to a player: it shows as a pad of
 * its own.
 *
 * Protocol, one command per line, one reply per line ("ok ..." or "err <msg>"):
 *   ping                              ok padsim 3 <profile> <usb|bt> <plugged|unplugged>
 *   press <btn> | release <btn>       a b x y l1 r1 l2 r2 select start guide l3 r3 (l2/r2 also move the trigger axis)
 *   hold <btn> <ms>                   pressed, then released after ms
 *   stick <left|right> <x> <y>        -32768..32767 each
 *   trigger <l2|r2> <0..255>
 *   dpad <up|down|left|right|center>  also up-left, up-right, down-left, down-right
 *   reset                             everything released and centred
 *   profile <x360|ds4|generic> [usb|bt]
 *   unplug | plug                     the pad gone / back (a hot-plug test)
 *   battery <0..100> [charging|discharging|full] | battery off
 * The pad is reset whenever the host side goes away, so a client that dies mid-press leaves nothing held.
 *
 * Build in the guest: gcc -O2 -Wall -o padsim padsim.c   (tools/vm/abvm.py padsim-install does it)
 */

#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define BATTERY_ROOT "/run/padsim/power_supply"
#define BATTERY_MAC "aa:bb:cc:00:ab:01"
#define BATTERY_DIR BATTERY_ROOT "/ps-controller-battery-" BATTERY_MAC

struct button {
    const char *name;
    int code;
};

struct profile {
    const char *name;
    const char *deviceName;
    unsigned short vendor, product, usbVersion, btVersion; /* btVersion 0: no Bluetooth variant */
    const struct button *buttons;                          /* in the kernel driver's code order, {0} ended */
    int triggerButtons;                                    /* the driver also reports L2/R2 as buttons */
};

/* xpad */
static const struct button x360Buttons[] = {
    {"a", BTN_A},       {"b", BTN_B},         {"x", BTN_X},           {"y", BTN_Y},
    {"l1", BTN_TL},     {"r1", BTN_TR},       {"select", BTN_SELECT}, {"start", BTN_START},
    {"guide", BTN_MODE}, {"l3", BTN_THUMBL},  {"r3", BTN_THUMBR},     {0, 0},
};
/* hid-playstation / hid-sony's DualShock 4: Cross South, Circle East, Triangle North, Square West */
static const struct button ds4Buttons[] = {
    {"a", BTN_SOUTH},    {"b", BTN_EAST},      {"y", BTN_NORTH},       {"x", BTN_WEST},
    {"l1", BTN_TL},      {"r1", BTN_TR},       {"l2", BTN_TL2},        {"r2", BTN_TR2},
    {"select", BTN_SELECT}, {"start", BTN_START}, {"guide", BTN_MODE}, {"l3", BTN_THUMBL},
    {"r3", BTN_THUMBR},  {0, 0},
};

static const struct profile profiles[] = {
    {"x360", "Microsoft X-Box 360 pad", 0x045e, 0x028e, 0x0114, 0, x360Buttons, 0},
    {"ds4", "Wireless Controller", 0x054c, 0x09cc, 0x8111, 0x8100, ds4Buttons, 1},
    /* ids no pad has: SDL has no mapping for it */
    {"generic", "AutoBleem Test Pad", 0x1209, 0xab01, 0x0100, 0x0100, ds4Buttons, 1},
};

static const struct profile *current = &profiles[0];
static int bluetooth = 0;
static int uifd = -1;
static int ctlfd = -1;

static void emit(int type, int code, int value) {
    if (uifd < 0)
        return;
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = type;
    ev.code = code;
    ev.value = value;
    if (write(uifd, &ev, sizeof(ev)) != (ssize_t)sizeof(ev))
        perror("padsim: write(uinput)");
}

static void syn(void) { emit(EV_SYN, SYN_REPORT, 0); }

static int buttonCode(const char *name) {
    for (const struct button *b = current->buttons; b->name; b++)
        if (!strcmp(name, b->name))
            return b->code;
    return -1;
}

static void setupAxis(int code, int min, int max, int fuzz, int flat) {
    struct uinput_abs_setup abs;
    memset(&abs, 0, sizeof(abs));
    abs.code = code;
    abs.absinfo.minimum = min;
    abs.absinfo.maximum = max;
    abs.absinfo.fuzz = fuzz;
    abs.absinfo.flat = flat;
    ioctl(uifd, UI_SET_ABSBIT, code);
    ioctl(uifd, UI_ABS_SETUP, &abs);
}

static int plug(void) {
    if (uifd >= 0)
        return 0;
    uifd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (uifd < 0) {
        perror("padsim: open(/dev/uinput)");
        return -1;
    }
    ioctl(uifd, UI_SET_EVBIT, EV_KEY);
    for (const struct button *b = current->buttons; b->name; b++)
        ioctl(uifd, UI_SET_KEYBIT, b->code);
    ioctl(uifd, UI_SET_EVBIT, EV_ABS);
    /* both drivers: the sticks on X/Y and RX/RY, the triggers on Z/RZ, the d-pad a hat */
    int stickMin = current->buttons == x360Buttons ? -32768 : 0;
    int stickMax = current->buttons == x360Buttons ? 32767 : 255;
    int fuzz = current->buttons == x360Buttons ? 16 : 0;
    int flat = current->buttons == x360Buttons ? 128 : 0;
    setupAxis(ABS_X, stickMin, stickMax, fuzz, flat);
    setupAxis(ABS_Y, stickMin, stickMax, fuzz, flat);
    setupAxis(ABS_Z, 0, 255, 0, 0);
    setupAxis(ABS_RX, stickMin, stickMax, fuzz, flat);
    setupAxis(ABS_RY, stickMin, stickMax, fuzz, flat);
    setupAxis(ABS_RZ, 0, 255, 0, 0);
    setupAxis(ABS_HAT0X, -1, 1, 0, 0);
    setupAxis(ABS_HAT0Y, -1, 1, 0, 0);

    struct uinput_setup setup;
    memset(&setup, 0, sizeof(setup));
    setup.id.bustype = bluetooth ? BUS_BLUETOOTH : BUS_USB;
    setup.id.vendor = current->vendor;
    setup.id.product = current->product;
    setup.id.version = bluetooth ? current->btVersion : current->usbVersion;
    snprintf(setup.name, sizeof(setup.name), "%s", current->deviceName);
    ioctl(uifd, UI_SET_PHYS, bluetooth ? BATTERY_MAC : "padsim/usb0");
    ioctl(uifd, UI_DEV_SETUP, &setup);
    if (ioctl(uifd, UI_DEV_CREATE) < 0) {
        perror("padsim: UI_DEV_CREATE");
        close(uifd);
        uifd = -1;
        return -1;
    }
    return 0;
}

static void unplug(void) {
    if (uifd < 0)
        return;
    ioctl(uifd, UI_DEV_DESTROY);
    close(uifd);
    uifd = -1;
}

/* a stick value in -32768..32767 in the profile's own range (a DS4 reports 0..255, centre 128) */
static int stickValue(int v) {
    if (current->buttons == x360Buttons)
        return v;
    return (v + 32768) * 255 / 65535;
}

/* a reply is dropped rather than block: the host may have gone away between the command and its answer */
static void reply(const char *msg) {
    char line[160];
    int n = snprintf(line, sizeof(line), "%s\n", msg);
    struct pollfd p = {ctlfd, POLLOUT, 0};
    if (poll(&p, 1, 500) == 1 && (p.revents & POLLOUT)) {
        if (write(ctlfd, line, n) != n)
            perror("padsim: write(port)");
    }
}

static void resetPad(void) {
    for (const struct button *b = current->buttons; b->name; b++)
        emit(EV_KEY, b->code, 0);
    emit(EV_ABS, ABS_X, stickValue(0));
    emit(EV_ABS, ABS_Y, stickValue(0));
    emit(EV_ABS, ABS_RX, stickValue(0));
    emit(EV_ABS, ABS_RY, stickValue(0));
    emit(EV_ABS, ABS_Z, 0);
    emit(EV_ABS, ABS_RZ, 0);
    emit(EV_ABS, ABS_HAT0X, 0);
    emit(EV_ABS, ABS_HAT0Y, 0);
    syn();
}

static int triggerAxis(const char *name) {
    if (!strcmp(name, "l2"))
        return ABS_Z;
    if (!strcmp(name, "r2"))
        return ABS_RZ;
    return -1;
}

static int setButton(const char *name, int down) {
    int axis = triggerAxis(name);
    int code = buttonCode(name);
    if (axis < 0 && code < 0)
        return -1;
    if (axis >= 0)
        emit(EV_ABS, axis, down ? 255 : 0);
    if (code >= 0)
        emit(EV_KEY, code, down);
    syn();
    return 0;
}

static int clampInt(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static int writeFile(const char *path, const char *text) {
    FILE *f = fopen(path, "w");
    if (!f)
        return -1;
    fputs(text, f);
    fclose(f);
    return 0;
}

static void battery(const char *level, const char *state) {
    if (!strcmp(level, "off")) {
        unlink(BATTERY_DIR "/capacity");
        unlink(BATTERY_DIR "/status");
        unlink(BATTERY_DIR "/type");
        unlink(BATTERY_DIR "/scope");
        rmdir(BATTERY_DIR);
        reply("ok");
        return;
    }
    const char *status = !state[0] || !strcmp(state, "discharging") ? "Discharging"
                         : !strcmp(state, "charging")                ? "Charging"
                         : !strcmp(state, "full")                    ? "Full"
                                                                     : NULL;
    if (!status) {
        reply("err battery state: charging, discharging or full");
        return;
    }
    char text[16];
    snprintf(text, sizeof(text), "%d\n", clampInt(atoi(level), 0, 100));
    mkdir("/run/padsim", 0755);
    mkdir(BATTERY_ROOT, 0755);
    mkdir(BATTERY_DIR, 0755);
    char statusLine[32];
    snprintf(statusLine, sizeof(statusLine), "%s\n", status);
    if (writeFile(BATTERY_DIR "/type", "Battery\n") || writeFile(BATTERY_DIR "/scope", "Device\n") ||
        writeFile(BATTERY_DIR "/status", statusLine) || writeFile(BATTERY_DIR "/capacity", text)) {
        reply("err cannot write " BATTERY_DIR);
        return;
    }
    reply("ok");
}

static void handleLine(char *line) {
    char cmd[16] = {0}, a1[32] = {0}, a2[32] = {0}, a3[32] = {0};
    int n = sscanf(line, "%15s %31s %31s %31s", cmd, a1, a2, a3);
    if (n < 1) {
        reply("err empty");
    } else if (!strcmp(cmd, "ping")) {
        char msg[96];
        snprintf(msg, sizeof(msg), "ok padsim 3 %s %s %s", current->name, bluetooth ? "bt" : "usb",
                 uifd >= 0 ? "plugged" : "unplugged");
        reply(msg);
    } else if (!strcmp(cmd, "reset")) {
        resetPad();
        reply("ok");
    } else if (!strcmp(cmd, "unplug")) {
        unplug();
        reply("ok");
    } else if (!strcmp(cmd, "plug")) {
        reply(plug() == 0 ? "ok" : "err cannot create the pad");
    } else if (!strcmp(cmd, "profile") && n >= 2) {
        const struct profile *wanted = NULL;
        for (size_t i = 0; i < sizeof(profiles) / sizeof(profiles[0]); i++)
            if (!strcmp(a1, profiles[i].name))
                wanted = &profiles[i];
        int bt = n >= 3 && !strcmp(a2, "bt");
        if (!wanted || (n >= 3 && !bt && strcmp(a2, "usb"))) {
            reply("err profile: x360|ds4|generic [usb|bt]");
        } else if (bt && !wanted->btVersion) {
            reply("err no such pad over Bluetooth");
        } else {
            unplug();
            current = wanted;
            bluetooth = bt;
            reply(plug() == 0 ? "ok" : "err cannot create the pad");
        }
    } else if (!strcmp(cmd, "battery") && n >= 2) {
        battery(a1, n >= 3 ? a2 : "");
    } else if (uifd < 0) {
        reply("err the pad is unplugged");
    } else if ((!strcmp(cmd, "press") || !strcmp(cmd, "release")) && n >= 2) {
        reply(setButton(a1, cmd[0] == 'p') == 0 ? "ok" : "err unknown button");
    } else if (!strcmp(cmd, "hold") && n >= 3) {
        if (setButton(a1, 1) != 0) {
            reply("err unknown button");
            return;
        }
        int ms = clampInt(atoi(a2), 0, 60000);
        struct timespec ts = {ms / 1000, (long)(ms % 1000) * 1000000L};
        nanosleep(&ts, NULL);
        setButton(a1, 0);
        reply("ok");
    } else if (!strcmp(cmd, "stick") && n >= 4) {
        int left = !strcmp(a1, "left");
        if (!left && strcmp(a1, "right")) {
            reply("err unknown stick");
            return;
        }
        emit(EV_ABS, left ? ABS_X : ABS_RX, stickValue(clampInt(atoi(a2), -32768, 32767)));
        emit(EV_ABS, left ? ABS_Y : ABS_RY, stickValue(clampInt(atoi(a3), -32768, 32767)));
        syn();
        reply("ok");
    } else if (!strcmp(cmd, "trigger") && n >= 3) {
        int axis = triggerAxis(a1);
        if (axis < 0) {
            reply("err unknown trigger");
            return;
        }
        int value = clampInt(atoi(a2), 0, 255);
        emit(EV_ABS, axis, value);
        if (current->triggerButtons)
            emit(EV_KEY, axis == ABS_Z ? BTN_TL2 : BTN_TR2, value > 0);
        syn();
        reply("ok");
    } else if (!strcmp(cmd, "dpad") && n >= 2) {
        int hx = 0, hy = 0;
        if (strstr(a1, "up"))
            hy = -1;
        if (strstr(a1, "down"))
            hy = 1;
        if (strstr(a1, "left"))
            hx = -1;
        if (strstr(a1, "right"))
            hx = 1;
        if (hx == 0 && hy == 0 && strcmp(a1, "center")) {
            reply("err unknown direction");
            return;
        }
        emit(EV_ABS, ABS_HAT0X, hx);
        emit(EV_ABS, ABS_HAT0Y, hy);
        syn();
        reply("ok");
    } else {
        reply("err bad command");
    }
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <virtio-serial port>\n", argv[0]);
        return 1;
    }
    mkdir("/run/padsim", 0755);
    mkdir(BATTERY_ROOT, 0755);
    if (plug() != 0)
        return 1;
    ctlfd = open(argv[1], O_RDWR | O_NONBLOCK);
    if (ctlfd < 0) {
        perror("padsim: open(port)");
        return 1;
    }

    char buf[512];
    size_t len = 0;
    int connected = 0;
    for (;;) {
        struct pollfd p = {ctlfd, POLLIN, 0};
        int r = poll(&p, 1, 1000);
        if (r < 0) {
            if (errno == EINTR)
                continue;
            perror("padsim: poll");
            break;
        }
        if (r == 0)
            continue;
        if (!(p.revents & POLLIN)) {
            /* POLLHUP with nothing to read: the host side is not connected */
            if (connected) {
                resetPad();
                connected = 0;
            }
            len = 0; /* a half line from a client that went away never joins the next one's command */
            usleep(200000);
            continue;
        }
        char chunk[256];
        ssize_t got = read(ctlfd, chunk, sizeof(chunk));
        if (got <= 0) {
            if (got < 0 && (errno == EAGAIN || errno == EINTR))
                continue;
            if (connected) {
                resetPad();
                connected = 0;
            }
            len = 0;
            usleep(200000);
            continue;
        }
        connected = 1;
        for (ssize_t i = 0; i < got; i++) {
            char c = chunk[i];
            if (c == '\r')
                continue;
            if (c == '\n') {
                buf[len] = '\0';
                if (len > 0)
                    handleLine(buf);
                len = 0;
            } else if (len + 1 < sizeof(buf)) {
                buf[len++] = c;
            }
        }
    }
    unplug();
    return 0;
}
