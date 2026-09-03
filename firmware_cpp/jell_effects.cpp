#include "jell_effects.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "pico/time.h"
#include "jell_config.hpp"

namespace
{
    // Seconds since this function was last called from the given timestamp slot.
    // Uses the 64-bit microsecond clock directly so the result stays exact after days
    // of uptime, and caps long gaps (e.g. right after a mode switch).
    float seconds_since_last_call(uint64_t& last_us)
    {
        const uint64_t now_us = time_us_64();
        float dt = (last_us == 0) ? 0.0f : (float)(now_us - last_us) * 1e-6f;
        last_us = now_us;
        return std::min(dt, 0.1f);
    }

    float smoothstep01(float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }

    constexpr float TWO_PI = 6.2831853f;

    float frand01()
    {
        return (float)rand() / (float)RAND_MAX;
    }

    float frand(float lo, float hi)
    {
        return lo + (hi - lo) * frand01();
    }

    float frac(float x)
    {
        return x - floorf(x);
    }

    // Raised cosine: 0 at p = 0, 1 at p = 0.5, 0 at p = 1.
    float bump(float p)
    {
        return 0.5f - 0.5f * cosf(TWO_PI * p);
    }

    // ---- sparks: single pixels that glow up and fade with their own envelope ----

    struct Spark
    {
        bool active = false;
        int spoke = -1; // -1 = on the ring
        int pixel = 0;
        float age_s = 0.0f;
        float life_s = 1.0f;
        float hue = 0.0f;
        float peak = 1.0f;
    };

    // Start a spark in a free slot of the pool, if there is one.
    void spawn_spark(Spark* pool, int count, float ring_share,
                     float life_lo, float life_hi, float hue_lo, float hue_hi, float peak_lo, float peak_hi)
    {
        for (int i = 0; i < count; i++)
        {
            Spark& k = pool[i];
            if (k.active)
                continue;
            k.active = true;
            if (frand01() < ring_share)
            {
                k.spoke = -1;
                k.pixel = rand() % JellConfig::NUMBER_LEDS_IN_RING;
            }
            else
            {
                k.spoke = rand() % JellConfig::NUMBER_OF_TENTACLES;
                k.pixel = rand() % JellConfig::NUMBER_LEDS_IN_EACH_TENTACLE;
            }
            k.age_s = 0.0f;
            k.life_s = frand(life_lo, life_hi);
            k.hue = frand(hue_lo, hue_hi);
            k.peak = frand(peak_lo, peak_hi);
            return;
        }
    }

    // Age every spark by dt and draw it over a background of brightness bg_v.
    void draw_sparks(Canvas& canvas, Spark* pool, int count, float dt, float (*envelope)(float),
                     float saturation, float bg_v)
    {
        for (int i = 0; i < count; i++)
        {
            Spark& k = pool[i];
            if (!k.active)
                continue;
            k.age_s += dt;
            if (k.age_s >= k.life_s)
            {
                k.active = false;
                continue;
            }
            const float v = std::max(bg_v, k.peak * envelope(k.age_s / k.life_s));
            if (k.spoke < 0)
                canvas.ring_pixel_hsv(k.pixel, k.hue, saturation, v);
            else
                canvas.spoke_pixel_hsv(k.spoke, k.pixel, k.hue, saturation, v);
        }
    }

    // ---- drops: one head per tentacle running downwards, the trail comes from Canvas::fade ----

    // Draw the heads and advance them by speed * dt. drop_pos < 0 means no drop on that tentacle.
    void advance_drops(Canvas& canvas, float* drop_pos, float speed, float dt, float h, float s, float v)
    {
        for (int t = 0; t < JellConfig::NUMBER_OF_TENTACLES; t++)
        {
            if (drop_pos[t] < 0.0f)
                continue;

            canvas.spoke_pixel_hsv(t, (int)drop_pos[t], h, s, v);

            drop_pos[t] += speed * dt;
            if (drop_pos[t] >= (float)JellConfig::NUMBER_LEDS_IN_EACH_TENTACLE)
                drop_pos[t] = -1.0f;
        }
    }

