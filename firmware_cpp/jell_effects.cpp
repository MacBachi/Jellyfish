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

    // Interpolate between two hues the short way round the colour circle.
    float hue_lerp_shortest(float a, float b, float t)
    {
        const float d = fmodf(b - a + 540.0f, 360.0f) - 180.0f; // -180..180
        return fmodf(a + d * t + 360.0f, 360.0f);
    }

    float smoothstep01(float t)
    {
        t = std::clamp(t, 0.0f, 1.0f);
        return t * t * (3.0f - 2.0f * t);
    }
}

void effect_miclevelCheck(
    Canvas& canvas,
    const AudioFrame& audio)
{
    canvas.all_pixels_hsv(220.0f, 1.0f, audio.smoothed_level);
    canvas.all_noodles_level(audio.smoothed_level);
    canvas.show();
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
    canvas.show();
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

    canvas.show();
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
        for (int s = 0; s < JellConfig::NUMBER_OF_TENTACLES; s++)
            drop_pos[s] = -1.0f;
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
    for (int s = 0; s < JellConfig::NUMBER_OF_TENTACLES; s++)
    {
        if (drop_pos[s] < 0.0f)
            continue;

        canvas.spoke_pixel_hsv(s, (int)drop_pos[s], HUE_DROP, 0.8f, 1.0f);

        drop_pos[s] += DROP_SPEED_LEDS_PER_S * dt;
        if (drop_pos[s] >= (float)JellConfig::NUMBER_LEDS_IN_EACH_TENTACLE)
            drop_pos[s] = -1.0f;
    }

    canvas.all_noodles_level(std::max(0.15f, flash));

    canvas.show();
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

    canvas.show();
}
