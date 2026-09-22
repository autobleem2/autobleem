// padtest - what SDL tells a program about its joysticks, printed.
//
// The subject the shim is tested against. Run it twice, once plain and once with libabpad.so
// preloaded, and the difference between the two printouts *is* what the shim does - in a form that
// can go in a bug report. It deliberately uses SDL's **joystick** API and nothing else, because that
// is the surface the shim stands in front of and the only one SDL 1.2 has.
//
//   padtest [--seconds N]
//
//   ./padtest                                             what the app would see on its own
//   LD_PRELOAD=./libabpad.so ./padtest                    what the shim shows it
//
// Built against SDL2 here. The same source compiles against SDL 1.2 where a libSDL-1.2 is to hand,
// which is how the SDL1 half of the shim gets a subject of its own.

#define SDL_MAIN_HANDLED
#include <SDL2/SDL.h>
#ifdef main
#undef main
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>

int main(int argc, char *argv[]) {
    int seconds = 30;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--seconds") == 0 && i + 1 < argc) {
            seconds = atoi(argv[++i]);
        }
    }

    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_JOYSTICK | SDL_INIT_VIDEO) != 0) {
        // no video on a headless test box is fine; the joysticks are what matter
        if (SDL_Init(SDL_INIT_JOYSTICK) != 0) {
            printf("SDL_Init failed: %s\n", SDL_GetError());
            return 1;
        }
    }

    int count = SDL_NumJoysticks();
    printf("SDL_NumJoysticks() = %d\n", count);
    if (count <= 0) {
        printf("nothing to read - is a pad plugged in, and is abpadd running?\n");
        SDL_Quit();
        return 1;
    }

    SDL_Joystick *pads[8] = {};
    for (int i = 0; i < count && i < 8; ++i) {
        pads[i] = SDL_JoystickOpen(i);
        if (!pads[i]) {
            printf("[%d] will not open: %s\n", i, SDL_GetError());
            continue;
        }
        const char *name = SDL_JoystickName(pads[i]);
        printf("[%d] %s\n", i, name ? name : "(no name)");
        printf("     %d buttons, %d axes, %d hats\n", SDL_JoystickNumButtons(pads[i]), SDL_JoystickNumAxes(pads[i]),
               SDL_JoystickNumHats(pads[i]));
    }

    printf("\nreading for %d seconds - press things\n", seconds);
    Uint32 until = SDL_GetTicks() + static_cast<Uint32>(seconds) * 1000;
    while (SDL_GetTicks() < until) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            switch (event.type) {
            case SDL_JOYBUTTONDOWN:
                printf("pad %d  button %d  down\n", event.jbutton.which, event.jbutton.button);
                break;
            case SDL_JOYBUTTONUP:
                printf("pad %d  button %d  up\n", event.jbutton.which, event.jbutton.button);
                break;
            case SDL_JOYHATMOTION:
                printf("pad %d  hat %d     %d\n", event.jhat.which, event.jhat.hat, event.jhat.value);
                break;
            case SDL_JOYAXISMOTION:
                // only what a person would call a movement, or a resting trigger drowns the log
                if (event.jaxis.value > 8000 || event.jaxis.value < -8000) {
                    printf("pad %d  axis %d    %d\n", event.jaxis.which, event.jaxis.axis, event.jaxis.value);
                }
                break;
            case SDL_QUIT:
                printf("SDL_QUIT - the hotkey, if the shim is in front of us\n");
                until = 0;
                break;
            default:
                break;
            }
        }
        fflush(stdout);
        SDL_Delay(16);
    }

    for (SDL_Joystick *pad : pads) {
        if (pad) {
            SDL_JoystickClose(pad);
        }
    }
    SDL_Quit();
    printf("done\n");
    return 0;
}
