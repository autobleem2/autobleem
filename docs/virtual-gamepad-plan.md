# The virtual gamepad for third-party Apps (plan)

The console's `Apps/` hold programs nobody here compiled: RetroBoot 1.2's ports (amiberry, doom,
eduke32, openbor, opentyrian, sdlpop, shadowwarrior, wolf4sdl) and whatever a user drops in. The ones
ported with an eye on the PSC pad work; the rest take the pad badly or not at all. RetroBoot papered
over this with a launcher that injected a pad configuration into the app's process and masked SDL's
joystick routines in the shared library. This is that, done deliberately and from our own
`gamecontrollerdb.txt`.

## Why they break

The PSC pad is not what a Linux port expects. Our own mapping line for it says so:

```
030000004c050000da0c000011010000,Playstation Classic Controller,
    a:b2,b:b1,x:b3,y:b0,back:b8,start:b9,
    leftshoulder:b6,lefttrigger:b4,rightshoulder:b7,righttrigger:b5,
    dpup:-a1,dpdown:+a1,dpleft:-a0,dpright:+a0,platform:Linux,
```

Ten buttons, two axes, **no hat, no analog sticks**, and the face buttons in Sony's own order -
`b0` is Triangle, `b2` is Cross. The pad nearly every Linux port was written against is the wired
Xbox 360 one (`030000005e0400008e02000010010000`): `a:b0,b:b1,x:b2,y:b3`, eleven buttons, six axes,
the d-pad on **hat 0**, the triggers on axes 2 and 5.

Four distinct failures, wanting different answers:

1. **The indices are shifted.** An app that hardcodes "button 0 = fire" fires on Triangle.
2. **The d-pad is on the wrong kind of input.** An app reading `hat0` reads nothing at all; an app
   reading axes 0/1 as the left stick gets a digital d-pad there - usually acceptable, sometimes not.
3. **No analog sticks and no hat exist to read.** An app that steers only by stick cannot be steered.
4. **The app never opens a joystick.** The DOS-era ports (wolf4sdl, sdlpop, opentyrian) are keyboard
   games; on a console with no keyboard they are unplayable whatever the pad reports.

## What the SDL version does and does not change

**SDL 1.2 has only the joystick API** - `SDL_Joystick*` and the four joystick event types. There is
no GameController abstraction, no GUID, no mapping database: that layer arrived with SDL2. So for an
SDL 1.2 binary the joystick API is the only surface that exists, and interposing it is not one option
among several, it is the whole of what can be done from outside.

That is also what makes one design cover everything: **the joystick API is the common denominator**.
It is exported by SDL 1.2 and by SDL2, both times as an interposable dynamic symbol, so a single
shim reaches every binary we are likely to meet - including ones not written yet.

One caveat to keep in view. An **SDL2 app using the GameController API** (`SDL_GameControllerGetButton`
and friends) never calls the joystick entry points itself: SDL2's own internals reach its joystick
code through the dynapi `_REAL` symbols *inside* `libSDL2.so`, which `LD_PRELOAD` cannot intercept.
Such an app is therefore untouched by joystick interposition - and does not need it, as long as SDL
has a mapping for the pad, which `SDL_GAMECONTROLLERCONFIG` hands it for free. Interposing the dozen
`SDL_GameController*` entry points as well is cheap and buys full control there too; it goes in once
the joystick half is proven. (Worth confirming on a console binary with `nm -D` that the app's SDL2
really is a dynapi build - if it is not, the joystick interposition catches those apps too.)

So, by app:

| the app is | covered by |
|---|---|
| SDL 1.2, any input style | the joystick interposition (the only lever there is) |
| SDL2, raw joystick API | the joystick interposition |
| SDL2, GameController API | `SDL_GAMECONTROLLERCONFIG` now; the `SDL_GameController*` interposition later |
| keyboard-only, either SDL | the shim's keyboard mode (pad pushed as key events) |
| statically linked SDL | nothing from outside - see "Later" |

## Where we are allowed to stand

Everything a third-party App loads is ours. `rc/app_env.sh` is sourced by every app's `run.sh`
(`tools/install_autobleem.py` rewrote them all to do it), it links the site's libs pack into
`/tmp/applib` and sets `LD_LIBRARY_PATH` - so we own the SDL 1.2 and SDL2 those programs link
against, their environment and their command line. Adding `LD_PRELOAD` there costs nothing and
reaches every app at once.

