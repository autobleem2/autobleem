/* padsim - test-only virtual gamepads for the test VMs (never on a real image).
 *
 * Reads line commands from a virtio-serial port and drives up to four uinput devices, each looking to SDL exactly
 * like a real pad on Linux - the name, the ids, the version (SDL's GUID, so the same line of its database is found)
 * and the buttons and axes in the kernel driver's order. So the launcher, the emulators, RetroArch and the Apps see
 * the pads a tester would plug in, and a raw-joystick screen (PSC-Bios's mapping wizard) sees real axes, hats and
 * buttons. Whatever a tester does with a real pad - plug it in, pull it out, a second one, switch it off, run its
 * battery down, charge it on a cable - has a command here.
 *
 * Pads: `@<n> <command>` sends a command to pad n (1..4); without it, pad 1. Pad 1 starts plugged in as an x360,
 * the others unplugged until `@n plug` (or `@n profile ...`).
 *
 * Profiles (`profile <name> [usb|bt]`; the pad is pulled out and plugged back in as the new one):
 *   x360     a wired Xbox 360 pad on xpad (GUID 030000005e0400008e02000014010000) - the default; USB only
 *   ds4      a DualShock 4 v2 on hid-playstation/hid-sony: USB 030000004c050000cc09000011810000,
 *            Bluetooth 050000004c050000cc09000000810000 ("PS4 Controller" in SDL's database)
 *   generic  a pad SDL knows nothing about (no mapping - what makes the wizard start by itself); USB or BT
 * The face buttons have the Xbox names on every profile (a b x y = Cross Circle Square Triangle on a DS4), so a
 * test script works with any of them.
 *
 * Power and battery:
 *   unplug / plug        a USB pad's cable pulled out / put back; a Bluetooth pad switched off (or out of range) /
 *                        on again
 *   battery <0..100>     the pad's battery level (a DS4 or generic pad; an x360 is wired and has none); `battery off`
 *                        takes the battery node away
 *   cable in|out         a charging cable: a USB pad is plugged in / pulled out; a Bluetooth pad stays on Bluetooth
 *                        and charges ("Charging", "Full" at 100) / runs on its battery again ("Discharging")
 * A battery is a power_supply node like the Sony drivers' - /run/padsim/power_supply/ps-controller-battery-<mac>/
 * {capacity,status,type,scope}, pad n's mac aa:bb:cc:00:ab:0n - which the launcher reads when started with
 * AB_PAD_BATTERY_DIR pointing there (abvm.py padsim-install sets that up). uinput cannot give a pad a serial, so
 * the launcher cannot tie a battery to a player: it shows as a pad of its own.
 *
 * Protocol, one command per line, one reply per line ("ok ..." or "err <msg>"):
 *   ping                              ok padsim 5 - then per pad: <n>:<profile>/<usb|bt>/<plugged|unplugged>, and
 *                                     kbd:<plugged|unplugged>
 *   press <btn> | release <btn>       a b x y l1 r1 l2 r2 select start guide l3 r3 (l2/r2 also move the trigger axis)
 *   hold <btn> <ms>                   pressed, then released after ms
 *   stick <left|right> <x> <y>        -32768..32767 each
 *   trigger <l2|r2> <0..255>
 *   dpad <up|down|left|right|center>  also up-left, up-right, down-left, down-right
 *   reset                             everything released and centred
 *   profile <x360|ds4|generic> [usb|bt] | plug | unplug | battery <0..100>|off | cable in|out
 *
 * A USB keyboard ("AutoBleem Test Keyboard", 1209:ab02, starts unplugged; the kernel repeats a held key):
 *   kbd plug | kbd unplug
 *   kbd press <key> | kbd release <key> | kbd tap <key> [ms]   (60 ms by default)
 *   kbd combo <key>+<key>...          pressed left to right, released right to left (ctrl+alt+delete)
 *   kbd type <text>                   the rest of the line, as a US layout types it (capitals and symbols shifted)
 *   kbd reset                         every key released
 * Keys: a-z 0-9 enter esc backspace tab space minus equal leftbrace rightbrace backslash semicolon apostrophe
 * grave comma dot slash capslock f1-f12 up down left right home end pageup pagedown insert delete shift rshift
 * ctrl rctrl alt altgr meta menu printscreen pause.
 * Every pad and the keyboard are reset whenever the host side goes away, so a client that dies mid-press leaves
 * nothing held.
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
#define PADS 4

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
    int hasBattery;
    int smallSticks;                                       /* sticks 0..255 (centre 128), not -32768..32767 */
};

