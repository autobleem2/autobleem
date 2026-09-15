//
// Created by screemer on 2019-07-31.
//

#include <cmath>
#include "starfx.h"
#include "../gui/gui.h"

namespace {

//******************
// depth tiers
//******************
// far stars are dim, tiny, slow and by far the most numerous; near stars are bright, big, fast and rare -
// the classic layered-parallax look, but with continuous per-star jitter within each tier (see StarFx::StarFx)
// rather than every star in a tier being identical.
struct Tier {
    float speed;       // base px per 16ms frame
    float size;        // base px
    float brightness;  // 0..1, multiplies the tint at full twinkle
    int count;
};

const Tier TIERS[] = {
        {0.55f, 1.0f, 0.45f, 90},
        {1.10f, 1.4f, 0.55f, 70},
        {1.90f, 1.8f, 0.70f, 55},
        {3.00f, 2.4f, 0.85f, 45},
        {4.40f, 3.2f, 1.00f, 40},
};
const int TIER_COUNT = sizeof(TIERS) / sizeof(TIERS[0]);

//******************
// color tints
//******************
struct Tint {
    unsigned char r, g, b;
    int weight;   // relative odds of a star picking this tint; weights need not sum to 100
};

const Tint TINTS[] = {
        {255, 255, 255, 70},   // white - most stars
        {170, 200, 255, 18},   // cool blue
        {255, 220, 170, 12},   // warm amber
};
const int TINT_COUNT = sizeof(TINTS) / sizeof(TINTS[0]);

const int TWINKLE_LEVELS = 6;   // brightness steps twinkle is quantized to, for batching

int pickTint(std::mt19937 &rng) {
    int totalWeight = 0;
    for (const Tint &t : TINTS) totalWeight += t.weight;

    std::uniform_int_distribution<int> pick(0, totalWeight - 1);
    int roll = pick(rng);
    for (int i = 0; i < TINT_COUNT; i++) {
        roll -= TINTS[i].weight;
        if (roll < 0) return i;
    }
    return TINT_COUNT - 1;
}

int bucketIndex(int tier, int tint, int level) {
    return (tier * TINT_COUNT + tint) * TWINKLE_LEVELS + level;
}

} // namespace

//*******************************
// StarFx::StarFx
//*******************************
StarFx::StarFx() {
    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    std::uniform_real_distribution<float> twoPi(0.0f, 6.2831853f);

    for (int t = 0; t < TIER_COUNT; t++) {
        const Tier &tier = TIERS[t];
        for (int i = 0; i < tier.count; i++) {
            Star s;
            s.x = unit(rng) * SCREEN_WIDTH;
            s.y = unit(rng) * SCREEN_HEIGHT;
            s.speed = tier.speed * (0.85f + unit(rng) * 0.3f);      // +/-15% jitter so a tier isn't lockstep
            s.size = tier.size * (0.85f + unit(rng) * 0.3f);
            s.driftPhase = twoPi(rng);
            s.driftSpeed = 0.01f + unit(rng) * 0.03f;
            s.driftAmount = 0.5f + unit(rng) * (1.3f + t * 0.4f);   // nearer stars sway a bit more
            s.twinklePhase = twoPi(rng);
            s.twinkleSpeed = 0.02f + unit(rng) * 0.08f;
            s.tier = (unsigned char) t;
            s.tint = (unsigned char) pickTint(rng);

            stars.push_back(s);
        }
    }

    buckets.resize(TIER_COUNT * TINT_COUNT * TWINKLE_LEVELS);
}

//*******************************
// StarFx::maybeSpawnComet
//*******************************
void StarFx::maybeSpawnComet() {
    if (comet.active) return;

    // tried once per frame; ~1 in 800 succeeds, so a comet crosses roughly every 10-15 seconds at 60fps
    std::uniform_int_distribution<int> chance(0, 799);
    if (chance(rng) != 0) return;

    std::uniform_real_distribution<float> unit(0.0f, 1.0f);
    comet.x = unit(rng) * SCREEN_WIDTH * 0.6f;
    comet.y = -20.0f - unit(rng) * (SCREEN_HEIGHT * 0.25f);
    float angle = 0.35f + unit(rng) * 0.35f;   // roughly 20-40 degrees below horizontal
    float speed = 14.0f + unit(rng) * 10.0f;   // px per 16ms frame
    comet.vx = speed * cosf(angle);
    comet.vy = speed * sinf(angle);
    comet.life = 0.0f;
    comet.maxLife = 24.0f + unit(rng) * 10.0f;
    comet.active = true;
}