    void init_drops(float* drop_pos)
    {
        for (int t = 0; t < JellConfig::NUMBER_OF_TENTACLES; t++)
            drop_pos[t] = -1.0f;
    }
}

void effect_miclevelCheck(
    Canvas& canvas,
    const AudioFrame& audio)
{
    canvas.all_pixels_hsv(220.0f, 1.0f, audio.smoothed_level);
    canvas.all_noodles_level(audio.smoothed_level);
}


void effect_LEDchanneltest(Canvas& canvas)
{
    constexpr uint64_t STEP_US = 1000000; // one second per colour and per noodle

    static uint64_t last_step_us = 0;
    static int state = 0;

    const uint64_t now_us = time_us_64();
    if (now_us - last_step_us >= STEP_US)
    {
        state++;
        last_step_us = now_us;
    }

    if (state % 3 == 0)
    {
        canvas.all_pixels_hsv(0.0f, 1.0f, 1.0f);
    }
    else if (state % 3 == 1)
    {
        canvas.all_pixels_hsv(120.0f, 1.0f, 1.0f);
    }
    else if (state % 3 == 2)
    {
        canvas.all_pixels_hsv(240.0f, 1.0f, 1.0f);
    }


    if (state % 4 == 0)
    {
        canvas.noodle_level(0, 1.0f);
        canvas.noodle_level(1, 0.0f);
        canvas.noodle_level(2, 0.0f);
        canvas.noodle_level(3, 0.0f);
    }
    else if (state % 4 == 1)
    {
        canvas.noodle_level(0, 0.0f);
        canvas.noodle_level(1, 1.0f);
        canvas.noodle_level(2, 0.0f);
        canvas.noodle_level(3, 0.0f);
    }
    else if (state % 4 == 2)
    {
        canvas.noodle_level(0, 0.0f);
        canvas.noodle_level(1, 0.0f);
        canvas.noodle_level(2, 1.0f);
        canvas.noodle_level(3, 0.0f);
    }
    else if (state % 4 == 3)
    {
        canvas.noodle_level(0, 0.0f);
        canvas.noodle_level(1, 0.0f);
        canvas.noodle_level(2, 0.0f);
        canvas.noodle_level(3, 1.0f);
    }
}

void effect_micNField(Canvas& canvas, const AudioFrame& audio, float time)
{
    for (int i = 0; i < JellConfig::NUMBER_LEDS_IN_RING; i++)
    {
        Point3 p = canvas.ring_position(i);

        float n = Field::noise(p, 1.0f, audio.smoothed_level + time * .3);

        canvas.ring_pixel_hsv(
            i,
            220.0f + (n * n * 100),
            1.0f,
            audio.smoothed_level);
    }

    for (int s = 0; s < JellConfig::NUMBER_OF_TENTACLES; s++)
    {
        for (int i = 0; i < JellConfig::NUMBER_LEDS_IN_EACH_TENTACLE; i++)
        {
            Point3 p = canvas.spoke_position(s, i);

            float n = Field::noise(p, 0.5f, audio.smoothed_level + time * .3);

            canvas.spoke_pixel_hsv(
                s,
                i,
                220.0f + (n * n * 100),
                1.0f,
                audio.smoothed_level);
        }
    }

    float pwml = audio.smoothed_level * 2;

    if (pwml > 1.0f)
        pwml = 1.0f;

    canvas.all_noodles_level(pwml);

}