/* xpad */
static const struct button x360Buttons[] = {
    {"a", BTN_A},        {"b", BTN_B},        {"x", BTN_X},           {"y", BTN_Y},
    {"l1", BTN_TL},      {"r1", BTN_TR},      {"select", BTN_SELECT}, {"start", BTN_START},
    {"guide", BTN_MODE}, {"l3", BTN_THUMBL},  {"r3", BTN_THUMBR},     {0, 0},
};
/* hid-playstation / hid-sony's DualShock 4: Cross South, Circle East, Triangle North, Square West */
static const struct button ds4Buttons[] = {
    {"a", BTN_SOUTH},       {"b", BTN_EAST},      {"y", BTN_NORTH},    {"x", BTN_WEST},
    {"l1", BTN_TL},         {"r1", BTN_TR},       {"l2", BTN_TL2},     {"r2", BTN_TR2},
    {"select", BTN_SELECT}, {"start", BTN_START}, {"guide", BTN_MODE}, {"l3", BTN_THUMBL},
    {"r3", BTN_THUMBR},     {0, 0},
};

static const struct profile profiles[] = {
    {"x360", "Microsoft X-Box 360 pad", 0x045e, 0x028e, 0x0114, 0, x360Buttons, 0, 0, 0},
    {"ds4", "Wireless Controller", 0x054c, 0x09cc, 0x8111, 0x8100, ds4Buttons, 1, 1, 1},
    /* ids no pad has: SDL has no mapping for it */
    {"generic", "AutoBleem Test Pad", 0x1209, 0xab01, 0x0100, 0x0100, ds4Buttons, 1, 1, 1},
};

/* the USB keyboard: key names as a tester types them, in no particular order */
static const struct button keys[] = {
    {"a", KEY_A},           {"b", KEY_B},         {"c", KEY_C},           {"d", KEY_D},
    {"e", KEY_E},           {"f", KEY_F},         {"g", KEY_G},           {"h", KEY_H},
    {"i", KEY_I},           {"j", KEY_J},         {"k", KEY_K},           {"l", KEY_L},
    {"m", KEY_M},           {"n", KEY_N},         {"o", KEY_O},           {"p", KEY_P},
    {"q", KEY_Q},           {"r", KEY_R},         {"s", KEY_S},           {"t", KEY_T},
    {"u", KEY_U},           {"v", KEY_V},         {"w", KEY_W},           {"x", KEY_X},
    {"y", KEY_Y},           {"z", KEY_Z},         {"1", KEY_1},           {"2", KEY_2},
    {"3", KEY_3},           {"4", KEY_4},         {"5", KEY_5},           {"6", KEY_6},
    {"7", KEY_7},           {"8", KEY_8},         {"9", KEY_9},           {"0", KEY_0},
    {"enter", KEY_ENTER},   {"esc", KEY_ESC},     {"backspace", KEY_BACKSPACE}, {"tab", KEY_TAB},
    {"space", KEY_SPACE},   {"minus", KEY_MINUS}, {"equal", KEY_EQUAL},   {"leftbrace", KEY_LEFTBRACE},
    {"rightbrace", KEY_RIGHTBRACE}, {"backslash", KEY_BACKSLASH}, {"semicolon", KEY_SEMICOLON},
    {"apostrophe", KEY_APOSTROPHE}, {"grave", KEY_GRAVE}, {"comma", KEY_COMMA}, {"dot", KEY_DOT},
    {"slash", KEY_SLASH},   {"capslock", KEY_CAPSLOCK},
    {"f1", KEY_F1},         {"f2", KEY_F2},       {"f3", KEY_F3},         {"f4", KEY_F4},
    {"f5", KEY_F5},         {"f6", KEY_F6},       {"f7", KEY_F7},         {"f8", KEY_F8},
    {"f9", KEY_F9},         {"f10", KEY_F10},     {"f11", KEY_F11},       {"f12", KEY_F12},
    {"up", KEY_UP},         {"down", KEY_DOWN},   {"left", KEY_LEFT},     {"right", KEY_RIGHT},
    {"home", KEY_HOME},     {"end", KEY_END},     {"pageup", KEY_PAGEUP}, {"pagedown", KEY_PAGEDOWN},
    {"insert", KEY_INSERT}, {"delete", KEY_DELETE}, {"shift", KEY_LEFTSHIFT}, {"rshift", KEY_RIGHTSHIFT},
    {"ctrl", KEY_LEFTCTRL}, {"rctrl", KEY_RIGHTCTRL}, {"alt", KEY_LEFTALT}, {"altgr", KEY_RIGHTALT},
    {"meta", KEY_LEFTMETA}, {"menu", KEY_COMPOSE}, {"printscreen", KEY_SYSRQ}, {"pause", KEY_PAUSE},
    {0, 0},
};

