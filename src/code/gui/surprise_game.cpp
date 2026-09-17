//
// Created for the "Surprise" easter egg on the About screen.
//

#include "surprise_game.h"
#include "gui.h"
#include "text_renderer.h"

#include <algorithm>
#include <cmath>
#include <string>

using namespace std;

namespace {
    const int ShipW = 56, ShipH = 42;
    const int AlienW = 48, AlienH = 42;
    const int LaserW = 8, LaserH = 32;
    const int PowerUpSize = 30;

    const int Rows = 4, Cols = 7;
    const int ColSpacing = 68, RowSpacing = 56;
    const int TopMargin = 100;

    const float PlayerBulletSpeed = 9.0f;   // px per 16ms frame
    const float AlienBulletSpeed = 4.0f;
    const float PowerUpFallSpeed = 2.2f;
    const unsigned int FireCooldownMs = 220;
    const unsigned int HitInvulnMs = 5000;   // 5s - long enough to recover after a hit
    const int MaxPlayerBullets = 6;          // generous enough for a 3-bolt Spread volley plus a bit more
    const float FormationMaxY = 220.0f;      // never lets the wave crawl down onto the ship

    const float DiveDurationFrames = 90.0f;    // ~1.5s at 60fps: swoop from formation to off the bottom
    const float ReturnDurationFrames = 60.0f;  // ~1s: fly back in, either after a dive or at wave start

    // the Warblade-style continuous weave: each row sways on its own sine phase, so the wave ripples as a
    // whole instead of marching and bouncing as a rigid block
    const float SwayAmplitude = 46.0f;
    const float SwayAngularSpeed = 1.0f;   // rad/s at wave 1
    const float RowPhaseStep = 0.7f;       // radians of phase offset between adjacent rows
    const float FormationDriftSpeed = 0.05f;   // px per 16ms frame, downward, scaled by waveSpeedScale
    const float EnemySpeedStepPerFiveWaves = 0.15f;   // dives, shots and the wave itself, every 5th wave

    const unsigned int EntranceStaggerMs = 180;   // extra entrance delay per row
    const unsigned int EntranceColStaggerMs = 40; // extra entrance delay per column, for a diagonal cascade

    const int PowerUpDropPercent = 20;   // chance an exploded alien drops a timed power-up (rapid/spread/power)
    const int ExtraLifeDropPercent = 2;  // separate chance it drops an extra life instead (10 was a life a wave)
    const int ExtraLifeEveryNthDrop = 50;   // ...and whatever the dice say, the Nth drop since the last one is a life
    const float ExtraLifeFallScale = 0.6f;  // a life falls slower than the timed power-ups, so it can be caught
    const unsigned int PowerUpDurationMs = 10000;

    const unsigned int LifeLostFreezeMs = 2000;

    bool overlaps(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh) {
        return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
    }
}

//*******************************
// SurpriseGame::reset
//*******************************
void SurpriseGame::reset(unsigned int nowTicks) {
    shipX = SCREEN_WIDTH / 2.0f - ShipW / 2.0f;
    lives = 3;
    score = 0;
    wave = 1;
    activePowerUp = PowerUpType::None;
    powerUpUntilTicks = 0;
    dropsSinceExtraLife = 0;
    playerBullets.clear();
    alienBullets.clear();
    powerUps.clear();
    lastTicks = nowTicks;
    lastShotTicks = 0;
    hitInvulnUntil = 0;
    nextDiveAtTicks = nowTicks + 2500;
    freezeUntilTicks = 0;
    totalFrozenMs = 0;
    spawnWave(nowTicks);
}