void effect_micDrops(Canvas& canvas, const AudioFrame& audio, float time, bool beat)
{
    // --- Tunables ---
    constexpr float TRAIL_TAU_S = 0.12f;           // afterglow behind a drop head
    constexpr float DROP_SPEED_LEDS_PER_S = 30.0f; // how fast a drop runs down a tentacle
    constexpr float FLASH_TAU_S = 0.25f;           // ring and noodle flash decay after a beat
    constexpr float IDLE_DROP_EVERY_S = 1.5f;      // in silence, a lone drop now and then
    constexpr float HUE_IDLE = 220.0f;
    constexpr float HUE_DROP = 190.0f;

    // --- State ---
    static uint64_t last_us = 0;
    static float since_beat_s = 1000.0f;
    static float since_idle_drop_s = 0.0f;
    static float drop_pos[JellConfig::NUMBER_OF_TENTACLES]; // head position in LEDs, < 0 = no drop
    static bool initialised = false;

    if (!initialised)
    {
        init_drops(drop_pos);
        initialised = true;
    }

    const float dt = seconds_since_last_call(last_us);
    since_beat_s += dt;
    since_idle_drop_s += dt;

    if (beat)
    {
        since_beat_s = 0.0f;
        since_idle_drop_s = 0.0f;
        for (int s = 0; s < JellConfig::NUMBER_OF_TENTACLES; s++)
            drop_pos[s] = 0.0f;
    }
    else if (since_idle_drop_s > IDLE_DROP_EVERY_S)
    {
        since_idle_drop_s = 0.0f;
        drop_pos[rand() % JellConfig::NUMBER_OF_TENTACLES] = 0.0f;
    }

    // Everything drawn in earlier frames fades; the drops' tails are what this leaves behind.
    canvas.fade(dt, TRAIL_TAU_S);

    const float flash = expf(-since_beat_s / FLASH_TAU_S);

    // Ring: a dim glow that follows the music, with the beat flash on top.
    const float ring_v = std::max(0.05f + 0.35f * audio.smoothed_level, flash);
    for (int i = 0; i < JellConfig::NUMBER_LEDS_IN_RING; i++)
    {
        Point3 p = canvas.ring_position(i);
        float n = Field::noise(p, 1.0f, time * 0.3f);
        canvas.ring_pixel_hsv(i, HUE_IDLE + (HUE_DROP - HUE_IDLE) * flash + n * 30.0f, 1.0f, ring_v);
    }

    // Drops: only the head is drawn each frame, the fade above draws the tail.
    advance_drops(canvas, drop_pos, DROP_SPEED_LEDS_PER_S, dt, HUE_DROP, 0.8f, 1.0f);

    canvas.all_noodles_level(std::max(0.15f, flash));

}

float palette_hue(int slot, float time, float cycle_period_s, bool cycle)
{
    constexpr int N = JellConfig::PALETTE_SIZE;

    if (slot < 0)
        slot = 0;

    if (!cycle)
        return JellConfig::PALETTE[slot % N];

    if (cycle_period_s < 1.0f)
        cycle_period_s = 1.0f;

    const float blend_s = std::min(JellConfig::CYCLE_BLEND_S, cycle_period_s * 0.5f);

    // Which period we are in, and how far into it.
    const float phase = time / cycle_period_s;
    const int step = (int)floorf(phase);
    const float within_s = (phase - (float)step) * cycle_period_s;

    const int from = (((slot + step) % N) + N) % N;
    const int to = (from + 1) % N;

    const float t = (within_s - (cycle_period_s - blend_s)) / blend_s; // < 0 outside the blend window
    return hue_lerp_shortest(JellConfig::PALETTE[from], JellConfig::PALETTE[to], smoothstep01(t));
}

void effect_palette(Canvas& canvas, float time, float hue)
{
    // The ambient noise animation, held to +-10 degrees around this jelly's hue.
    effect_ambientNField(canvas, time, 1.0f, hue, 20.0f, 0.15f);
}