/* `kbd type`: a character as the key a US layout types it with, shifted or not */
static const char unshifted[] = "`1234567890-=[]\\;',./";
static const char shifted[] = "~!@#$%^&*()_+{}|:\"<>?";
static const int punctuation[] = {KEY_GRAVE, KEY_1,     KEY_2,         KEY_3,          KEY_4,         KEY_5,
                                  KEY_6,     KEY_7,     KEY_8,         KEY_9,          KEY_0,         KEY_MINUS,
                                  KEY_EQUAL, KEY_LEFTBRACE, KEY_RIGHTBRACE, KEY_BACKSLASH, KEY_SEMICOLON,
                                  KEY_APOSTROPHE, KEY_COMMA, KEY_DOT, KEY_SLASH};

static int kbdfd = -1;

struct pad {
    const struct profile *profile;
    int bluetooth;
    int fd;          /* the uinput device, -1: unplugged */
    int level;       /* battery percent, -1: no battery node */
    int cable;       /* a charging cable in */
};

static struct pad pads[PADS];
static int ctlfd = -1;

static void emit(struct pad *p, int type, int code, int value) {
    if (p->fd < 0)
        return;
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.type = type;
    ev.code = code;
    ev.value = value;
    if (write(p->fd, &ev, sizeof(ev)) != (ssize_t)sizeof(ev))
        perror("padsim: write(uinput)");
}

static void syn(struct pad *p) { emit(p, EV_SYN, SYN_REPORT, 0); }

static int buttonCode(struct pad *p, const char *name) {
    for (const struct button *b = p->profile->buttons; b->name; b++)
        if (!strcmp(name, b->name))
            return b->code;
    return -1;
}

static void setupAxis(int fd, int code, int min, int max, int fuzz, int flat) {
    struct uinput_abs_setup abs;
    memset(&abs, 0, sizeof(abs));
    abs.code = code;
    abs.absinfo.minimum = min;
    abs.absinfo.maximum = max;
    abs.absinfo.fuzz = fuzz;
    abs.absinfo.flat = flat;
    ioctl(fd, UI_SET_ABSBIT, code);
    ioctl(fd, UI_ABS_SETUP, &abs);
}

static void mac(int index, char *out, size_t size) { snprintf(out, size, "aa:bb:cc:00:ab:%02x", index + 1); }