//*******************************
// SurpriseGame::spawnWave
//*******************************
void SurpriseGame::spawnWave(unsigned int nowTicks) {
    aliens.clear();
    int gridWidth = (Cols - 1) * ColSpacing + AlienW;
    float startX = (SCREEN_WIDTH - gridWidth) / 2.0f;

    for (int r = 0; r < Rows; r++) {
        for (int c = 0; c < Cols; c++) {
            Alien a;
            a.baseX = startX + c * ColSpacing;
            a.baseY = TopMargin + r * RowSpacing;
            a.row = r;
            a.kind = r % 2;
            a.returning = true;
            a.returnT = 0;
            a.returnStartX = a.baseX;
            a.returnStartY = -(float) AlienH - r * 30.0f;
            a.entranceDelayUntil = nowTicks + r * EntranceStaggerMs + c * EntranceColStaggerMs;
            a.x = a.returnStartX;
            a.y = a.returnStartY;
            aliens.push_back(a);
        }
    }
    formationY = 0;
    // 12% quicker each wave, and a further step every fifth wave (5, 10, 15, ...) - the step is what makes
    // the dives and the shots faster too, which the per-wave ramp deliberately leaves alone
    waveSpeedScale = 1.0f + (wave - 1) * 0.12f;
    enemySpeedScale = 1.0f + ((wave - 1) / 5) * EnemySpeedStepPerFiveWaves;
    waveSpeedScale *= enemySpeedScale;
    playerBullets.clear();
    alienBullets.clear();
}

//*******************************
// SurpriseGame::tryFire
//*******************************
void SurpriseGame::tryFire(unsigned int nowTicks) {
    if (gameOver()) return;

    unsigned int cooldown = (activePowerUp == PowerUpType::Rapid) ? FireCooldownMs / 3 : FireCooldownMs;
    if (nowTicks - lastShotTicks < cooldown) return;

    int aliveCount = 0;
    for (const Bullet &b : playerBullets) if (b.alive) aliveCount++;
    if (aliveCount >= MaxPlayerBullets) return;

    float startX = shipX + ShipW / 2.0f - LaserW / 2.0f;
    float startY = SCREEN_HEIGHT - 90.0f - LaserH;

    if (activePowerUp == PowerUpType::Spread) {
        const float driftAngles[3] = {-2.2f, 0.0f, 2.2f};
        for (float vx : driftAngles) {
            Bullet b;
            b.x = startX;
            b.y = startY;
            b.vx = vx;
            b.alive = true;
            playerBullets.push_back(b);
        }
    } else if (activePowerUp == PowerUpType::Power) {
        Bullet b;
        b.x = startX;
        b.y = startY;
        b.pierceLeft = 2;   // this bolt can pass through up to 3 aliens total
        b.alive = true;
        playerBullets.push_back(b);
    } else {
        Bullet b;
        b.x = startX;
        b.y = startY;
        b.alive = true;
        playerBullets.push_back(b);
    }

    lastShotTicks = nowTicks;
    sounds.playerShoot.play();
}

//*******************************
// SurpriseGame::awayFromFormationCount
//*******************************
int SurpriseGame::awayFromFormationCount() const {
    int n = 0;
    for (const Alien &a : aliens) if (a.alive && (a.diving || a.returning)) n++;
    return n;
}

//*******************************
// SurpriseGame::restX / restY
// the alien's current "at rest" position: baseY plus the wave's slow downward creep for Y, baseX plus this
// row's own sine sway for X - this is what gives the formation its continuous Warblade-style ripple.
//*******************************
float SurpriseGame::restX(const Alien &a, unsigned int nowTicks) const {
    // frozen time doesn't count towards the sway phase, or the whole formation would visibly jump ahead by
    // the frozen duration the instant a "life lost" freeze ends
    float t = (nowTicks - totalFrozenMs) / 1000.0f;
    return a.baseX + SwayAmplitude * sinf(SwayAngularSpeed * waveSpeedScale * t + a.row * RowPhaseStep);
}

float SurpriseGame::restY(const Alien &a) const {
    return a.baseY + formationY;
}

