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

    const int Rows = 4, Cols = 7;
    const int ColSpacing = 68, RowSpacing = 56;
    const int TopMargin = 100;

    const float PlayerBulletSpeed = 9.0f;   // px per 16ms frame
    const float AlienBulletSpeed = 4.0f;
    const unsigned int FireCooldownMs = 220;
    const unsigned int HitInvulnMs = 5000;   // 5s - long enough to recover after a hit
    const int MaxPlayerBullets = 3;
    const float FormationStepDown = 16.0f;
    const float FormationMaxY = 220.0f;   // never lets the marching grid crawl down onto the ship

    const float DiveDurationFrames = 90.0f;    // ~1.5s at 60fps: swoop from formation to off the bottom
    const float ReturnDurationFrames = 60.0f;  // ~1s: loop back in from off the top into the formation slot
}

//*******************************
// SurpriseGame::reset
//*******************************
void SurpriseGame::reset(unsigned int nowTicks) {
    shipX = SCREEN_WIDTH / 2.0f - ShipW / 2.0f;
    lives = 3;
    score = 0;
    wave = 1;
    playerBullets.clear();
    alienBullets.clear();
    lastTicks = nowTicks;
    lastShotTicks = 0;
    hitInvulnUntil = 0;
    nextDiveAtTicks = nowTicks + 1500;
    spawnWave();
}

//*******************************
// SurpriseGame::spawnWave
//*******************************
void SurpriseGame::spawnWave() {
    aliens.clear();
    int gridWidth = (Cols - 1) * ColSpacing + AlienW;
    float startX = (SCREEN_WIDTH - gridWidth) / 2.0f;

    for (int r = 0; r < Rows; r++) {
        for (int c = 0; c < Cols; c++) {
            Alien a;
            a.baseX = startX + c * ColSpacing;
            a.baseY = TopMargin + r * RowSpacing;
            a.x = a.baseX;
            a.y = a.baseY;
            a.kind = r % 2;
            aliens.push_back(a);
        }
    }
    formationDir = 1.0f;
    formationX = 0;
    formationY = 0;
    formationSpeed = 0.6f + (wave - 1) * 0.15f;
    playerBullets.clear();
    alienBullets.clear();
}

//*******************************
// SurpriseGame::fire
//*******************************
void SurpriseGame::fire(unsigned int nowTicks) {
    if (gameOver()) return;
    if (nowTicks - lastShotTicks < FireCooldownMs) return;

    int aliveCount = 0;
    for (const Bullet &b : playerBullets) if (b.alive) aliveCount++;
    if (aliveCount >= MaxPlayerBullets) return;

    Bullet b;
    b.x = shipX + ShipW / 2.0f - LaserW / 2.0f;
    b.y = SCREEN_HEIGHT - 90.0f - LaserH;
    b.alive = true;
    playerBullets.push_back(b);
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
    nextDiveAtTicks = nowTicks + nextDelay(rng);
}

