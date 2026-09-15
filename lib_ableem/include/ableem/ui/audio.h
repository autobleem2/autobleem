#pragma once

#include <memory>
#include <string>
#include "types.h"

namespace ableem {

//******************
// Sound
//******************
// A short sound effect (WAV). Cheap shared handle.
class ABLEEM_API Sound {
public:
    Sound();
    Sound(const Sound &) = default;
    Sound &operator=(const Sound &) = default;

    static Sound load(const std::string &path);
    bool valid() const;
    void play() const; // plays on the first free channel, does nothing if not valid

private:
    explicit Sound(void *chunk);
    std::shared_ptr<void> handle;
};

//******************
// Music
//******************
// Background music (mod/ogg/mp3/...). Only one Music can be playing at a time (SDL_mixer's own limitation).
class ABLEEM_API Music {
public:
    Music();
    Music(const Music &) = default;
    Music &operator=(const Music &) = default;

    static Music load(const std::string &path);
    bool valid() const;

    void play(int loops = -1) const; // loops < 0 means "forever"
    void setVolume(int volume0to128) const;
    bool isPlaying() const;
    void halt() const;
    void fadeOut(int ms) const;

private:
    explicit Music(void *musicHandle);
    std::shared_ptr<void> handle;
};

//******************
// Audio
//******************
// Owns the SDL_mixer device. One instance lives inside GuiBase.
class ABLEEM_API Audio {
public:
    Audio();
    ~Audio();
    Audio(const Audio &) = delete;
    Audio &operator=(const Audio &) = delete;

    bool open(int frequency = 44100, int channels = 2, int chunkSize = 1024);
    // closes the device completely, looping until SDL_mixer reports it is actually closed (SDL_mixer
    // devices can be opened more than once by the same process; this undoes all of them)
    void close();
    bool isOpen() const;
    std::string driverName() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace ableem
