//
// SurpriseGame: the "Surprise" easter egg on the About screen. Start toggles it on; the starfield already
// behind the credits keeps running as the backdrop. A small Galaga-style shoot-em-up: dpad left/right moves
// the ship, Cross fires, a formation of aliens marches side to side and occasionally one peels off to dive
// at the player before looping back in from the top of the screen. Select restarts on the spot; Start exits
// back to the normal About screen.
//
// Sprites and sound effects are Kenney's "Space Shooter Redux" (CC0 / public domain, www.kenney.nl), copied
// into resources/surprise_game/ - see the license.txt alongside them.
#pragma once

#include <ableem/ui/renderer.h>
#include <ableem/ui/texture.h>
#include <ableem/ui/font.h>
#include <ableem/ui/audio.h>
#include <random>
#include <vector>

class TextRenderer;

//******************
// SurpriseSprites
//******************
// Loaded once by GuiAbout::init() and handed to every render() call - Texture is a cheap shared handle.
struct SurpriseSprites {
    ableem::Texture ship;
    ableem::Texture enemy1;
    ableem::Texture enemy2;
    ableem::Texture ufo;          // the diving alien
    ableem::Texture laserPlayer;
    ableem::Texture laserEnemy;
};

//******************
// SurpriseSounds
//******************
// Loaded once by GuiAbout::init() and assigned to SurpriseGame::sounds before the first update()/fire().
struct SurpriseSounds {
    ableem::Sound playerShoot;
    ableem::Sound enemyShoot;
    ableem::Sound explosion;
    ableem::Sound playerHit;
    ableem::Sound waveClear;
};

//******************
// SurpriseGame
//******************
class SurpriseGame {
public:
    // set once by the owning screen before the first update()/fire(), same as StarFx::renderer
    SurpriseSounds sounds;

    void reset(unsigned int nowTicks);

    // moveLeft/moveRight are the current dpad state, sampled every frame (like the rest of the app's screens)
    void update(unsigned int nowTicks, bool moveLeft, bool moveRight);

    // called for every frame Cross is held down; internally rate-limited to one shot per fire cooldown
    void fire(unsigned int nowTicks);

    void render(ableem::Renderer &renderer, TextRenderer &text, const ableem::Font &font,
                const SurpriseSprites &sprites);

    bool gameOver() const { return lives <= 0; }
    int currentScore() const { return score; }
    int currentHighScore() const { return highScore; }
    // seeds the in-session high score from Config's saved value; never lowers it. Call once, before reset().
    void seedHighScore(int hs) { if (hs > highScore) highScore = hs; }

private:
    struct Bullet {
        float x = 0, y = 0;
        bool alive = false;
    };

    struct Alien {
        float baseX = 0, baseY = 0;   // formation slot, before the formation offset/step-down is added
        float x = 0, y = 0;           // current position
        bool alive = true;
        bool diving = false;          // swooping down at the ship along the bezier dive curve
        bool returning = false;       // dive finished off-screen; looping back in from the top now
        float diveT = 0;              // 0..1 progress through the dive curve
        float diveStartX = 0, diveStartY = 0;
        float diveTargetX = 0;
        float returnT = 0;            // 0..1 progress flying back down into the formation slot
        int kind = 0;                 // 0/1 -> enemy1/enemy2 sprite while in formation
    };

    std::vector<Alien> aliens;
    std::vector<Bullet> playerBullets;
    std::vector<Bullet> alienBullets;

    float shipX = 0;
    int lives = 3;
    int score = 0;
    int highScore = 0;
    int wave = 1;

    void bumpScore(int delta) {
        score += delta;
        if (score > highScore) highScore = score;
    }

    float formationDir = 1.0f;
    float formationX = 0;
    float formationY = 0;
    float formationSpeed = 0.6f;

    unsigned int lastTicks = 0;
    unsigned int lastShotTicks = 0;
    unsigned int hitInvulnUntil = 0;
    unsigned int nextDiveAtTicks = 0;

    std::mt19937 rng{std::random_device{}()};

    void spawnWave();
    void maybeStartDive(unsigned int nowTicks);
    void updateAliens(float dtFrames, unsigned int nowTicks);
    void updateBullets(float dtFrames);
    void handleCollisions(unsigned int nowTicks);
    int awayFromFormationCount() const;   // aliens currently diving or returning
};