//*******************************
// SurpriseGame::maybeStartDive
//*******************************
void SurpriseGame::maybeStartDive(unsigned int nowTicks) {
    if (gameOver()) return;
    if (nowTicks < nextDiveAtTicks) return;
    if (awayFromFormationCount() >= 2) return;

    vector<int> candidates;
    for (size_t i = 0; i < aliens.size(); i++) {
        if (aliens[i].alive && !aliens[i].diving && !aliens[i].returning) candidates.push_back((int) i);
    }
    if (candidates.empty()) return;

    uniform_int_distribution<int> pick(0, (int) candidates.size() - 1);
    Alien &a = aliens[candidates[pick(rng)]];
    a.diving = true;
    a.diveT = 0;
    a.diveStartX = a.x;
    a.diveStartY = a.y;
    a.diveTargetX = shipX + ShipW / 2.0f;

    uniform_int_distribution<int> nextDelay(1400, 3200);
    nextDiveAtTicks = nowTicks + (unsigned int) (nextDelay(rng) / waveSpeedScale);
}

//*******************************
// SurpriseGame::updateAliens
//*******************************
void SurpriseGame::updateAliens(float dtFrames, unsigned int nowTicks) {
    bool anyResting = false;
    for (const Alien &a : aliens) {
        if (a.alive && !a.diving && !a.returning) { anyResting = true; break; }
    }
    if (anyResting && formationY < FormationMaxY) {
        formationY += FormationDriftSpeed * waveSpeedScale * dtFrames;
        if (formationY > FormationMaxY) formationY = FormationMaxY;
    }

    for (Alien &a : aliens) {
        if (!a.alive) continue;

        if (a.diving) {
            a.diveT += dtFrames * enemySpeedScale / DiveDurationFrames;
            if (a.diveT >= 1.0f) {
                // off the bottom of the screen now - loop back in from the top instead of teleporting
                // straight back into the formation slot, so it reads as "flying back", not "reappearing"
                a.diving = false;
                a.returning = true;
                a.returnT = 0;
                a.returnStartX = a.baseX;
                a.returnStartY = -(float) AlienH;
                a.entranceDelayUntil = 0;
                a.x = a.returnStartX;
                a.y = a.returnStartY;
                continue;
            }
            float t = a.diveT;
            float u = 1.0f - t;
            float endY = SCREEN_HEIGHT + 40.0f;
            float ctrlY = SCREEN_HEIGHT * 0.55f;
            // quadratic bezier: start -> (target x, mid height) -> (target x, off the bottom of the screen)
            a.x = u * u * a.diveStartX + 2 * u * t * a.diveTargetX + t * t * a.diveTargetX;
            a.y = u * u * a.diveStartY + 2 * u * t * ctrlY + t * t * endY;
            continue;
        }

        if (a.returning) {
            if (nowTicks < a.entranceDelayUntil) {
                a.x = a.returnStartX;
                a.y = a.returnStartY;
                continue;   // still waiting for its turn in the cascading entrance
            }
            a.returnT += dtFrames / ReturnDurationFrames;
            float targetX = restX(a, nowTicks);
            float targetY = restY(a);
            if (a.returnT >= 1.0f) {
                a.returning = false;
                a.x = targetX;
                a.y = targetY;
                continue;
            }
            a.x = a.returnStartX + (targetX - a.returnStartX) * a.returnT;
            a.y = a.returnStartY + (targetY - a.returnStartY) * a.returnT;
            continue;
        }

        a.x = restX(a, nowTicks);
        a.y = restY(a);
    }

    maybeStartDive(nowTicks);

    // an alien firing down at the ship: only the bottom-most alive alien in each column may fire, so shots
    // always look like they come from the front rank
    if (!gameOver()) {
        uniform_int_distribution<int> chance(0, 219);
        for (size_t i = 0; i < aliens.size(); i++) {
            Alien &a = aliens[i];
            if (!a.alive || a.diving || a.returning) continue;
            bool blocked = false;
            for (const Alien &other : aliens) {
                if (&other == &a || !other.alive) continue;
                if (other.baseX == a.baseX && other.baseY > a.baseY) { blocked = true; break; }
            }
            if (blocked) continue;
            if (chance(rng) != 0) continue;

            Bullet b;
            b.x = a.x + AlienW / 2.0f - LaserW / 2.0f;
            b.y = a.y + AlienH;
            b.alive = true;
            alienBullets.push_back(b);
            sounds.enemyShoot.play();
        }
    }
}

