#include <cstdio>
#include <cstdlib>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include <cmath>

#include "jell_led.hpp"
#include "jell_audio.hpp"
#include "jell_canvas.hpp"
#include "jell_config.hpp"
#include "jell_effects.hpp"
#include "jell_state.hpp"
#include "jell_beat.hpp"

namespace {
    constexpr uint BUTTON_PREV = 19;
    constexpr uint BUTTON_NEXT = 20;
    constexpr uint LOOP_SLEEP_DURATION_MS = 20;
}

// Shared between the cores, see jell_state.hpp.
SharedState g_state;
volatile uint32_t g_local_beat_count = 0;

// Core 0's own copy of the state; every change goes through here and then into g_state.
JellState core0_state;

// --- Global State ---
// Initialize LEDs     
LedString ring(pio0, 1, 2, JellConfig::NUMBER_LEDS_IN_RING, JellConfig::LED_ORDER_RING);
// One LedString per tentacle header NeoPix2..NeoPix8 (GPIO 3..9).
// PIO0 SM0 belongs to the microphone. PIO1 SM2/SM3 and PIO2 SM3 stay unclaimed so the
// CYW43 WLAN driver can pick one of them.
LedString spokes[JellConfig::NUMBER_OF_TENTACLES] = {
    LedString(pio0, 2, 3, JellConfig::NUMBER_LEDS_IN_EACH_TENTACLE, JellConfig::LED_ORDER_TENTACLE),
    LedString(pio0, 3, 4, JellConfig::NUMBER_LEDS_IN_EACH_TENTACLE, JellConfig::LED_ORDER_TENTACLE),
    LedString(pio1, 0, 5, JellConfig::NUMBER_LEDS_IN_EACH_TENTACLE, JellConfig::LED_ORDER_TENTACLE),
    LedString(pio1, 1, 6, JellConfig::NUMBER_LEDS_IN_EACH_TENTACLE, JellConfig::LED_ORDER_TENTACLE),
    LedString(pio2, 0, 7, JellConfig::NUMBER_LEDS_IN_EACH_TENTACLE, JellConfig::LED_ORDER_TENTACLE),
    LedString(pio2, 1, 8, JellConfig::NUMBER_LEDS_IN_EACH_TENTACLE, JellConfig::LED_ORDER_TENTACLE),
    LedString(pio2, 2, 9, JellConfig::NUMBER_LEDS_IN_EACH_TENTACLE, JellConfig::LED_ORDER_TENTACLE)
};

PwmLight noodles[JellConfig::NUMBER_OF_NOODLES] = {
    PwmLight(12, 0.5f, 0.0f, 0.8f),
    PwmLight(13, 0.0f, 0.5f, 0.8f),
    PwmLight(14, -0.5f, 0.0f, 0.8f),
    PwmLight(15, 0.0f, -0.5f, 0.8f)
};

// Initialize Audio on PIO0, state machine 0, BCLK=Pin16, WS = BCLK + 1, DIN=Pin18
Microphone mic(256);

Canvas canvas(ring, spokes, noodles);

//button helpers
void step_mode(int delta)
{
    constexpr int count = static_cast<int>(JellConfig::DisplayMode::Count);
    int mode = static_cast<int>(core0_state.mode);
    mode = ((mode + delta) % count + count) % count;
    core0_state.mode = static_cast<JellConfig::DisplayMode>(mode);
    g_state.write(core0_state);
}

// Identify: the AP jelly blinks red, every other jelly blue, IDENT_BLINKS times in step
// with each other (the blink phase comes from the shared master time). Returns true
// while it is drawing, in which case the normal effect is skipped for this frame.
bool render_ident(const JellState& s, int64_t master_us)
{
    if (s.ident_start_master_us == 0)
        return false;

    const int64_t period_us = (int64_t)(JellConfig::IDENT_BLINK_PERIOD_S * 1e6f);
    const int64_t elapsed_us = master_us - s.ident_start_master_us;

    if (elapsed_us < 0 || elapsed_us >= period_us * JellConfig::IDENT_BLINKS)
        return false;

    const bool on = (elapsed_us % period_us) < period_us / 2;

    canvas.set_global(1.0f, 0.0f); // true red / blue regardless of the current hue offset
    canvas.all_pixels_hsv(s.is_ap ? 0.0f : 240.0f, 1.0f, on ? 1.0f : 0.0f);
    canvas.all_noodles_level(on ? 1.0f : 0.0f);
    canvas.show();
    return true;
}


// --- Main ---

