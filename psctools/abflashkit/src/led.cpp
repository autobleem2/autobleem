//
// Created by screemer on 2020-03-03.
//

#include "led.h"

#include <string>
#include <zconf.h>
#include <SDL2/SDL_timer.h>
#include <iostream>
#include <array>
#include <memory>
#include <algorithm>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>

static int *currentMode;
static bool *active;

using namespace std;
void led::setMode(int mode)
{
    *currentMode = mode;
}

void led::setFile(string path, string val)
{
    string command="echo "+val+" >> "+path;
    array<char, 2048> buffer;
    string result;

    unique_ptr<FILE, decltype(&pclose)> pipe(popen(command.c_str(), "r"), pclose);
    if (!pipe) {
        throw runtime_error("popen() failed!");
    }
    while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
        result += buffer.data();
    }
    if (!result.empty()) {
        result.erase(remove(result.begin(), result.end(), '\n'));
    }

}

void led::setColor(int color)
{
    if ((color!=LED_GREEN) && (color!=LED_RED) && (color !=LED_ORANGE) && (color != LED_OFF)) return;

    if (color==LED_GREEN)
    {
        setFile(greenPath,"1");
        setFile(redPath, "0");
    }

    if (color==LED_RED)
    {
        setFile(greenPath,"0");
        setFile(redPath, "1");
    }

    if (color==LED_ORANGE)
    {
        setFile(greenPath,"1");
        setFile(redPath, "1");
    }
    if (color==LED_OFF)
    {
        setFile(greenPath,"0");
        setFile(redPath, "0");
    }
}

void led::init()
{
    currentMode = static_cast<int *>(mmap(NULL, sizeof *currentMode, PROT_READ | PROT_WRITE,
                                          MAP_SHARED | MAP_ANONYMOUS, -1, 0));
    active = static_cast<bool *>(mmap(NULL, sizeof *active, PROT_READ | PROT_WRITE,
                                          MAP_SHARED | MAP_ANONYMOUS, -1, 0));
    *active = true;
    *currentMode = LED_GREEN;
    if (fork() == 0)
    {
        while (*active)
        {

            lastBlink = !lastBlink;
            if (*currentMode==LED_GREEN || *currentMode==LED_RED || *currentMode==LED_ORANGE || *currentMode==LED_OFF)
            {
                setColor(*currentMode);
            } else
            {
                if (lastBlink)
                {
                    int newMode = -1;
                    if (*currentMode == BLINK_GREEN) newMode = LED_GREEN;
                    if (*currentMode == BLINK_RED) newMode = LED_RED;
                    if (*currentMode == BLINK_ORANGE) newMode = LED_ORANGE;
                    setColor(newMode);
                } else
                {
                    setColor(LED_OFF);
                }
            }
            SDL_Delay(500);
        }
    }
}