## The decision

**An `LD_PRELOAD` interposer, `libabpad.so`, that owns the pad and shows the app a pad it knows.**

The shim reads the physical pads itself from **evdev** (`/dev/input/event*`), resolves them through
the launcher's `gamecontrollerdb.txt` exactly as the launcher does, and presents the app **one
synthetic joystick in a well-known layout** (the wired X360 pad by default) - or a keyboard, or
both. The app's own SDL never sees a real pad: the joystick entry points are answered by us.

**One library, not two.** SDL 1.2 and SDL2 share symbol *names* with different meanings
(`SDL_JoystickName` takes an index in SDL 1.2 and a handle in SDL2; the event structs are unrelated),
so the shim decides which ABI it is serving at its first intercepted call -
`dlsym(RTLD_NEXT, "SDL_GameControllerAddMapping")` answers it - and branches. That keeps `app_env.sh`
free of detection and means a new app needs no thought at all.

Why not the two alternatives:

- **A `uinput` virtual device** (a daemon creating a real kernel device the app just finds) is the
  more general answer - it would reach non-SDL apps too - and the AutoBleem kernel has
  `CONFIG_INPUT_UINPUT=y`. But a **stock-firmware** console is the common case and we do not know
  that its kernel has uinput at all; and even where it works, the *real* pad stays visible next to
  the virtual one, so an app taking joystick 0 still takes the wrong one unless we hide the real
  node. Worth having later for non-SDL apps (see "Later"), not as the mechanism.
- **Patching SDL 1.2 / SDL2 in the libs pack** would work (we ship them) but only for apps that use
  *our* copy, it is invisible to anyone reading the app's folder, and it cannot be configured per
  app. A preload is the same power, per app, reversible, and legible.

## Reading the pad: the one piece of real difficulty

A `gamecontrollerdb.txt` line is written in **SDL's** numbering - `b3` means "the fourth button SDL
enumerated for this device", not an evdev key code. Reading evdev ourselves means reproducing SDL's
enumeration order: its Linux joystick driver walks a fixed list of `BTN_*` codes, and the `ABS_*`
axes in code order with `ABS_HAT0X..ABS_HAT3Y` pulled out as hats. That order is table-driven, has
been stable across SDL 2.0.x, and is **pure logic with no I/O** - so it is the part that gets unit
tests, and the rest of the shim stays thin.

The device's GUID is formed the same way SDL forms it, from `EVIOCGID`: bus, vendor, product and
version as little-endian 16-bit values each followed by a zero pair - which is exactly how
`030000004c050000da0c000011010000` decodes (bus 0003, vendor 054c, product 0cda, version 0111). So
the shim matches db lines by GUID, and the pad the **pscbios wizard** mapped is the pad the apps get.

## The pieces

| piece | what it is |
|---|---|
| `apps/abpad/src/core/` | `abpad_core`: the evdev enumeration order, the GUID, the `gamecontrollerdb` line parser, the virtual-pad layouts, the profile file, the physical-to-virtual translation and the pad-to-key translation. No I/O, no SDL, tested from `tests/apps/test_abpad_core.cpp`. |
| `apps/abpad/src/shim/` | The interposer: the evdev reader, the ABI detection, and the SDL entry points for both ABIs. |
| `payload/Autobleem/rc/app_env.sh` | Exports `LD_PRELOAD`, `AB_PAD_DB`, `AB_PAD_PROFILE`, and `SDL_GAMECONTROLLERCONFIG` for SDL2 GameController apps. |
| `Apps/<name>/pad.ini` | The app's profile. Absent = the default profile next to `app_env.sh`. |

### What the shim answers

