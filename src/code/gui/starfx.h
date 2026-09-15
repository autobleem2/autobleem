//
// Created by screemer on 2019-07-31.
//

#pragma once

#include <random>
#include <vector>
#include <ableem/ui/renderer.h>

//******************
// StarFx
//******************
// A parallax starfield with twinkling and the occasional shooting star, used as the background of the
// About screen. render() is driven by a tick count (ableem::Platform::ticks()) so motion stays smooth and
// frame-rate independent, rather than advancing a fixed amount per call.
//
// Stars are batched by (depth tier, color tint, twinkle brightness level) and drawn with Renderer::fillRects
// - a handful of draw calls for the whole field instead of one per star, which matters on the PSC's weak GPU.
class StarFx {
public:
    StarFx();
    void render(unsigned int nowTicks);
    ableem::Renderer *renderer = nullptr;   // set by the owning screen before the first render()

private:
    //******************
    // Star
    //******************
    struct Star {
        float x = 0, y = 0;
        float speed = 0;                            // px per 16ms "frame"
        float size = 0;                              // px (square side)
        float driftPhase = 0, driftSpeed = 0, driftAmount = 0;   // gentle side-to-side sway
        float twinklePhase = 0, twinkleSpeed = 0;    // brightness pulsing
        unsigned char tier = 0;   // depth bucket: 0 (far/dim/slow/small) .. N-1 (near/bright/fast/big)
        unsigned char tint = 0;   // color bucket: white / cool / warm
    };

    //******************
    // Comet
    //******************
    // a rare, fast diagonal streak with a fading additive-blended trail
    struct Comet {
        bool active = false;
        float x = 0, y = 0, vx = 0, vy = 0;
        float life = 0, maxLife = 1;
    };

    std::vector<Star> stars;
    std::vector<std::vector<ableem::Rect>> buckets;   // reused every frame; see the .cpp for the indexing
    Comet comet;
    unsigned int lastTicks = 0;
    std::mt19937 rng{std::random_device{}()};

    void maybeSpawnComet();
    void renderComet(float dtFrames);
};
