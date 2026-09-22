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
| statically linked, GameController | `SDL_GAMECONTROLLERCONFIG_FILE` pointed at the daemon's mappings file |
| SDL2, GameController API | the `SDL_GameController*` interposition - the same virtual pad as everyone else |
| keyboard-only, either SDL | the shim's keyboard mode (pad pushed as key events) |
| statically linked SDL | nothing from outside - see "Later" |

## Where we are allowed to stand

Everything a third-party App loads is ours. `rc/app_env.sh` is sourced by every app's `run.sh`
(`tools/install_autobleem.py` rewrote them all to do it), it links the site's libs pack into
`/tmp/applib` and sets `LD_LIBRARY_PATH` - so we own the SDL 1.2 and SDL2 those programs link
against, their environment and their command line. Adding `LD_PRELOAD` there costs nothing and
reaches every app at once.

## The decision

**A daemon that is the virtual gamepad, and an `LD_PRELOAD` shim that serves it to each app.**

`abpadd` is the one process that reads the pads. It runs SDL2 and reads every pad through the
**GameController API** with our `gamecontrollerdb.txt` - so a pad resolves in an App exactly as it
resolves in the launcher and in pcsx, by construction, because it is the same code reading the same
file. It publishes each player's controller state in shared memory. `libabpad.so`, preloaded into the
app, reads that and answers the app's joystick calls with a pad the app knows - the wired X360 pad by
default - or with key presses, or both.

**Why the daemon is a separate process and not just a library.** Reading the pad through SDL2 needs a
libSDL2 in the process doing the reading. An SDL 1.2 app cannot have one: both libraries export
`SDL_Init`, `SDL_PollEvent`, `SDL_NumJoysticks`, `SDL_JoystickOpen` and the rest, and the dynamic
linker binds each reference to whichever object comes first, so an SDL1 app could end up calling SDL2
code with SDL1 expectations - memory corruption, not merely wrong input. The escapes are `dlmopen`
into a private link-map namespace (glibc-specific, exotic, and the console is glibc 2.24) or a
privately renamed SDL2 linked into the shim. A second process is cheaper than either and needs no
tricks at all, and it buys one reader of the hardware instead of one per app.

What that leaves in the app's process is a shim with **no SDL, no evdev and no mapping logic** - it
reads a struct and answers questions - which is the right amount of code to have living inside
somebody else's game loop.