void effect_ambientNField(Canvas& canvas, float time, float noisescale, float huebase, float huerange, float timescale)
{
    //float noisescale = 1.0f;
    float field_offset = 1000.0;
    //float huebase = 220.0f;
    //float huerange = 360.0f;
    //float timescale = 0.1f;

    for (int i = 0; i < JellConfig::NUMBER_LEDS_IN_RING; i++)
    {
        Point3 p = canvas.ring_position(i);

        float f_b = Field::noise(p, noisescale, time * timescale);
        float f_h = Field::noise({p.x + field_offset, p.y, p.z}, noisescale, time * timescale);

        canvas.ring_pixel_hsv(
            i,
            huebase + (f_h * f_h - 0.5) * huerange,
            1.0f,
            f_b);
    }

    for (int s = 0; s < JellConfig::NUMBER_OF_TENTACLES; s++)
    {
        for (int i = 0; i < JellConfig::NUMBER_LEDS_IN_EACH_TENTACLE; i++)
        {
            Point3 p = canvas.spoke_position(s, i);

            float f_b = Field::noise(p, noisescale, time * timescale);
            float f_h = Field::noise({p.x + field_offset, p.y, p.z}, noisescale, time * timescale);

            canvas.spoke_pixel_hsv(
                s,
                i,
                huebase + (f_h * f_h - 0.5) * huerange,
                1.0f,
                f_b);
        }
    }

    for (int n = 0; n < JellConfig::NUMBER_OF_NOODLES; n++)
    {
        Point3 np = canvas.noodle_position(n);

        float f_b = Field::noise(np, noisescale, time * timescale);

        canvas.noodle_level(n, (f_b * .6f) + 0.4f);
    }

}

// ---------------------------------------------------------------- the calm modes

void effect_breathe(Canvas& canvas, float time)
{
    constexpr float PERIOD_S = 6.0f;
    constexpr float HUE = 200.0f;
    constexpr float TIP_LAG = 0.25f;   // fraction of a period the tentacle tip lags the ring
    constexpr float TENTACLE_DEPTH = 10.5f;

    const float p = frac(time / PERIOD_S);
    const float e = bump(p);

    for (int i = 0; i < JellConfig::NUMBER_LEDS_IN_RING; i++)
        canvas.ring_pixel_hsv(i, HUE + 15.0f * e, 0.85f, 0.06f + 0.54f * e);

    for (int t = 0; t < JellConfig::NUMBER_OF_TENTACLES; t++)
    {
        for (int j = 0; j < JellConfig::NUMBER_LEDS_IN_EACH_TENTACLE; j++)
        {
            const float z = canvas.spoke_position(t, j).z; // 0 at the ring, negative below
            const float ez = bump(frac(p + TIP_LAG * z / TENTACLE_DEPTH));
            canvas.spoke_pixel_hsv(t, j, HUE + 15.0f * ez, 0.85f, 0.06f + 0.38f * ez);
        }
    }

    canvas.all_noodles_level(0.10f + 0.5f * e);
}

namespace
{
    float glimmer_envelope(float u)
    {
        if (u < 0.15f)
            return u / 0.15f;
        const float d = (1.0f - u) / 0.85f;
        return d * d;
    }

    float firefly_envelope(float u)
    {
        return sinf(3.1415927f * u);
    }
}

void effect_glimmer(Canvas& canvas)
{
    constexpr int POOL = 12;
    constexpr float SPAWN_PER_S = 2.0f;
    constexpr float BG_H = 190.0f, BG_S = 0.9f, BG_V = 0.02f;

    static Spark pool[POOL];
    static uint64_t last_us = 0;
    const float dt = seconds_since_last_call(last_us);

    canvas.all_pixels_hsv(BG_H, BG_S, BG_V);
    canvas.all_noodles_level(0.03f);

    if (frand01() < dt * SPAWN_PER_S)
        spawn_spark(pool, POOL, 0.3f, 1.5f, 3.0f, 160.0f, 195.0f, 0.5f, 0.9f);

    draw_sparks(canvas, pool, POOL, dt, glimmer_envelope, 0.9f, BG_V);
}