*Joystick mode* - the app is shown one (or two) pads in the chosen layout and nothing else:
`SDL_NumJoysticks`, `SDL_JoystickOpen/Close/Index`, `SDL_JoystickName`, `SDL_JoystickNum{Axes,
Buttons,Hats,Balls}`, `SDL_JoystickGet{Axis,Button,Hat,Ball}`, `SDL_JoystickUpdate`,
`SDL_JoystickEventState`, and on SDL2 the `SDL_JoystickGetGUID*`/`SDL_JoystickInstanceID`/
`SDL_JoystickNameForIndex` family. Events are produced in `SDL_PollEvent`/`SDL_WaitEvent`/
`SDL_PeepEvents`, where the app's own joystick events (if its SDL found the pad anyway) are dropped
on the way past.

*Keyboard mode* - the same virtual pad, but each element is pushed as a key event, so a keyboard-only
game is playable. The key names are the app's business and live in its `pad.ini`.

*Either mode* - a **hotkey** (Start+Select by default) pushes `SDL_QUIT`, because several of these
apps have no way out.

`SDL_Init`/`SDL_InitSubSystem` are intercepted too: the shim needs no subsystem from the app's SDL,
but a keyboard-only app must not be able to leave the joystick machinery switched off.

### The profile

```ini
# Apps/wolf4sdl/pad.ini
mode = keyboard          # joystick | keyboard | both | off
virtual = x360           # x360 | ps3 | psc  - the pad a joystick-mode app is shown
players = 1
hotkey = start+select    # quit the app
key.a = Left Ctrl        # keyboard mode: a virtual-pad element -> an SDL key name
key.dpup = Up
```

## Steps

1. `abpad_core` + its tests: the SDL evdev enumeration table, the GUID, the db line parser, the
   virtual layouts, the profile parser, the translation from a physical pad's raw state to the
   virtual pad's, and from the virtual pad to key events. Windows-testable, no device needed.
2. The shim: evdev reading, ABI detection, the SDL2 joystick entry points and the event queue.
   Built for psc and rpi.
3. The SDL 1.2 half of the same entry points.
4. `app_env.sh` and the profile defaults; the shim shipped in the console package
   (`Autobleem/lib/`), a default profile per known app.
5. The `SDL_GameController*` entry points, if the console's apps turn out to need them.
6. A launcher-side page: which pads the shim will see and what an app is shown (the Hardware
   Information screen already lists the pads and their mappings).

## Testing it without a console

Testing the shim against the stick's opaque third-party binaries is guesswork: when nothing happens
there is no way to tell a wrong mapping from a preload that never took. Two subjects of our own fix
that, and both build for every architecture we target.

- **`padtest`** - a few dozen lines, built twice, once against SDL 1.2 and once against SDL2, that
  prints what SDL tells it: the joystick count, each one's name and its button/axis/hat counts, and
  then every change. Run it with and without `LD_PRELOAD` and the difference *is* the shim's
  behaviour, in a form that can go in a bug report. This is the harness; it comes with step 2.
- **SDLPoP** (the Prince of Persia port) as the real game. GPL-3.0, so it sits next to our own
  licence; it builds from source for the PC, the Pi and the console; and it is **keyboard-driven**,
  which makes it the honest test of keyboard mode rather than a joystick app that would have worked
  anyway. Built here rather than taken as a binary, it can also be rebuilt against SDL 1.2 if a
  second SDL1 subject is wanted.

So the order of proof is: the core's tests on any host, `padtest` under the preload on the dev host
and the Pi, SDLPoP played with a pad on the Pi, and only then the console's own Apps.

Steps 2-5 cannot be fully verified here - the console's Apps need a console. Everything before that
can: `make_rpi.sh` cross-builds ARM on this machine and a Pi runs the preload for real.

## Later

- **uinput**, once it is known whether a stock console has it: it reaches non-SDL apps and statically
  linked ones, and a virtual *keyboard* device would make keyboard mode work for those too.
- The same shim is useful on the Pi and the PC stick, where the Apps idea does not exist yet.

## Open questions for the console

- Which of the eight apps are SDL 1.2 and which are SDL2? (`readelf -d` on each binary.)
- Is the apps' SDL2 a dynapi build (`nm -D | grep _REAL`)? Decides whether step 5 is needed.
- Does a stock-firmware kernel have `uinput` (`/dev/uinput`, or `CONFIG_INPUT_UINPUT` in
  `/proc/config.gz`)? Decides whether "Later" is possible at all.
- Do any of the apps carry their own pad configuration that should be honoured instead?
- Is any of them statically linked against SDL?
