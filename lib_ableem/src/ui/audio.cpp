#include "ableem/ui/audio.h"
#include "sdl_common.h"
#include <iostream>

namespace ableem {

//******************
// Sound
//******************
namespace {
void destroyChunk(void *c) { if (c) Mix_FreeChunk(static_cast<Mix_Chunk *>(c)); }
void destroyMusic(void *m) { if (m) Mix_FreeMusic(static_cast<Mix_Music *>(m)); }
} // namespace

Sound::Sound() : handle(nullptr) {}
Sound::Sound(void *chunk) : handle(chunk, destroyChunk) {}

Sound Sound::load(const std::string &path) {
    Mix_Chunk *c = Mix_LoadWAV(path.c_str());
    if (!c) {
        std::cout << "Could not load sound: " << path << " (" << Mix_GetError() << ")" << std::endl;
    }
    return Sound(c);
}

bool Sound::valid() const { return handle != nullptr; }

void Sound::play() const {
    if (handle) Mix_PlayChannel(-1, static_cast<Mix_Chunk *>(handle.get()), 0);
}

//******************
// Music
//******************
Music::Music() : handle(nullptr) {}
Music::Music(void *m) : handle(m, destroyMusic) {}

Music Music::load(const std::string &path) {
    Mix_Music *m = Mix_LoadMUS(path.c_str());
    if (!m) {
        std::cout << "Could not load music: " << path << " (" << Mix_GetError() << ")" << std::endl;
    }
    return Music(m);
}

bool Music::valid() const { return handle != nullptr; }

void Music::play(int loops) const {
    if (handle) Mix_PlayMusic(static_cast<Mix_Music *>(handle.get()), loops);
}

void Music::setVolume(int volume0to128) const {
    Mix_VolumeMusic(volume0to128);
}

bool Music::isPlaying() const {
    return Mix_PlayingMusic() != 0;
}

void Music::halt() const {
    Mix_HaltMusic();
}

void Music::fadeOut(int ms) const {
    Mix_FadeOutMusic(ms);
}

//******************
// Audio
//******************
struct Audio::Impl {
    bool open = false;
};

Audio::Audio() : impl(new Impl()) {}
Audio::~Audio() { close(); }

bool Audio::open(int frequency, int channels, int chunkSize) {
    close(); // matches the original restartAudio(): fully close any previously opened device(s) first

    if (Mix_OpenAudio(frequency, MIX_DEFAULT_FORMAT, channels, chunkSize) == -1) {
        std::cout << "Unable to open audio: " << Mix_GetError() << std::endl;
        impl->open = false;
        return false;
    }
    impl->open = true;

    const char *driver = SDL_GetCurrentAudioDriver();
    if (driver) {
        std::cout << "Audio subsystem initialized; driver = " << driver << "." << std::endl;
    } else {
        std::cout << "Audio subsystem not initialized." << std::endl;
    }
    return true;
}

void Audio::close() {
    // SDL_mixer devices can be opened more than once (each call to Mix_OpenAudio increments a ref count);
    // this is the "close until it reports actually closed" idiom the original code used before forking an
    // emulator or reopening at a different frequency.
    int frequency, channels;
    Uint16 format;
    int timesOpened = Mix_QuerySpec(&frequency, &format, &channels);
    for (int i = 0; i < timesOpened; i++) {
        Mix_CloseAudio();
    }
    while (Mix_QuerySpec(&frequency, &format, &channels)) {
        Mix_CloseAudio();
    }
    impl->open = false;
}

bool Audio::isOpen() const { return impl->open; }

std::string Audio::driverName() const {
    const char *driver = SDL_GetCurrentAudioDriver();
    return driver ? driver : "";
}

} // namespace ableem