void effect_aurora(Canvas& canvas, float time)
{
    constexpr float SCALE = 1.5f;
    constexpr float TIMESCALE = 0.03f; // one drift across the ring takes about 70 s
    constexpr float FIELD_OFFSET = 1000.0f;

    for (int i = 0; i < JellConfig::NUMBER_LEDS_IN_RING; i++)
    {
        const Point3 p = canvas.ring_position(i);
        const float f_b = Field::noise(p, SCALE, time * TIMESCALE);
        const float f_h = Field::noise({p.x + FIELD_OFFSET, p.y, p.z}, SCALE, time * TIMESCALE);
        canvas.ring_pixel_hsv(i, 195.0f + (f_h - 0.5f) * 150.0f, 1.0f, 0.15f + 0.5f * f_b);
    }

    for (int t = 0; t < JellConfig::NUMBER_OF_TENTACLES; t++)
    {
        for (int j = 0; j < JellConfig::NUMBER_LEDS_IN_EACH_TENTACLE; j++)
        {
            const Point3 p = canvas.spoke_position(t, j);
            const float f_b = Field::noise(p, SCALE, time * TIMESCALE);
            const float f_h = Field::noise({p.x + FIELD_OFFSET, p.y, p.z}, SCALE, time * TIMESCALE);
            const float depth = 1.0f + p.z / 14.0f; // 1 at the ring, ~0.25 at the tip
            canvas.spoke_pixel_hsv(t, j, 195.0f + (f_h - 0.5f) * 150.0f, 1.0f, 0.35f * (0.3f + 0.7f * f_b) * depth);
        }
    }

    for (int n = 0; n < JellConfig::NUMBER_OF_NOODLES; n++)
    {
        const float f_b = Field::noise(canvas.noodle_position(n), SCALE, time * TIMESCALE);
        canvas.noodle_level(n, 0.15f + 0.25f * f_b);
    }
}

void effect_current(Canvas& canvas, float time)
{
    constexpr float PERIOD_S = 8.0f;

    const float ph_ring = time / PERIOD_S;

    for (int i = 0; i < JellConfig::NUMBER_LEDS_IN_RING; i++)
        canvas.ring_pixel_hsv(i, 200.0f, 0.9f, 0.12f + 0.08f * sinf(TWO_PI * ph_ring));

    for (int t = 0; t < JellConfig::NUMBER_OF_TENTACLES; t++)
    {
        for (int j = 0; j < JellConfig::NUMBER_LEDS_IN_EACH_TENTACLE; j++)
        {
            // + j/16 makes the crest move towards j = 0, i.e. up the strand.
            const float ph = ph_ring + (float)j / (float)JellConfig::NUMBER_LEDS_IN_EACH_TENTACLE + 0.13f * (float)t;
            const float w = 0.5f + 0.5f * sinf(TWO_PI * ph);
            canvas.spoke_pixel_hsv(t, j, 185.0f, 0.9f, 0.08f + 0.35f * w * w);
        }
    }

    for (int n = 0; n < JellConfig::NUMBER_OF_NOODLES; n++)
        canvas.noodle_level(n, 0.10f + 0.15f * (0.5f + 0.5f * sinf(TWO_PI * ph_ring + 0.25f * (float)n)));
}

void effect_lantern(Canvas& canvas, float time)
{
    constexpr float HUE_RING = 32.0f;
    constexpr float HUE_TIP_SHIFT = 14.0f; // deeper orange towards the tentacle tips
    constexpr float FIELD_OFFSET = 1000.0f;
    constexpr float FLICKER = 0.08f;      // the whole candle effect; resist making it larger

    auto lantern_v = [&](Point3 p) {
        const float slow = Field::noise({p.x + FIELD_OFFSET, p.y, p.z}, 0.5f, time * 0.1f);
        const float fast = Field::noise(p, 2.0f, time * 0.8f);
        return 0.35f + 0.15f * slow + FLICKER * (fast - 0.5f);
    };

    for (int i = 0; i < JellConfig::NUMBER_LEDS_IN_RING; i++)
        canvas.ring_pixel_hsv(i, HUE_RING, 0.95f, lantern_v(canvas.ring_position(i)));

    for (int t = 0; t < JellConfig::NUMBER_OF_TENTACLES; t++)
    {
        for (int j = 0; j < JellConfig::NUMBER_LEDS_IN_EACH_TENTACLE; j++)
        {
            const Point3 p = canvas.spoke_position(t, j);
            canvas.spoke_pixel_hsv(t, j, HUE_RING + HUE_TIP_SHIFT * p.z / 10.5f, 0.95f, 0.6f * lantern_v(p));
        }
    }

    for (int n = 0; n < JellConfig::NUMBER_OF_NOODLES; n++)
        canvas.noodle_level(n, 0.35f + 0.1f * Field::noise(canvas.noodle_position(n), 2.0f, time * 0.8f));
}