//*******************************
// StarFx::renderComet
//*******************************
void StarFx::renderComet(float dtFrames) {
    if (!comet.active) return;

    comet.x += comet.vx * dtFrames;
    comet.y += comet.vy * dtFrames;
    comet.life += dtFrames;

    if (comet.life >= comet.maxLife || comet.x > SCREEN_WIDTH + 40 || comet.y > SCREEN_HEIGHT + 40) {
        comet.active = false;
        return;
    }

    float fade = 1.0f - (comet.life / comet.maxLife);
    renderer->setBlendMode(ableem::BlendMode::Add);   // additive glow for the trail and head

    const int trailSteps = 6;
    for (int i = 0; i < trailSteps; i++) {
        float back = (float) i;
        ableem::Point a{(int) (comet.x - comet.vx * back), (int) (comet.y - comet.vy * back)};
        ableem::Point b{(int) (comet.x - comet.vx * (back + 1)), (int) (comet.y - comet.vy * (back + 1))};

        float segFade = fade * (1.0f - (float) i / trailSteps);
        unsigned char c = (unsigned char) (200.0f * segFade);
        unsigned char cb = (unsigned char) (255.0f * segFade);
        renderer->setDrawColor(ableem::Color(c, c, cb, 255));
        renderer->drawLine(a, b);
    }

    unsigned char headC = (unsigned char) (255.0f * fade);
    renderer->setDrawColor(ableem::Color(headC, headC, headC, 255));
    renderer->fillRect(ableem::Rect((int) comet.x - 1, (int) comet.y - 1, 3, 3));

    renderer->setBlendMode(ableem::BlendMode::Blend);   // restore what the caller had set for the stars/overlay
}

//*******************************
// StarFx::render
//*******************************
void StarFx::render(unsigned int nowTicks) {
    // frame-rate independent motion: dt is normalized to "frames" of 16ms (~60fps), clamped so a stall
    // (window drag, first frame) doesn't fling every star across the screen in one jump
    unsigned int dt = (lastTicks == 0) ? 16 : (nowTicks - lastTicks);
    if (dt > 200) dt = 200;
    lastTicks = nowTicks;
    float dtFrames = dt / 16.0f;

    for (auto &bucket : buckets) bucket.clear();

    std::uniform_real_distribution<float> unit(0.0f, 1.0f);

    for (Star &s : stars) {
        s.y += s.speed * dtFrames;
        if (s.y > SCREEN_HEIGHT + s.size) {
            s.y -= (SCREEN_HEIGHT + s.size);
            s.x = unit(rng) * SCREEN_WIDTH;   // fresh horizontal position each time it recycles to the top
        }
        s.driftPhase += s.driftSpeed * dtFrames;
        s.twinklePhase += s.twinkleSpeed * dtFrames;

        float twinkle = 0.55f + 0.45f * (0.5f + 0.5f * sinf(s.twinklePhase));   // 0.55 .. 1.0
        int level = (int) (twinkle * (TWINKLE_LEVELS - 1) + 0.5f);
        if (level < 0) level = 0;
        if (level >= TWINKLE_LEVELS) level = TWINKLE_LEVELS - 1;

        float drawX = s.x + sinf(s.driftPhase) * s.driftAmount;
        buckets[bucketIndex(s.tier, s.tint, level)].emplace_back(
                (int) drawX, (int) s.y, (int) s.size + 1, (int) s.size + 1);
    }

    for (int t = 0; t < TIER_COUNT; t++) {
        for (int ti = 0; ti < TINT_COUNT; ti++) {
            for (int lvl = 0; lvl < TWINKLE_LEVELS; lvl++) {
                std::vector<ableem::Rect> &rects = buckets[bucketIndex(t, ti, lvl)];
                if (rects.empty()) continue;

                float brightness = TIERS[t].brightness * (0.55f + 0.45f * lvl / (float) (TWINKLE_LEVELS - 1));
                const Tint &tint = TINTS[ti];
                renderer->setDrawColor(ableem::Color(
                        (unsigned char) (tint.r * brightness),
                        (unsigned char) (tint.g * brightness),
                        (unsigned char) (tint.b * brightness),
                        255));
                renderer->fillRects(rects.data(), (int) rects.size());
            }
        }
    }

    maybeSpawnComet();
    renderComet(dtFrames);
}