**Hotplug and players are the daemon's problem, not the app's.** `abpadd` watches SDL's device events
and keeps a slot per player, so player two unplugging does not shuffle player three into their place,
and a pad plugged in mid-game simply starts working. The shim, meanwhile, shows the app a **fixed**
number of pads (the profile's `players`), present from the first call to the last: an unplugged pad
reads centred and unpressed. SDL 1.2 has no notion of a joystick arriving or leaving, and plenty of
SDL2-era ports handle it badly, so no app is ever asked to cope with one.

**A pad no database knows is still playable.** SDL only offers a pad as a GameController when it has a
mapping for its GUID, so an unknown pad would otherwise be invisible. The daemon guesses a mapping
from the pad's shape, hands it to `SDL_GameControllerAddMapping`, and everything downstream carries on
as if the database had known it.

An app **statically linked against SDL** cannot be interposed at all - its calls never reach a symbol
we can replace. `app_env.sh` detects that and leaves it alone; uinput would be the only route.

Why not the alternatives:

- **A `uinput` virtual device** (a kernel device the app just finds) is the more general answer - it
  would reach non-SDL apps too - and the AutoBleem kernel has `CONFIG_INPUT_UINPUT=y`. But a
  **stock-firmware** console is the common case and we do not know that its kernel has uinput at all;
  and even where it works the *real* pad stays visible next to the virtual one, so an app taking
  joystick 0 still takes the wrong one unless we hide the real node. Worth having later for non-SDL
  and statically linked apps (see "Later"), not as the mechanism.
- **Patching SDL 1.2 / SDL2 in the libs pack** would work (we ship them) but only for apps that use
  *our* copy, it is invisible to anyone reading the app's folder, and it cannot be configured per app.
- **Reading evdev in the shim** and applying the mapping line ourselves - the first design here - is
  self-contained and needs no daemon, but it reimplements SDL's evdev numbering and its axis scaling,
  and a copy of somebody else's logic is a copy that can drift from it.

## The pieces

| piece | what it is |
|---|---|
| `apps/abpad/src/core/` | `abpad_core`: the element vocabulary and controller state, the gamecontrollerdb line a virtual layout is written as, the layouts themselves, the shared-memory contract, the per-app profile and the key table. No I/O, no SDL, tested from `tests/apps/test_abpad_core.cpp`. |
| `apps/abpad/src/daemon/` | `abpadd`, the virtual gamepad: SDL2 + our database, hotplug, a slot per player, the shared block. `--probe` prints what SDL makes of every pad, `--watch` prints what is being published - between them they answer "is it the daemon or the app?" without a debugger. |
| `apps/abpad/src/shim/` | `libabpad.so`, preloaded: reads the block and answers the app's SDL. One library for both ABIs, which it tells apart at its first intercepted call (`dlsym(RTLD_NEXT, "SDL_GameControllerAddMapping")` answers it) - so `app_env.sh` needs no detection and a new app needs no thought. |
| `payload/Autobleem/rc/app_env.sh` | Starts `abpadd` for the app's lifetime, exports `LD_PRELOAD`, `AB_PAD_DB`, `AB_PAD_SHM`, `AB_PAD_PROFILE`, and `SDL_GAMECONTROLLERCONFIG` for SDL2 GameController apps. |
| `Apps/<name>/pad.ini` | The app's profile. Absent = the default profile next to `app_env.sh`. |

**The mappings file.** For an app the preload cannot reach - one statically linked against SDL - the
daemon also writes the mapping it resolved for each pad that is plugged in, to be handed over with
`SDL_GAMECONTROLLERCONFIG_FILE`. What goes in that file matters: pointing an app at our
`gamecontrollerdb.txt` would be *wrong*, because a file entry overrides SDL's built-in table, and for
a pad SDL already knows - a DualShock through hidapi - the built-in entry is the right one while ours
may be a stale line for another of that pad's modes. Writing back what SDL actually resolved avoids
the whole question.

**Run it as root.** SDL reaches a modern pad through hidapi, which needs `/dev/hidraw*`, and those are
root-only; without them it falls back to evdev, where the same physical pad has a different GUID
(SDL stamps the driver into it - a trailing `h`), a different layout, and may match a quite different
database line. The launcher runs as root, so anything less makes the daemon resolve the pad
differently from the launcher - the one thing it exists not to do. `abpadd` says so if it cannot get
at hidraw, and `--probe` names the driver each pad came through.

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

1. `abpad_core` + its tests: the elements and controller state, the mapping line and the virtual
   layouts, the shared-memory contract, the profile, the key table. **Done.**
2. `abpadd`: SDL2 with our database, hotplug with a slot per player, the shared block, `--probe` and
   `--watch`. **Done** - runs on the dev host against a real pad.
3. `libabpad.so`: the shared block read, the virtual pads served, the SDL2 joystick entry points and
   the event queue, the hotkey.
4. The SDL 1.2 half of the same entry points, behind the ABI detection.
5. Keyboard mode: the profile's keys pushed as key events in whichever ABI.
6. `app_env.sh`, the default profile and a profile per known app; `abpadd` and the shim shipped in the
   console package and the Pi one.
7. ~~The `SDL_GameController*` entry points~~ - **done**, and brought forward rather than left to an
   "if needed": an SDL2 app using that API would otherwise reach past the shim to the real pad and be
   the one app with a different pad, a different layout and none of the profile's remapping, keyboard
   mode or hotkey. Both views are now the same virtual pad. The controller view needs no layout - SDL's
   controller model *is* what the daemon publishes - and both a joystick and a controller event are
   raised for one press, as SDL itself does.
8. A launcher-side page: which pads the daemon sees and what an app is shown.

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
- **`Apps/` on the Pi and the PC stick**, which this is what makes possible. The ports already made
  for the console - opentyrian, quake, sdlpop, wolf4sdl, eduke32 - are ARM Linux SDL binaries; what
  stops them being useful on a Pi is the same thing that makes them awkward on the console, that they
  were built against somebody else's pad. With the daemon reading the Pi's pad through the same
  database and the shim showing each binary the pad it expects, they run there as they run here, and
  the launcher's Apps set becomes worth having on both. A 32-bit Pi runs the console's armhf binaries
  directly; a 64-bit one needs them rebuilt, which is the porters' problem, not this one's.

## Open questions for the console

- Which of the eight apps are SDL 1.2 and which are SDL2? (`readelf -d` on each binary.)
- Is the apps' SDL2 a dynapi build (`nm -D | grep _REAL`)? Decides whether step 5 is needed.
- Does a stock-firmware kernel have `uinput` (`/dev/uinput`, or `CONFIG_INPUT_UINPUT` in
  `/proc/config.gz`)? Decides whether "Later" is possible at all.
- Do any of the apps carry their own pad configuration that should be honoured instead?
- Is any of them statically linked against SDL?