// This runs ONLY on Core 1
[[noreturn]] void core1_entry()
{
    static BeatDetector beat_detector;
    static uint32_t seen_network_beats = 0;

    while (true)
    {
        const JellState s = g_state.read();

        // Master time: the AP's clock, which every station follows via time_offset_us.
        const int64_t master_us = (int64_t)time_us_64() + s.time_offset_us;
        const float time = (float)master_us * 1e-6f;

        if (render_ident(s, master_us))
            continue;

        canvas.set_global(s.brightness, s.hue_offset);

        switch (s.mode)
        {
        case JellConfig::DisplayMode::micLevelCheck:
            {
                AudioFrame audio = mic.capture();
                printf(">Level: %f, RMS: %f, RMS_Min: %f, RMS_Max: %f, smoothed_peak: %f, smoothed_level: %f\n",
                       audio.level, audio.rms, audio.rms_min, audio.rms_max, audio.smoothed_peak, audio.smoothed_level);

                effect_miclevelCheck(canvas, audio);
                break;
            }

        case JellConfig::DisplayMode::LEDChannelTest:
            {
                effect_LEDchanneltest(canvas);
                break;
            }

        case JellConfig::DisplayMode::Mic_NField:
            {
                AudioFrame audio = mic.capture();
                effect_micNField(canvas, audio, time);
                break;
            }

        case JellConfig::DisplayMode::Mic_Drops:
            {
                AudioFrame audio = mic.capture();

                bool beat;
                if (s.follow_network_beats)
                {
                    // Station: drop whenever the AP told us it heard a beat.
                    beat = (s.beat_count != seen_network_beats);
                    seen_network_beats = s.beat_count;
                }
                else
                {
                    // AP or lone jelly: listen ourselves, and let core 0 tell the others.
                    beat = beat_detector.update(audio.level);
                    if (beat)
                        g_local_beat_count = g_local_beat_count + 1;
                }

                effect_micDrops(canvas, audio, time, beat);
                break;
            }

        case JellConfig::DisplayMode::Palette:
            {
                effect_palette(canvas, time, palette_hue(s.slot, time, s.cycle_period_s, false));
                break;
            }

        case JellConfig::DisplayMode::Palette_Cycle:
            {
                effect_palette(canvas, time, palette_hue(s.slot, time, s.cycle_period_s, true));
                break;
            }

        case JellConfig::DisplayMode::Ambient_Rainbow:
            {
                effect_ambientNField(canvas, time, 1.0f, 220.0f, 360.0f, 0.15f);
                break;
            }

        case JellConfig::DisplayMode::Ambient_Deepsea:
            {
                effect_ambientNField(canvas, time, 2.0f, 220.0f, 100.0f, 0.8f);
                break;
            }

        default:
            break;
        }
    }
}


[[noreturn]] int main()
{
    stdio_init_all();
    sleep_ms(1000);

    mic.init(
        pio0,
        0,
        16,
        18);

    gpio_init(BUTTON_PREV);
    gpio_set_dir(BUTTON_PREV, GPIO_IN);
    gpio_pull_up(BUTTON_PREV);

    gpio_init(BUTTON_NEXT);
    gpio_set_dir(BUTTON_NEXT, GPIO_IN);
    gpio_pull_up(BUTTON_NEXT);

    // Map LEDs to 3D/Spatial coordinates
    // Map LEDs around a unit circle
    for (int i = 0; i < JellConfig::NUMBER_LEDS_IN_RING; i++)
    {
        float angle = 2.0f * M_PI * (float)i / (float)JellConfig::NUMBER_LEDS_IN_RING;

        float x = cosf(angle);
        float y = sinf(angle);

        ring.map_pixel(i, x, y, 0.0f);
    }

    // Height of each tentacle LED below the ring. The first 12 values were measured on the
    // draped tentacles; beyond that the tentacle is assumed to hang straight down.
    constexpr float height_map[] = {
        -1, -2, -2.75, -1.75, -0.5, -0.5, -1.5, -2.5, -3.5, -4.5, -5.5, -6.5,
        -7.5, -8.5, -9.5, -10.5};
    static_assert(sizeof(height_map) / sizeof(height_map[0]) == JellConfig::NUMBER_LEDS_IN_EACH_TENTACLE,
                  "height_map needs one entry per tentacle LED");

    // Where each tentacle hangs on the unit circle of the ring. NeoPix2..5 sit on the four
    // ribs of the print, NeoPix6..8 in between them.
    constexpr float tentacle_xy[][2] = {
        {1.0f, 0.0f}, {0.0f, 1.0f}, {-1.0f, 0.0f}, {0.0f, -1.0f},
        {0.7071f, 0.7071f}, {-0.7071f, 0.7071f}, {-0.7071f, -0.7071f}};
    static_assert(sizeof(tentacle_xy) / sizeof(tentacle_xy[0]) == JellConfig::NUMBER_OF_TENTACLES,
                  "tentacle_xy needs one entry per tentacle");

    for (int s = 0; s < JellConfig::NUMBER_OF_TENTACLES; s++)
    {
        for (int i = 0; i < JellConfig::NUMBER_LEDS_IN_EACH_TENTACLE; i++)
        {
            spokes[s].map_pixel(i, tentacle_xy[s][0], tentacle_xy[s][1], height_map[i]);
        }
    }

    g_state.write(core0_state);
    multicore_launch_core1(core1_entry);

    bool last_prev = false;
    bool last_next = false;

    while (true)
    {
        const bool prev = !gpio_get(BUTTON_PREV);
        const bool next = !gpio_get(BUTTON_NEXT);

        if (prev && !last_prev)
        {
            step_mode(-1);
        }

        if (next && !last_next)
        {
            step_mode(+1);
        }

        last_prev = prev;
        last_next = next;

        sleep_ms(LOOP_SLEEP_DURATION_MS);
    }
}