//*******************************
// SurpriseGame::updateAliens
//*******************************
void SurpriseGame::updateAliens(float dtFrames, unsigned int nowTicks) {
    // formation march: bounce off the screen edges, step down on every bounce
    float minX = 1e9f, maxX = -1e9f;
    bool any = false;
    for (const Alien &a : aliens) {
        if (!a.alive || a.diving || a.returning) continue;
        any = true;
        minX = min(minX, a.baseX + formationX);
        maxX = max(maxX, a.baseX + formationX + AlienW);
    }
    if (any) {
        formationX += formationSpeed * formationDir * dtFrames;
        if (minX <= 12 && formationDir < 0) {
            formationDir = 1;
            if (formationY < FormationMaxY) formationY += FormationStepDown;
        } else if (maxX >= SCREEN_WIDTH - 12 && formationDir > 0) {
            formationDir = -1;
            if (formationY < FormationMaxY) formationY += FormationStepDown;
        }
    }

    for (Alien &a : aliens) {
        if (!a.alive) continue;

        if (a.diving) {
            a.diveT += dtFrames / DiveDurationFrames;
            if (a.diveT >= 1.0f) {
                // off the bottom of the screen now - loop back in from the top instead of teleporting
                // straight back into the formation slot, so it reads as "flying back", not "reappearing"
                a.diving = false;
                a.returning = true;
                a.returnT = 0;
                a.x = a.baseX + formationX;
                a.y = -(float) AlienH;
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
            a.returnT += dtFrames / ReturnDurationFrames;
            if (a.returnT >= 1.0f) {
                a.returning = false;
                a.x = a.baseX + formationX;
                a.y = a.baseY + formationY;
                continue;
            }
            a.x = a.baseX + formationX;
            a.y = -(float) AlienH + a.returnT * (a.baseY + formationY + (float) AlienH);
            continue;
        }

        a.x = a.baseX + formationX;
        a.y = a.baseY + formationY;
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
                if (fabs(other.baseX - a.baseX) < 4 && other.baseY > a.baseY) { blocked = true; break; }
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
        if (b.y < -LaserH) b.alive = false;
    }
    for (Bullet &b : alienBullets) {
        if (!b.alive) continue;
        b.y += AlienBulletSpeed * dtFrames;
        if (b.y > SCREEN_HEIGHT) b.alive = false;
    }
    playerBullets.erase(remove_if(playerBullets.begin(), playerBullets.end(),
                                   [](const Bullet &b) { return !b.alive; }), playerBullets.end());
    alienBullets.erase(remove_if(alienBullets.begin(), alienBullets.end(),
                                  [](const Bullet &b) { return !b.alive; }), alienBullets.end());
}

namespace {
    bool overlaps(float ax, float ay, float aw, float ah, float bx, float by, float bw, float bh) {
        return ax < bx + bw && ax + aw > bx && ay < by + bh && ay + ah > by;
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
                b.alive = false;
                a.alive = false;
                bumpScore(a.diving ? 150 : 100);
                sounds.explosion.play();
                break;
            }
        }
    }

    float shipY = SCREEN_HEIGHT - 90.0f;
    bool invulnerable = nowTicks < hitInvulnUntil;

    if (!invulnerable && !gameOver()) {
        for (Bullet &b : alienBullets) {
            if (!b.alive) continue;
            if (overlaps(b.x, b.y, LaserW, LaserH, shipX, shipY, ShipW, ShipH)) {
                b.alive = false;
                lives--;
                hitInvulnUntil = nowTicks + HitInvulnMs;
                sounds.playerHit.play();
                break;
            }
        }
        for (Alien &a : aliens) {
            if (!a.alive || !a.diving) continue;
            if (overlaps(a.x, a.y, AlienW, AlienH, shipX, shipY, ShipW, ShipH)) {
                a.alive = false;
                lives--;
                hitInvulnUntil = nowTicks + HitInvulnMs;
                sounds.playerHit.play();
                break;
            }
        }
    }

    bool anyAlive = false;
    for (const Alien &a : aliens) if (a.alive) { anyAlive = true; break; }
    if (!anyAlive && !gameOver()) {
        wave++;
        bumpScore(500);
        sounds.waveClear.play();
        spawnWave();
    }
}

//*******************************
// SurpriseGame::update
//*******************************
void SurpriseGame::update(unsigned int nowTicks, bool moveLeft, bool moveRight) {
    unsigned int dt = (lastTicks == 0) ? 16 : (nowTicks - lastTicks);
    if (dt > 200) dt = 200;
    lastTicks = nowTicks;
    float dtFrames = dt / 16.0f;

    if (gameOver()) return;

    const float shipSpeed = 7.0f;
    if (moveLeft) shipX -= shipSpeed * dtFrames;
    if (moveRight) shipX += shipSpeed * dtFrames;
    shipX = max(10.0f, min((float) SCREEN_WIDTH - 10.0f - ShipW, shipX));

    updateAliens(dtFrames, nowTicks);
    updateBullets(dtFrames);
    handleCollisions(nowTicks);
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

    for (const Bullet &b : playerBullets) {
        if (!b.alive) continue;
        ableem::Rect dst((int) b.x, (int) b.y, LaserW, LaserH);
        renderer.copy(sprites.laserPlayer, nullptr, &dst);
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

    if (gameOver()) {
        text.renderText(font, _("GAME OVER"), 0, SCREEN_HEIGHT / 2 - 20, XALIGN_CENTER);
        text.renderText(font, _("Final score") + ": " + to_string(score), 0, SCREEN_HEIGHT / 2 + 10, XALIGN_CENTER);
    }
}