void effect_moonlight(Canvas& canvas, float time)
{
    constexpr float HUE = 215.0f, SAT = 0.35f;
    constexpr float TIMESCALE = 0.02f;

    for (int i = 0; i < JellConfig::NUMBER_LEDS_IN_RING; i++)
        canvas.ring_pixel_hsv(i, HUE, SAT, 0.06f + 0.03f * Field::noise(canvas.ring_position(i), 1.0f, time * TIMESCALE));

    for (int t = 0; t < JellConfig::NUMBER_OF_TENTACLES; t++)
        for (int j = 0; j < JellConfig::NUMBER_LEDS_IN_EACH_TENTACLE; j++)
            canvas.spoke_pixel_hsv(t, j, HUE, SAT, 0.03f + 0.02f * Field::noise(canvas.spoke_position(t, j), 1.0f, time * TIMESCALE));

    canvas.all_noodles_level(0.02f);
}

void effect_drizzle(Canvas& canvas)
{
    constexpr float TRAIL_TAU_S = 0.6f;
    constexpr float DROP_SPEED_LEDS_PER_S = 6.0f; // about 2.7 s per strand
    constexpr float SPAWN_PER_S = 0.7f;
    constexpr float HUE = 205.0f;

    static float drop_pos[JellConfig::NUMBER_OF_TENTACLES];
    static bool initialised = false;
    static uint64_t last_us = 0;

    if (!initialised)
    {
        init_drops(drop_pos);
        initialised = true;
    }

    const float dt = seconds_since_last_call(last_us);

    canvas.fade(dt, TRAIL_TAU_S);

    // The fade touches the ring too, so its faint glow is redrawn every frame.
    for (int i = 0; i < JellConfig::NUMBER_LEDS_IN_RING; i++)
        canvas.ring_pixel_hsv(i, HUE, 0.8f, 0.03f);

    if (frand01() < dt * SPAWN_PER_S)
    {
        const int t = rand() % JellConfig::NUMBER_OF_TENTACLES;
        if (drop_pos[t] < 0.0f)
            drop_pos[t] = 0.0f;
    }

    advance_drops(canvas, drop_pos, DROP_SPEED_LEDS_PER_S, dt, HUE, 0.6f, 0.7f);

    canvas.all_noodles_level(0.05f);
}

void effect_fireflies(Canvas& canvas)
{
    constexpr int POOL = 6;
    constexpr float SPAWN_PER_S = 0.8f;

    static Spark pool[POOL];
    static uint64_t last_us = 0;
    const float dt = seconds_since_last_call(last_us);

    // Tentacles dark, a faint warm ring, so the fireflies are all one sees.
    canvas.all_pixels_hsv(50.0f, 0.8f, 0.0f);
    for (int i = 0; i < JellConfig::NUMBER_LEDS_IN_RING; i++)
        canvas.ring_pixel_hsv(i, 50.0f, 0.8f, 0.03f);
    canvas.all_noodles_level(0.03f);

    if (frand01() < dt * SPAWN_PER_S)
        spawn_spark(pool, POOL, 0.0f, 3.0f, 6.0f, 70.0f, 85.0f, 0.6f, 1.0f);

    draw_sparks(canvas, pool, POOL, dt, firefly_envelope, 0.85f, 0.0f);
}