//*******************************
// SurpriseGame::updateBullets
//*******************************
void SurpriseGame::updateBullets(float dtFrames) {
    for (Bullet &b : playerBullets) {
        if (!b.alive) continue;
        b.y -= PlayerBulletSpeed * dtFrames;
        b.x += b.vx * dtFrames;
        if (b.y < -LaserH || b.x < -LaserW || b.x > SCREEN_WIDTH) b.alive = false;
    }
    for (Bullet &b : alienBullets) {
        if (!b.alive) continue;
        b.y += AlienBulletSpeed * enemySpeedScale * dtFrames;
        if (b.y > SCREEN_HEIGHT) b.alive = false;
    }
    playerBullets.erase(remove_if(playerBullets.begin(), playerBullets.end(),
                                   [](const Bullet &b) { return !b.alive; }), playerBullets.end());
    alienBullets.erase(remove_if(alienBullets.begin(), alienBullets.end(),
                                  [](const Bullet &b) { return !b.alive; }), alienBullets.end());
}

//*******************************
// SurpriseGame::maybeDropPowerUp
//*******************************
void SurpriseGame::maybeDropPowerUp(float x, float y) {
    // one roll picks between an extra life (ExtraLifeDropPercent), one of the three timed power-ups
    // (PowerUpDropPercent), or nothing - so the two chances don't stack into two pickups on one kill.
    uniform_int_distribution<int> chance(0, 99);
    int roll = chance(rng);

    PowerUpType type;
    if (roll < ExtraLifeDropPercent) {
        type = PowerUpType::ExtraLife;
    } else if (roll < ExtraLifeDropPercent + PowerUpDropPercent) {
        uniform_int_distribution<int> pick(0, 2);
        static const PowerUpType types[3] = {PowerUpType::Rapid, PowerUpType::Spread, PowerUpType::Power};
        type = types[pick(rng)];
    } else {
        return;
    }

    // a bad streak of dice must not go on forever: every ExtraLifeEveryNthDrop-th drop is a life regardless
    if (type != PowerUpType::ExtraLife && ++dropsSinceExtraLife >= ExtraLifeEveryNthDrop) {
        type = PowerUpType::ExtraLife;
    }
    if (type == PowerUpType::ExtraLife) {
        dropsSinceExtraLife = 0;
    }

    PowerUp p;
    p.x = x;
    p.y = y;
    p.type = type;
    p.alive = true;
    powerUps.push_back(p);
}

//*******************************
// SurpriseGame::updatePowerUps
//*******************************
void SurpriseGame::updatePowerUps(float dtFrames, unsigned int nowTicks) {
    float shipY = SCREEN_HEIGHT - 90.0f;
    for (PowerUp &p : powerUps) {
        if (!p.alive) continue;
        p.y += PowerUpFallSpeed * (p.type == PowerUpType::ExtraLife ? ExtraLifeFallScale : 1.0f) * dtFrames;
        if (p.y > SCREEN_HEIGHT) {
            p.alive = false;
            continue;
        }
        if (!gameOver() && overlaps(p.x, p.y, PowerUpSize, PowerUpSize, shipX, shipY, ShipW, ShipH)) {
            if (p.type == PowerUpType::ExtraLife) {
                lives++;
            } else {
                activePowerUp = p.type;
                powerUpUntilTicks = nowTicks + PowerUpDurationMs;
            }
            p.alive = false;
            sounds.powerup.play();
        }
    }
    powerUps.erase(remove_if(powerUps.begin(), powerUps.end(),
                              [](const PowerUp &p) { return !p.alive; }), powerUps.end());

    if (activePowerUp != PowerUpType::None && nowTicks >= powerUpUntilTicks) {
        activePowerUp = PowerUpType::None;
    }
}

