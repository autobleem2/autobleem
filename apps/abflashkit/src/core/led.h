//
// Led: the console's power LED (red and green under /sys/class/leds) as the flasher's progress indicator
// - green while idle, blinking green while backing up, red while the recovery flag is set and the kernel
// written, off just before the reboot. SysfsLed drives the real one from a thread; NullLed logs.
//
#pragma once

#include <atomic>
#include <string>
#include <thread>

//******************
// Led
//******************
enum class LedMode { Off, Red, Green, Orange, BlinkRed, BlinkGreen, BlinkOrange };

class Led {
public:
    virtual ~Led() = default;
    virtual void setMode(LedMode mode) = 0;
    static const char *name(LedMode mode);
};

//******************
// SysfsLed
//******************
// writes /sys/class/leds/{red,green}/brightness; a blink is a thread toggling every BlinkPeriodMs, joined
// by the destructor (the 2020 tool forked a blinker it never reaped)
class SysfsLed : public Led {
public:
    explicit SysfsLed(const std::string &ledsDir = "/sys/class/leds");
    ~SysfsLed() override;
    void setMode(LedMode mode) override;

    static constexpr int BlinkPeriodMs = 500;

private:
    void write(const std::string &led, bool on);
    void show(LedMode mode, bool phase);
    void blinkLoop();

    std::string ledsDir_;
    std::atomic<LedMode> mode_;
    std::atomic<bool> stopping_;
    std::thread blinker_;
};

//******************
// NullLed
//******************
class NullLed : public Led {
public:
    void setMode(LedMode mode) override;
    LedMode mode = LedMode::Off;
};