static int plugPad(struct pad *p) {
    if (p->fd >= 0)
        return 0;
    const struct profile *pr = p->profile;
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("padsim: open(/dev/uinput)");
        return -1;
    }
    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    for (const struct button *b = pr->buttons; b->name; b++)
        ioctl(fd, UI_SET_KEYBIT, b->code);
    ioctl(fd, UI_SET_EVBIT, EV_ABS);
    /* both drivers: the sticks on X/Y and RX/RY, the triggers on Z/RZ, the d-pad a hat */
    int lo = pr->smallSticks ? 0 : -32768, hi = pr->smallSticks ? 255 : 32767;
    int fuzz = pr->smallSticks ? 0 : 16, flat = pr->smallSticks ? 0 : 128;
    setupAxis(fd, ABS_X, lo, hi, fuzz, flat);
    setupAxis(fd, ABS_Y, lo, hi, fuzz, flat);
    setupAxis(fd, ABS_Z, 0, 255, 0, 0);
    setupAxis(fd, ABS_RX, lo, hi, fuzz, flat);
    setupAxis(fd, ABS_RY, lo, hi, fuzz, flat);
    setupAxis(fd, ABS_RZ, 0, 255, 0, 0);
    setupAxis(fd, ABS_HAT0X, -1, 1, 0, 0);
    setupAxis(fd, ABS_HAT0Y, -1, 1, 0, 0);

    struct uinput_setup setup;
    memset(&setup, 0, sizeof(setup));
    setup.id.bustype = p->bluetooth ? BUS_BLUETOOTH : BUS_USB;
    setup.id.vendor = pr->vendor;
    setup.id.product = pr->product;
    setup.id.version = p->bluetooth ? pr->btVersion : pr->usbVersion;
    snprintf(setup.name, sizeof(setup.name), "%s", pr->deviceName);
    char phys[32];
    if (p->bluetooth)
        mac((int)(p - pads), phys, sizeof(phys));
    else
        snprintf(phys, sizeof(phys), "padsim/usb%d", (int)(p - pads));
    ioctl(fd, UI_SET_PHYS, phys);
    ioctl(fd, UI_DEV_SETUP, &setup);
    if (ioctl(fd, UI_DEV_CREATE) < 0) {
        perror("padsim: UI_DEV_CREATE");
        close(fd);
        return -1;
    }
    p->fd = fd;
    return 0;
}

static void unplugPad(struct pad *p) {
    if (p->fd < 0)
        return;
    ioctl(p->fd, UI_DEV_DESTROY);
    close(p->fd);
    p->fd = -1;
}

/* a stick value in -32768..32767 in the profile's own range (a DS4 reports 0..255, centre 128) */
static int stickValue(struct pad *p, int v) {
    if (!p->profile->smallSticks)
        return v;
    return (v + 32768) * 255 / 65535;
}

/* a reply is dropped rather than block: the host may have gone away between the command and its answer */
static void reply(const char *msg) {
    char line[256];
    int n = snprintf(line, sizeof(line), "%s\n", msg);
    struct pollfd p = {ctlfd, POLLOUT, 0};
    if (poll(&p, 1, 500) == 1 && (p.revents & POLLOUT)) {
        if (write(ctlfd, line, n) != n)
            perror("padsim: write(port)");
    }
}

static void resetPad(struct pad *p) {
    for (const struct button *b = p->profile->buttons; b->name; b++)
        emit(p, EV_KEY, b->code, 0);
    emit(p, EV_ABS, ABS_X, stickValue(p, 0));
    emit(p, EV_ABS, ABS_Y, stickValue(p, 0));
    emit(p, EV_ABS, ABS_RX, stickValue(p, 0));
    emit(p, EV_ABS, ABS_RY, stickValue(p, 0));
    emit(p, EV_ABS, ABS_Z, 0);
    emit(p, EV_ABS, ABS_RZ, 0);
    emit(p, EV_ABS, ABS_HAT0X, 0);
    emit(p, EV_ABS, ABS_HAT0Y, 0);
    syn(p);
}

static int triggerAxis(const char *name) {
    if (!strcmp(name, "l2"))
        return ABS_Z;
    if (!strcmp(name, "r2"))
        return ABS_RZ;
    return -1;
}

static int setButton(struct pad *p, const char *name, int down) {
    int axis = triggerAxis(name);
    int code = buttonCode(p, name);
    if (axis < 0 && code < 0)
        return -1;
    if (axis >= 0)
        emit(p, EV_ABS, axis, down ? 255 : 0);
    if (code >= 0)
        emit(p, EV_KEY, code, down);
    syn(p);
    return 0;
}