void effect_swarm(Canvas& canvas, float time, int slot)
{
    constexpr float PERIOD_S = 12.0f;
    constexpr float SLOT_STEP = 0.125f; // the pulse visits the next slot 1.5 s later
    constexpr float PULSE = 0.35f;      // fraction of the period one jelly's pulse lasts
    constexpr float DOWNWARD_LAG = 0.05f;

    if (slot < 0)
        slot = 0;

    const float hue = palette_hue(slot, time, JellConfig::DEFAULT_CYCLE_PERIOD_S, false);
    const float q = frac(time / PERIOD_S - (float)slot * SLOT_STEP);

    auto pulse = [](float qq) {
        qq = frac(qq);
        return qq < PULSE ? 0.5f - 0.5f * cosf(TWO_PI * qq / PULSE) : 0.0f;
    };

    const float g = pulse(q);

    for (int i = 0; i < JellConfig::NUMBER_LEDS_IN_RING; i++)
        canvas.ring_pixel_hsv(i, hue, 1.0f, 0.08f + 0.6f * g);

    for (int t = 0; t < JellConfig::NUMBER_OF_TENTACLES; t++)
    {
        for (int j = 0; j < JellConfig::NUMBER_LEDS_IN_EACH_TENTACLE; j++)
        {
            const float z = canvas.spoke_position(t, j).z;
            const float gz = pulse(q - DOWNWARD_LAG * (-z) / 10.5f); // flows down the tentacle
            canvas.spoke_pixel_hsv(t, j, hue, 1.0f, 0.05f + 0.5f * gz);
        }
    }

    canvas.all_noodles_level(0.05f + 0.5f * g);
}

void effect_whisper(Canvas& canvas, const AudioFrame& audio, float time)
{
    constexpr float TAU_UP_S = 2.0f;
    constexpr float TAU_DOWN_S = 4.0f;
    constexpr float HUE_QUIET = 200.0f;
    constexpr float HUE_LIVELY = 40.0f;

    static float ema = 0.0f;

    const float level = std::clamp(audio.level, 0.0f, 1.0f);
    const float tau = level > ema ? TAU_UP_S : TAU_DOWN_S;
    ema += (level - ema) * (1.0f - expf(-audio.dt_s / tau));

    const float hue = HUE_QUIET - (HUE_QUIET - HUE_LIVELY) * smoothstep01(ema);
    const float base_v = 0.10f + 0.55f * ema;

    for (int i = 0; i < JellConfig::NUMBER_LEDS_IN_RING; i++)
    {
        const float n = Field::noise(canvas.ring_position(i), 1.0f, time * 0.05f) - 0.5f;
        canvas.ring_pixel_hsv(i, hue + 16.0f * n, 0.9f, base_v + 0.05f * n);
    }

    for (int t = 0; t < JellConfig::NUMBER_OF_TENTACLES; t++)
    {
        for (int j = 0; j < JellConfig::NUMBER_LEDS_IN_EACH_TENTACLE; j++)
        {
            const float n = Field::noise(canvas.spoke_position(t, j), 1.0f, time * 0.05f) - 0.5f;
            canvas.spoke_pixel_hsv(t, j, hue + 16.0f * n, 0.9f, 0.7f * (base_v + 0.05f * n));
        }
    }

    canvas.all_noodles_level(0.05f + 0.4f * ema);
}

void effect_sos(Canvas& canvas, float time)
{
    constexpr float UNIT_S = 0.25f; // one Morse unit; a dit is 1, a dah 3
    // On/off run lengths in units: S (dit dit dit), gap, O (dah dah dah), gap, S, word gap.
    // Positive = light on, negative = off. Sums to 34 units, 8.5 s per cycle.
    constexpr int PATTERN[] = {1, -1, 1, -1, 1, -3, 3, -1, 3, -1, 3, -3, 1, -1, 1, -1, 1, -7};
    constexpr int TOTAL_UNITS = 34;

    float u = fmodf(time, TOTAL_UNITS * UNIT_S) / UNIT_S;
    if (u < 0.0f)
        u += TOTAL_UNITS;

    bool on = false;
    for (int run : PATTERN)
    {
        const float len = (float)(run < 0 ? -run : run);
        if (u < len)
        {
            on = run > 0;
            break;
        }
        u -= len;
    }

    canvas.all_pixels_hsv(0.0f, 1.0f, on ? 1.0f : 0.0f);
    canvas.all_noodles_level(on ? 1.0f : 0.0f);
}
