//
// Created by screemer on 2020-03-03.
//

#ifndef ABFLASHKIT_LED_H
#define ABFLASHKIT_LED_H

#include <string>
#define LED_RED      0
#define LED_GREEN    1
#define LED_ORANGE   2
#define BLINK_RED    3
#define BLINK_GREEN  4
#define BLINK_ORANGE 5
#define LED_OFF      6


using namespace std;

class led {
public:
    void init();
    void setMode(int mode);

private:
    void setColor(int color);
    void setFile(string path, string val);
    bool lastBlink = false;

    string redPath = "/sys/class/leds/red/brightness";
    string greenPath = "/sys/class/leds/green/brightness";
};


#endif //ABFLASHKIT_LED_H