static int clampInt(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static int writeFile(const char *dir, const char *name, const char *text) {
    char path[160];
    snprintf(path, sizeof(path), "%s/%s", dir, name);
    FILE *f = fopen(path, "w");
    if (!f)
        return -1;
    fputs(text, f);
    fclose(f);
    return 0;
}

/* the pad's power_supply node as its level and cable say - gone when it has no battery or is switched off */
static int syncBattery(struct pad *p) {
    char dir[128], address[24];
    mac((int)(p - pads), address, sizeof(address));
    snprintf(dir, sizeof(dir), BATTERY_ROOT "/ps-controller-battery-%s", address);
    if (p->level < 0 || p->fd < 0) {
        const char *files[] = {"capacity", "status", "type", "scope"};
        for (size_t i = 0; i < 4; i++) {
            char path[160];
            snprintf(path, sizeof(path), "%s/%s", dir, files[i]);
            unlink(path);
        }
        rmdir(dir);
        return 0;
    }
    const char *status = !p->cable ? "Discharging\n" : p->level >= 100 ? "Full\n" : "Charging\n";
    char capacity[16];
    snprintf(capacity, sizeof(capacity), "%d\n", p->level);
    mkdir("/run/padsim", 0755);
    mkdir(BATTERY_ROOT, 0755);
    mkdir(dir, 0755);
    if (writeFile(dir, "type", "Battery\n") || writeFile(dir, "scope", "Device\n") ||
        writeFile(dir, "status", status) || writeFile(dir, "capacity", capacity))
        return -1;
    return 0;
}

static void ping(void) {
    char msg[256];
    int n = snprintf(msg, sizeof(msg), "ok padsim 5");
    for (int i = 0; i < PADS; i++)
        n += snprintf(msg + n, sizeof(msg) - n, " %d:%s/%s/%s", i + 1, pads[i].profile->name,
                      pads[i].bluetooth ? "bt" : "usb", pads[i].fd >= 0 ? "plugged" : "unplugged");
    snprintf(msg + n, sizeof(msg) - n, " kbd:%s", kbdfd >= 0 ? "plugged" : "unplugged");
    reply(msg);
}

/* ------------------------------------------------------------------ the USB keyboard */

static void msleep(int ms) {
    struct timespec ts = {ms / 1000, (long)(ms % 1000) * 1000000L};
    nanosleep(&ts, NULL);
}

static void kemit(int code, int down) {
    if (kbdfd < 0)
        return;
    struct input_event ev[2];
    memset(ev, 0, sizeof(ev));
    ev[0].type = EV_KEY;
    ev[0].code = code;
    ev[0].value = down;
    ev[1].type = EV_SYN;
    ev[1].code = SYN_REPORT;
    if (write(kbdfd, ev, sizeof(ev)) != (ssize_t)sizeof(ev))
        perror("padsim: write(uinput keyboard)");
}

static int keyCode(const char *name) {
    for (const struct button *k = keys; k->name; k++)
        if (!strcmp(name, k->name))
            return k->code;
    return -1;
}

static int plugKbd(void) {
    if (kbdfd >= 0)
        return 0;
    int fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (fd < 0) {
        perror("padsim: open(/dev/uinput)");
        return -1;
    }
    ioctl(fd, UI_SET_EVBIT, EV_KEY);
    ioctl(fd, UI_SET_EVBIT, EV_REP); /* the kernel repeats a held key, as it does for a real keyboard */
    for (const struct button *k = keys; k->name; k++)
        ioctl(fd, UI_SET_KEYBIT, k->code);
    struct uinput_setup setup;
    memset(&setup, 0, sizeof(setup));
    setup.id.bustype = BUS_USB;
    setup.id.vendor = 0x1209; /* ids no keyboard has */
    setup.id.product = 0xab02;
    setup.id.version = 0x0110;
    snprintf(setup.name, sizeof(setup.name), "AutoBleem Test Keyboard");
    ioctl(fd, UI_SET_PHYS, "padsim/kbd");
    ioctl(fd, UI_DEV_SETUP, &setup);
    if (ioctl(fd, UI_DEV_CREATE) < 0) {
        perror("padsim: UI_DEV_CREATE (keyboard)");
        close(fd);
        return -1;
    }
    kbdfd = fd;
    return 0;
}

static void unplugKbd(void) {
    if (kbdfd < 0)
        return;
    ioctl(kbdfd, UI_DEV_DESTROY);
    close(kbdfd);
    kbdfd = -1;
}

static void resetKbd(void) {
    for (const struct button *k = keys; k->name; k++)
        kemit(k->code, 0);
}

static void typeText(const char *text) {
    for (const char *c = text; *c; c++) {
        int code = -1, shift = 0;
        char lower[2] = {(char)(*c >= 'A' && *c <= 'Z' ? *c - 'A' + 'a' : *c), 0};
        if (*c == ' ') {
            code = KEY_SPACE;
        } else if ((*c >= 'a' && *c <= 'z') || (*c >= 'A' && *c <= 'Z') || (*c >= '0' && *c <= '9')) {
            code = keyCode(lower);
            shift = *c >= 'A' && *c <= 'Z';
        } else if (strchr(unshifted, *c)) {
            code = punctuation[strchr(unshifted, *c) - unshifted];
        } else if (strchr(shifted, *c)) {
            code = punctuation[strchr(shifted, *c) - shifted];
            shift = 1;
        }
        if (code < 0)
            continue; /* no key types it on a US layout */
        if (shift)
            kemit(KEY_LEFTSHIFT, 1);
        kemit(code, 1);
        msleep(20);
        kemit(code, 0);
        if (shift)
            kemit(KEY_LEFTSHIFT, 0);
        msleep(20);
    }
}

/* "kbd <command>": the keyboard's own commands */
static void handleKbd(char *line) {
    char cmd[16] = {0}, a1[64] = {0}, a2[16] = {0};
    int n = sscanf(line, "%15s %63s %15s", cmd, a1, a2);
    if (n >= 1 && !strcmp(cmd, "plug")) {
        reply(plugKbd() == 0 ? "ok" : "err cannot create the keyboard");
        return;
    }
    if (n >= 1 && !strcmp(cmd, "unplug")) {
        unplugKbd();
        reply("ok");
        return;
    }
    if (kbdfd < 0) {
        reply("err the keyboard is unplugged (kbd plug)");
        return;
    }
    if (n >= 1 && !strcmp(cmd, "type")) {
        char *text = strstr(line, "type");
        text += 4;
        if (*text == ' ')
            text++;
        typeText(text);
        reply("ok");
    } else if (n >= 1 && !strcmp(cmd, "reset")) {
        resetKbd();
        reply("ok");
    } else if ((!strcmp(cmd, "press") || !strcmp(cmd, "release")) && n >= 2) {
        int code = keyCode(a1);
        if (code < 0) {
            reply("err unknown key");
            return;
        }
        kemit(code, cmd[0] == 'p');
        reply("ok");
    } else if (!strcmp(cmd, "tap") && n >= 2) {
        int code = keyCode(a1);
        if (code < 0) {
            reply("err unknown key");
            return;
        }
        kemit(code, 1);
        msleep(n >= 3 ? clampInt(atoi(a2), 0, 60000) : 60);
        kemit(code, 0);
        reply("ok");
    } else if (!strcmp(cmd, "combo") && n >= 2) {
        /* ctrl+alt+delete: pressed left to right, released right to left */
        int codes[8], count = 0;
        for (char *part = strtok(a1, "+"); part && count < 8; part = strtok(NULL, "+")) {
            codes[count] = keyCode(part);
            if (codes[count] < 0) {
                reply("err unknown key");
                return;
            }
            count++;
        }
        for (int i = 0; i < count; i++) {
            kemit(codes[i], 1);
            msleep(20);
        }
        msleep(60);
        for (int i = count - 1; i >= 0; i--) {
            kemit(codes[i], 0);
            msleep(20);
        }
        reply("ok");
    } else {
        reply("err kbd plug|unplug|press|release <key>|tap <key> [ms]|combo <k>+<k>|type <text>|reset");
    }
}

static void handleLine(char *line) {
    if (!strncmp(line, "kbd ", 4)) {
        handleKbd(line + 4);
        return;
    }
    struct pad *p = &pads[0];
    if (line[0] == '@') {
        int n = atoi(line + 1);
        if (n < 1 || n > PADS) {
            reply("err pad: @1..@4");
            return;
        }
        p = &pads[n - 1];
        line = strchr(line, ' ');
        if (!line) {
            reply("err a command after the pad");
            return;
        }
    }
    char cmd[16] = {0}, a1[32] = {0}, a2[32] = {0}, a3[32] = {0};
    int n = sscanf(line, "%15s %31s %31s %31s", cmd, a1, a2, a3);
    if (n < 1) {
        reply("err empty");
    } else if (!strcmp(cmd, "ping")) {
        ping();
    } else if (!strcmp(cmd, "unplug")) {
        unplugPad(p);
        syncBattery(p);
        reply("ok");
    } else if (!strcmp(cmd, "plug")) {
        reply(plugPad(p) == 0 && syncBattery(p) == 0 ? "ok" : "err cannot create the pad");
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
            unplugPad(p);
            p->profile = wanted;
            p->bluetooth = bt;
            if (!wanted->hasBattery)
                p->level = -1;
            reply(plugPad(p) == 0 && syncBattery(p) == 0 ? "ok" : "err cannot create the pad");
        }
    } else if (!strcmp(cmd, "battery") && n >= 2) {
        if (!strcmp(a1, "off")) {
            p->level = -1;
        } else if (!p->profile->hasBattery) {
            reply("err this pad is wired, it has no battery");
            return;
        } else {
            p->level = clampInt(atoi(a1), 0, 100);
        }
        reply(syncBattery(p) == 0 ? "ok" : "err cannot write " BATTERY_ROOT);
    } else if (!strcmp(cmd, "cable") && n >= 2) {
        int in = !strcmp(a1, "in");
        if (!in && strcmp(a1, "out")) {
            reply("err cable in|out");
            return;
        }
        p->cable = in;
        if (!p->bluetooth) {
            /* a USB pad lives on its cable */
            if (in) {
                if (plugPad(p) != 0) {
                    reply("err cannot create the pad");
                    return;
                }
            } else {
                unplugPad(p);
            }
        }
        reply(syncBattery(p) == 0 ? "ok" : "err cannot write " BATTERY_ROOT);
    } else if (p->fd < 0) {
        reply("err the pad is unplugged");
    } else if (!strcmp(cmd, "reset")) {
        resetPad(p);
        reply("ok");
    } else if ((!strcmp(cmd, "press") || !strcmp(cmd, "release")) && n >= 2) {
        reply(setButton(p, a1, cmd[0] == 'p') == 0 ? "ok" : "err unknown button");
    } else if (!strcmp(cmd, "hold") && n >= 3) {
        if (setButton(p, a1, 1) != 0) {
            reply("err unknown button");
            return;
        }
        int ms = clampInt(atoi(a2), 0, 60000);
        struct timespec ts = {ms / 1000, (long)(ms % 1000) * 1000000L};
        nanosleep(&ts, NULL);
        setButton(p, a1, 0);
        reply("ok");
    } else if (!strcmp(cmd, "stick") && n >= 4) {
        int left = !strcmp(a1, "left");
        if (!left && strcmp(a1, "right")) {
            reply("err unknown stick");
            return;
        }
        emit(p, EV_ABS, left ? ABS_X : ABS_RX, stickValue(p, clampInt(atoi(a2), -32768, 32767)));
        emit(p, EV_ABS, left ? ABS_Y : ABS_RY, stickValue(p, clampInt(atoi(a3), -32768, 32767)));
        syn(p);
        reply("ok");
    } else if (!strcmp(cmd, "trigger") && n >= 3) {
        int axis = triggerAxis(a1);
        if (axis < 0) {
            reply("err unknown trigger");
            return;
        }
        int value = clampInt(atoi(a2), 0, 255);
        emit(p, EV_ABS, axis, value);
        if (p->profile->triggerButtons)
            emit(p, EV_KEY, axis == ABS_Z ? BTN_TL2 : BTN_TR2, value > 0);
        syn(p);
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
        emit(p, EV_ABS, ABS_HAT0X, hx);
        emit(p, EV_ABS, ABS_HAT0Y, hy);
        syn(p);
        reply("ok");
    } else {
        reply("err bad command");
    }
}

static void resetAll(void) {
    for (int i = 0; i < PADS; i++)
        resetPad(&pads[i]);
    resetKbd();
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <virtio-serial port>\n", argv[0]);
        return 1;
    }
    for (int i = 0; i < PADS; i++) {
        pads[i].profile = &profiles[0];
        pads[i].fd = -1;
        pads[i].level = -1;
        syncBattery(&pads[i]); /* no battery node left over from a previous run */
    }
    mkdir("/run/padsim", 0755);
    mkdir(BATTERY_ROOT, 0755);
    if (plugPad(&pads[0]) != 0)
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
                resetAll();
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
                resetAll();
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
    for (int i = 0; i < PADS; i++)
        unplugPad(&pads[i]);
    unplugKbd();
    return 0;
}