//*******************************
// SurpriseGame::handleCollisions
//*******************************
void SurpriseGame::handleCollisions(unsigned int nowTicks) {
    for (Bullet &b : playerBullets) {
        if (!b.alive) continue;
        for (Alien &a : aliens) {
            if (!a.alive) continue;
            if (overlaps(b.x, b.y, LaserW, LaserH, a.x, a.y, AlienW, AlienH)) {
                a.alive = false;
                bumpScore(a.diving ? 150 : 100);
                sounds.explosion.play();
                maybeDropPowerUp(a.x + AlienW / 2.0f - PowerUpSize / 2.0f, a.y);
                if (b.pierceLeft > 0) {
                    b.pierceLeft--;
                } else {
                    b.alive = false;
                }
                break;
            }
        }
    }

    float shipY = SCREEN_HEIGHT - 90.0f;
    bool invulnerable = nowTicks < hitInvulnUntil;

    if (!invulnerable && !gameOver()) {
        // at most one hit lands per frame - an alien bullet and a diving alien could otherwise both overlap
        // the ship on the same frame and take two lives before hitInvulnUntil has a chance to guard the second
        bool hitThisFrame = false;
        for (Bullet &b : alienBullets) {
            if (!b.alive) continue;
            if (overlaps(b.x, b.y, LaserW, LaserH, shipX, shipY, ShipW, ShipH)) {
                b.alive = false;
                lives--;
                hitInvulnUntil = nowTicks + HitInvulnMs;
                sounds.playerHit.play();
                hitThisFrame = true;
                break;
            }
        }
        for (Alien &a : aliens) {
            if (hitThisFrame) break;
            if (!a.alive || !a.diving) continue;
            if (overlaps(a.x, a.y, AlienW, AlienH, shipX, shipY, ShipW, ShipH)) {
                a.alive = false;
                lives--;
                hitInvulnUntil = nowTicks + HitInvulnMs;
                sounds.playerHit.play();
                hitThisFrame = true;
                break;
            }
        }

        if (hitThisFrame) {
            // clear every laser on screen and freeze play for a couple of seconds, with a "LIFE LOST" banner
            playerBullets.clear();
            alienBullets.clear();
            freezeUntilTicks = nowTicks + LifeLostFreezeMs;
            totalFrozenMs += LifeLostFreezeMs;
        }
    }

    bool anyAlive = false;
    for (const Alien &a : aliens) if (a.alive) { anyAlive = true; break; }
    if (!anyAlive && !gameOver()) {
        wave++;
        bumpScore(500);
        sounds.waveClear.play();
        spawnWave(nowTicks);
    }
}

//*******************************
// SurpriseGame::update
//*******************************
void SurpriseGame::update(unsigned int nowTicks, bool moveLeft, bool moveRight, bool fireHeld) {
    unsigned int dt = (lastTicks == 0) ? 16 : (nowTicks - lastTicks);
    if (dt > 200) dt = 200;
    lastTicks = nowTicks;
    float dtFrames = dt / 16.0f;

    if (gameOver()) return;
    if (nowTicks < freezeUntilTicks) return;   // "life lost" hit-stun: hold everything in place

    const float shipSpeed = 7.0f;
    if (moveLeft) shipX -= shipSpeed * dtFrames;
    if (moveRight) shipX += shipSpeed * dtFrames;
    shipX = max(10.0f, min((float) SCREEN_WIDTH - 10.0f - ShipW, shipX));

    updateAliens(dtFrames, nowTicks);
    updateBullets(dtFrames);
    updatePowerUps(dtFrames, nowTicks);
    handleCollisions(nowTicks);

    bool autofiring = (activePowerUp == PowerUpType::Rapid);
    if (fireHeld || autofiring) tryFire(nowTicks);
}

