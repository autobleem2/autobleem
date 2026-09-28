/* padsim - a test-only virtual gamepad for the test VMs (never on a real image).
 *
 * Reads line commands from a virtio-serial port and drives a uinput device that looks to SDL exactly like a
 * wired Xbox 360 pad on Linux's xpad driver: the same name, ids and version (GUID
 * 030000005e0400008e02000014010000, "Xbox 360 Controller" in SDL's database), the same eleven buttons in the
 * same order (no L2/R2 buttons - the triggers are axes), the same six axes and the d-pad as hat 0. So the
 * launcher, the emulators, RetroArch and the Apps see the pad a tester would plug in, and a raw-joystick screen
 * (PSC-Bios's mapping wizard) sees real axes, hats and buttons.
 *
 * Protocol, one command per line, one reply per line ("ok ..." or "err <msg>"):
 *   ping                              ok padsim 2
 *   press <btn> | release <btn>       a b x y l1 r1 l2 r2 select start guide l3 r3 (l2/r2: the trigger to 255/0)
 *   hold <btn> <ms>                   pressed, then released after ms
 *   stick <left|right> <x> <y>        -32768..32767 each
 *   trigger <l2|r2> <0..255>
 *   dpad <up|down|left|right|center>  also up-left, up-right, down-left, down-right
 *   reset                             everything released and centred
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
#include <time.h>
#include <unistd.h>

static int uifd = -1;
static int ctlfd = -1;

/* xpad's buttons, in the kernel's code order - which is SDL's b0..b10 */
static const struct {
    const char *name;
    int code;
} buttons[] = {
    {"a", BTN_A},          {"b", BTN_B},         {"x", BTN_X},           {"y", BTN_Y},
    {"l1", BTN_TL},        {"r1", BTN_TR},       {"select", BTN_SELECT}, {"start", BTN_START},
    {"guide", BTN_MODE},   {"l3", BTN_THUMBL},   {"r3", BTN_THUMBR},
};
#define BUTTONS (sizeof(buttons) / sizeof(buttons[0]))

static void emit(int type, int code, int value) {
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
    for (size_t i = 0; i < BUTTONS; i++)
        if (!strcmp(name, buttons[i].name))
            return buttons[i].code;
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

static void setupUinput(void) {
    uifd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
    if (uifd < 0) {
        perror("padsim: open(/dev/uinput)");
        exit(1);
    }
    ioctl(uifd, UI_SET_EVBIT, EV_KEY);
    for (size_t i = 0; i < BUTTONS; i++)
        ioctl(uifd, UI_SET_KEYBIT, buttons[i].code);
    ioctl(uifd, UI_SET_EVBIT, EV_ABS);
    /* xpad's ranges: the sticks with its fuzz and flat, the triggers 0..255, the d-pad a hat */
    setupAxis(ABS_X, -32768, 32767, 16, 128);
    setupAxis(ABS_Y, -32768, 32767, 16, 128);
    setupAxis(ABS_Z, 0, 255, 0, 0);
    setupAxis(ABS_RX, -32768, 32767, 16, 128);
    setupAxis(ABS_RY, -32768, 32767, 16, 128);
    setupAxis(ABS_RZ, 0, 255, 0, 0);
    setupAxis(ABS_HAT0X, -1, 1, 0, 0);
    setupAxis(ABS_HAT0Y, -1, 1, 0, 0);

    struct uinput_setup setup;
    memset(&setup, 0, sizeof(setup));
    setup.id.bustype = BUS_USB;
    setup.id.vendor = 0x045e;
    setup.id.product = 0x028e;
    setup.id.version = 0x0114;
    strcpy(setup.name, "Microsoft X-Box 360 pad");
    ioctl(uifd, UI_DEV_SETUP, &setup);
    ioctl(uifd, UI_DEV_CREATE);
}

/* a reply is dropped rather than block: the host may have gone away between the command and its answer */
static void reply(const char *msg) {
    char line[128];
    int n = snprintf(line, sizeof(line), "%s\n", msg);
    struct pollfd p = {ctlfd, POLLOUT, 0};
    if (poll(&p, 1, 500) == 1 && (p.revents & POLLOUT)) {
        if (write(ctlfd, line, n) != n)
            perror("padsim: write(port)");
    }
}

static void resetPad(void) {
    for (size_t i = 0; i < BUTTONS; i++)
        emit(EV_KEY, buttons[i].code, 0);
    emit(EV_ABS, ABS_X, 0);
    emit(EV_ABS, ABS_Y, 0);
    emit(EV_ABS, ABS_RX, 0);
    emit(EV_ABS, ABS_RY, 0);
    emit(EV_ABS, ABS_Z, 0);
    emit(EV_ABS, ABS_RZ, 0);
    emit(EV_ABS, ABS_HAT0X, 0);
    emit(EV_ABS, ABS_HAT0Y, 0);
    syn();
}

static int triggerCode(const char *name) {
    if (!strcmp(name, "l2"))
        return ABS_Z;
    if (!strcmp(name, "r2"))
        return ABS_RZ;
    return -1;
}

static int setButton(const char *name, int down) {
    int code = triggerCode(name);
    if (code >= 0) {
        emit(EV_ABS, code, down ? 255 : 0);
        syn();
        return 0;
    }
    code = buttonCode(name);
    if (code < 0)
        return -1;
    emit(EV_KEY, code, down);
    syn();
    return 0;
}

static int clampInt(int v, int lo, int hi) { return v < lo ? lo : v > hi ? hi : v; }

static void handleLine(char *line) {
    char cmd[16] = {0}, a1[32] = {0}, a2[32] = {0}, a3[32] = {0};
    int n = sscanf(line, "%15s %31s %31s %31s", cmd, a1, a2, a3);
    if (n < 1) {
        reply("err empty");
    } else if (!strcmp(cmd, "ping")) {
        reply("ok padsim 2");
    } else if (!strcmp(cmd, "reset")) {
        resetPad();
        reply("ok");
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
        emit(EV_ABS, left ? ABS_X : ABS_RX, clampInt(atoi(a2), -32768, 32767));
        emit(EV_ABS, left ? ABS_Y : ABS_RY, clampInt(atoi(a3), -32768, 32767));
        syn();
        reply("ok");
    } else if (!strcmp(cmd, "trigger") && n >= 3) {
        int code = triggerCode(a1);
        if (code < 0) {
            reply("err unknown trigger");
            return;
        }
        emit(EV_ABS, code, clampInt(atoi(a2), 0, 255));
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
    setupUinput();
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
    ioctl(uifd, UI_DEV_DESTROY);
    return 0;
}
