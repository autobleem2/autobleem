//
// SurpriseGame: the "Surprise" easter egg on the About screen. Start toggles it on; the starfield already
// behind the credits keeps running as the backdrop. A small shoot-em-up: dpad moves the ship, Cross fires,
// Select restarts on the spot, Start exits back to the normal About screen.
//
// Enemy waves fly in staggered from the top and then hold a continuously undulating, snake-like formation
// (each row swaying on its own sine phase, slowly creeping downward) rather than a rigid marching block -
// closer to Warblade's enemy squadrons than classic Space Invaders. Aliens still occasionally peel off to
// dive at the player before looping back in from the top. Destroyed aliens sometimes drop a power-up
// (rapid-fire/autofire, a spread shot, or a piercing "power" shot) that the ship collects by flying over it,
// and more rarely (about 10% of kills) an extra life instead, collected the same way.
//
// Sprites and sound effects are Kenney's "Space Shooter Redux" (CC0 / public domain, www.kenney.nl) plus one
// CC0 "NES Shooter Music" track by SketchyLogic (opengameart.org), copied into resources/surprise_game/ -
// see the license.txt alongside them.
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
    ableem::Texture laserEnemy;   // doubles as the "power" (piercing) player shot - a bigger, meaner-looking bolt
    ableem::Texture powerupRapid;
    ableem::Texture powerupSpread;
    ableem::Texture powerupPower;
};

//******************
// SurpriseSounds
//******************
// Loaded once by GuiAbout::init() and assigned to SurpriseGame::sounds before the first update().
struct SurpriseSounds {
    ableem::Sound playerShoot;
    ableem::Sound enemyShoot;
    ableem::Sound explosion;
    ableem::Sound playerHit;
    ableem::Sound waveClear;
    ableem::Sound powerup;
};

//******************
// PowerUpType
//******************
enum class PowerUpType { None, Rapid, Spread, Power, ExtraLife };

//******************
// SurpriseGame
//******************
class SurpriseGame {
public:
    // set once by the owning screen before the first update(), same as StarFx::renderer
    SurpriseSounds sounds;

    void reset(unsigned int nowTicks);

    // moveLeft/moveRight/fireHeld are the current dpad/Cross state, sampled every frame (like the rest of
    // the app's screens); firing itself is cooldown-gated internally (and auto-fires under the Rapid
    // power-up even when fireHeld is false).
    void update(unsigned int nowTicks, bool moveLeft, bool moveRight, bool fireHeld);

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
        float vx = 0;          // horizontal drift per frame, for the spread shot's angled bolts
        int pierceLeft = 0;    // extra aliens this bolt can pass through after its first hit (the Power shot)
        bool alive = false;
    };

    struct Alien {
        float baseX = 0, baseY = 0;   // formation slot (baseY is also this row's sway/rest position)
        float x = 0, y = 0;           // current position
        int row = 0;
        bool alive = true;
        bool diving = false;          // swooping down at the ship along the bezier dive curve
        bool returning = false;       // looping back in from off the top, into the formation slot
        float diveT = 0;              // 0..1 progress through the dive curve
        float diveStartX = 0, diveStartY = 0;
        float diveTargetX = 0;
        float returnT = 0;            // 0..1 progress of the return/entrance flight
        float returnStartX = 0, returnStartY = 0;
        unsigned int entranceDelayUntil = 0;   // holds position until this tick, then the return flight starts
        int kind = 0;                 // 0/1 -> enemy1/enemy2 sprite while in formation
    };

    struct PowerUp {
        float x = 0, y = 0;
        PowerUpType type = PowerUpType::None;
        bool alive = false;
    };

    std::vector<Alien> aliens;
    std::vector<Bullet> playerBullets;
    std::vector<Bullet> alienBullets;
    std::vector<PowerUp> powerUps;

    float shipX = 0;
    int dropsSinceExtraLife = 0;   // power-ups dropped since the last extra life (see maybeDropPowerUp)
    int lives = 3;
    int score = 0;
    int highScore = 0;
    int wave = 1;

    void bumpScore(int delta) {
        score += delta;
        if (score > highScore) highScore = score;
    }

    // the formation's continuous Warblade-style weave: each row sways on its own sine phase and the whole
    // wave slowly creeps downward (capped) rather than bouncing off the screen edges as a rigid block
    float formationY = 0;
    float waveSpeedScale = 1.0f;   // faster sway/descent/dives on later waves

    PowerUpType activePowerUp = PowerUpType::None;
    unsigned int powerUpUntilTicks = 0;

    unsigned int lastTicks = 0;
    unsigned int lastShotTicks = 0;
    unsigned int hitInvulnUntil = 0;
    unsigned int nextDiveAtTicks = 0;

    // losing a life clears every laser on screen and freezes all game logic for a couple of seconds (with
    // a "LIFE LOST" banner) before play resumes - totalFrozenMs keeps the alien formation's sine sway from
    // jumping by the frozen duration in a single frame once play resumes
    unsigned int freezeUntilTicks = 0;
    unsigned int totalFrozenMs = 0;

    std::mt19937 rng{std::random_device{}()};

    void spawnWave(unsigned int nowTicks);
    void maybeStartDive(unsigned int nowTicks);
    void updateAliens(float dtFrames, unsigned int nowTicks);
    void updateBullets(float dtFrames);
    void updatePowerUps(float dtFrames, unsigned int nowTicks);
    void handleCollisions(unsigned int nowTicks);
    void tryFire(unsigned int nowTicks);
    void maybeDropPowerUp(float x, float y);
    int awayFromFormationCount() const;   // aliens currently diving or returning
    float restX(const Alien &a, unsigned int nowTicks) const;
    float restY(const Alien &a) const;
};