//*******************************
// SurpriseGame::render
//*******************************
void SurpriseGame::render(ableem::Renderer &renderer, TextRenderer &text, const ableem::Font &font,
                           const SurpriseSprites &sprites) {
    float shipY = SCREEN_HEIGHT - 90.0f;

    for (const Alien &a : aliens) {
        if (!a.alive) continue;
        const ableem::Texture &tex = a.diving ? sprites.ufo : (a.kind == 0 ? sprites.enemy1 : sprites.enemy2);
        ableem::Rect dst((int) a.x, (int) a.y, AlienW, AlienH);
        renderer.copy(tex, nullptr, &dst);
    }

    for (const PowerUp &p : powerUps) {
        if (!p.alive) continue;
        const ableem::Texture &tex = p.type == PowerUpType::Rapid ? sprites.powerupRapid
                                    : p.type == PowerUpType::Spread ? sprites.powerupSpread
                                    : p.type == PowerUpType::Power ? sprites.powerupPower
                                                                    : sprites.ship;   // ExtraLife: 1UP
        ableem::Rect dst((int) p.x, (int) p.y, PowerUpSize, PowerUpSize);
        renderer.copy(tex, nullptr, &dst);
        if (p.type == PowerUpType::ExtraLife) {
            // the ship sprite alone reads as "another ship"; say what it is
            text.renderText(font, "1UP", (int) p.x + PowerUpSize + 4, (int) p.y + 4, XALIGN_LEFT);
        }
    }

    for (const Bullet &b : playerBullets) {
        if (!b.alive) continue;
        const ableem::Texture &tex = b.pierceLeft > 0 ? sprites.laserEnemy : sprites.laserPlayer;
        ableem::Rect dst((int) b.x, (int) b.y, LaserW, LaserH);
        renderer.copy(tex, nullptr, &dst);
    }
    for (const Bullet &b : alienBullets) {
        if (!b.alive) continue;
        ableem::Rect dst((int) b.x, (int) b.y, LaserW, LaserH);
        renderer.copy(sprites.laserEnemy, nullptr, &dst);
    }

    // blink the ship while briefly invulnerable after a hit, instead of drawing it solid
    bool blinkHidden = lastTicks < hitInvulnUntil && ((lastTicks / 100) % 2 == 0);
    if (!gameOver() && !blinkHidden) {
        ableem::Rect dst((int) shipX, (int) shipY, ShipW, ShipH);
        renderer.copy(sprites.ship, nullptr, &dst);
    }

    text.renderText(font, _("SCORE") + ": " + to_string(score), 30, 20, XALIGN_LEFT);
    text.renderText(font, _("WAVE") + " " + to_string(wave), 0, 20, XALIGN_CENTER);
    text.renderText(font, _("LIVES") + ": " + to_string(max(0, lives)), 30, 20, XALIGN_RIGHT);
    text.renderText(font, _("HIGH SCORE") + ": " + to_string(highScore), 0, 46, XALIGN_CENTER);

    if (activePowerUp != PowerUpType::None) {
        string name = activePowerUp == PowerUpType::Rapid ? _("RAPID FIRE")
                    : activePowerUp == PowerUpType::Spread ? _("SPREAD SHOT")
                                                            : _("POWER SHOT");
        unsigned int remainingMs = (powerUpUntilTicks > lastTicks) ? (powerUpUntilTicks - lastTicks) : 0;
        text.renderText(font, name + " " + to_string(remainingMs / 1000 + 1) + "s", 0, 70, XALIGN_CENTER);
    }

    if (!gameOver() && lastTicks < freezeUntilTicks) {
        text.renderText(font, _("LIFE LOST"), 0, SCREEN_HEIGHT / 2 - 20, XALIGN_CENTER);
    }

    if (gameOver()) {
        text.renderText(font, _("GAME OVER"), 0, SCREEN_HEIGHT / 2 - 20, XALIGN_CENTER);
        text.renderText(font, _("Final score") + ": " + to_string(score), 0, SCREEN_HEIGHT / 2 + 10, XALIGN_CENTER);
    }
}
