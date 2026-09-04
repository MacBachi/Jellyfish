// power_sim: how much current every display mode draws.
//
// Builds the real effect code (jell_effects.cpp, jell_field.cpp, jell_canvas.cpp) for the
// host with stub Pico headers, runs each mode for a few minutes at 60 frames per second and
// sums the LED current from the RGB bytes the canvas would have sent to the strips. The
// result is what a USB power meter at the jelly's socket would show, up to the LED model:
// WS2812B chips measure between 34 and 65 mA at full white depending on the generation, so
// three models are printed side by side.
//
//   make            build with the default ring (JELL_RING_LEDS from jell_config.hpp)
//   make RING=39    build for a 39-LED ring
//   ./power_sim [tentacles] [leds per tentacle] [mic level 0..1] [brightness 0..1]
//
// Defaults: 4 tentacles of 12 LEDs (the original build), a quiet room (level 0.3), full
// brightness. Try level 1.0 for the sound modes at a loud party.
//
// Sources for the models:
//   60 mA: Adafruit's rule of thumb for WS2812B full white; QuinLED measured 65 mA
//   50 mA: PJRC measured 52.5 mA on one WS2812B strip
//   35 mA: PJRC measured 33.5 mA on another WS2812B product (newer chip)
//   tentacle wire: Pimoroni WS2812B-4040, 0.24 W per LED = 48 mA
//   dark LED: about 1 mA (PJRC, Zaitronics)
//   Pico 2 W with WLAN up, no power saving: 80 mA (peppe8o)
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include "jell_config.hpp"
#include "jell_canvas.hpp"
#include "jell_effects.hpp"

uint64_t g_now_us = 0;
const pio_program ws2812_program = {};
using DM = JellConfig::DisplayMode;

constexpr int RING = JellConfig::NUMBER_LEDS_IN_RING;
constexpr int TENT = JellConfig::NUMBER_OF_TENTACLES;
constexpr int TLEN = JellConfig::NUMBER_LEDS_IN_EACH_TENTACLE;
constexpr int NOOD = JellConfig::NUMBER_OF_NOODLES;

LedString ring(pio0, 1, 2, RING, JellConfig::LED_ORDER_RING);
LedString spokes[TENT] = {
    LedString(pio0, 2, 3, TLEN), LedString(pio0, 3, 4, TLEN), LedString(pio1, 0, 5, TLEN),
    LedString(pio1, 1, 6, TLEN), LedString(pio2, 0, 7, TLEN), LedString(pio2, 1, 8, TLEN),
    LedString(pio2, 2, 9, TLEN)};
PwmLight noodles[NOOD] = {
    PwmLight(12, 0.5f, 0.0f, 0.8f), PwmLight(13, 0.0f, 0.5f, 0.8f),
    PwmLight(14, -0.5f, 0.0f, 0.8f), PwmLight(15, 0.0f, -0.5f, 0.8f)};
Canvas canvas(ring, spokes, noodles);

// One LED model: current per colour channel at full duty, and the dark current.
// Full white = 3 * channel + dark.
struct Model { const char* name; double ring_ch_mA; double tent_ch_mA; double dark_mA; };
const Model MODELS[] = {
    {"60 mA LEDs", 19.7, 15.7, 1.0},
    {"50 mA LEDs", 16.3, 15.7, 1.0},
    {"35 mA LEDs", 10.8, 10.8, 1.0},
};
constexpr int NMODELS = sizeof(MODELS) / sizeof(MODELS[0]);
constexpr double PICO_mA = 80.0;   // Pico 2 W, WLAN up, no power saving
constexpr double NOODLE_mA = 10.0; // one noodle at full level, through its series resistor

const char* NAMES[] = {"Breathe", "Glimmer", "Aurora", "Current", "Lantern", "Moonlight", "Drizzle",
                       "Fireflies", "Swarm", "Whisper", "Playlist", "Mic field", "Mic drops", "Palette",
                       "Palette cycle", "Ambient rainbow", "Ambient deepsea", "Mic level check",
                       "LED channel test", "SOS"};
static_assert(sizeof(NAMES) / sizeof(NAMES[0]) == (int)DM::Count, "one name per mode");

// Same dispatch as render_mode() in JellyFloatOS.cpp, minus Playlist (it only picks one of these).
void render(DM mode, float time, const AudioFrame& audio, bool beat, int slot)
{
    switch (mode)
    {
    case DM::micLevelCheck: effect_miclevelCheck(canvas, audio); break;
    case DM::LEDChannelTest: effect_LEDchanneltest(canvas); break;
    case DM::Mic_NField: effect_micNField(canvas, audio, time); break;
    case DM::Mic_Drops: effect_micDrops(canvas, audio, time, beat); break;
    case DM::Palette: effect_palette(canvas, time, palette_hue(slot, time, JellConfig::DEFAULT_CYCLE_PERIOD_S, false)); break;
    case DM::Palette_Cycle: effect_palette(canvas, time, palette_hue(slot, time, JellConfig::DEFAULT_CYCLE_PERIOD_S, true)); break;
    case DM::Ambient_Rainbow: effect_ambientNField(canvas, time, 1.0f, 220.0f, 360.0f, 0.15f); break;
    case DM::Ambient_Deepsea: effect_ambientNField(canvas, time, 2.0f, 220.0f, 100.0f, 0.8f); break;
    case DM::Breathe: effect_breathe(canvas, time); break;
    case DM::Glimmer: effect_glimmer(canvas); break;
    case DM::Aurora: effect_aurora(canvas, time); break;
    case DM::Current: effect_current(canvas, time); break;
    case DM::Lantern: effect_lantern(canvas, time); break;
    case DM::Moonlight: effect_moonlight(canvas, time); break;
    case DM::Drizzle: effect_drizzle(canvas); break;
    case DM::Fireflies: effect_fireflies(canvas); break;
    case DM::Swarm: effect_swarm(canvas, time, slot); break;
    case DM::Whisper: effect_whisper(canvas, audio, time); break;
    case DM::SOS: effect_sos(canvas, time); break;
    default: break;
    }
}

int main(int argc, char** argv)
{
    const int tent_count = argc > 1 ? atoi(argv[1]) : 4;
    const int tent_len = argc > 2 ? atoi(argv[2]) : 12;
    const float level = argc > 3 ? (float)atof(argv[3]) : 0.3f;
    const float brightness = argc > 4 ? (float)atof(argv[4]) : 1.0f;
    if (tent_count < 0 || tent_count > TENT || tent_len < 0 || tent_len > TLEN)
    {
        fprintf(stderr, "tentacles 0..%d, leds per tentacle 0..%d\n", TENT, TLEN);
        return 1;
    }

    // The LED positions from JellyFloatOS.cpp: the ring on a unit circle, the tentacles at
    // their header positions with the measured heights.
    for (int i = 0; i < RING; i++)
    {
        const float a = 2.0f * (float)M_PI * (float)i / (float)RING;
        ring.map_pixel(i, cosf(a), sinf(a), 0.0f);
    }
    const float height_map[TLEN] = {-1, -2, -2.75, -1.75, -0.5, -0.5, -1.5, -2.5, -3.5, -4.5, -5.5, -6.5,
                                    -7.5, -8.5, -9.5, -10.5};
    const float tentacle_xy[TENT][2] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1},
                                        {0.7071f, 0.7071f}, {-0.7071f, 0.7071f}, {-0.7071f, -0.7071f}};
    for (int s = 0; s < TENT; s++)
        for (int i = 0; i < TLEN; i++)
            spokes[s].map_pixel(i, tentacle_xy[s][0], tentacle_xy[s][1], height_map[i]);

    printf("Ring %d LEDs, %d tentacles x %d LEDs, mic level %.1f, brightness %.2f, Pico %.0f mA\n",
           RING, tent_count, tent_len, level, brightness, PICO_mA);
    printf("Peak and mean supply current per mode (mA):\n\n%-18s", "mode");
    for (int k = 0; k < NMODELS; k++) printf(" | %-11s peak  mean", MODELS[k].name);
    printf("\n");

    static uint8_t frame[Canvas::FRAME_BYTES];
    const double duration_s = 240.0, fps = 60.0;
    for (int m = 0; m < (int)DM::Count; m++)
    {
        if (m == (int)DM::Playlist) continue;
        srand(1);
        canvas.clear();
        canvas.begin_crossfade(1.0f);
        canvas.set_global(brightness, 0.0f);
        double peak[NMODELS] = {}, sum[NMODELS] = {};
        int frames = 0;
        g_now_us = 1000000;
        for (int f = 0; f < (int)(duration_s * fps); f++)
        {
            g_now_us += (uint64_t)(1e6 / fps);
            const float t = (float)g_now_us * 1e-6f;
            AudioFrame a{};
            a.level = a.smoothed_level = a.rms = a.smoothed_peak = level;
            a.dt_s = 1.0f / (float)fps;
            const bool beat = (m == (int)DM::Mic_Drops) && (f % 30 == 0) && level > 0.5f;
            render((DM)m, t, a, beat, 0);
            canvas.show(1.0f);
            canvas.copy_frame(frame);
            frames++;
            for (int k = 0; k < NMODELS; k++)
            {
                const Model& M = MODELS[k];
                double mA = PICO_mA;
                for (int i = 0; i < RING; i++)
                    mA += M.dark_mA + M.ring_ch_mA * (frame[i * 3] + frame[i * 3 + 1] + frame[i * 3 + 2]) / 255.0;
                for (int s = 0; s < tent_count; s++)
                    for (int i = 0; i < tent_len; i++)
                    {
                        const uint8_t* p = frame + 3 * (RING + s * TLEN + i);
                        mA += M.dark_mA + M.tent_ch_mA * (p[0] + p[1] + p[2]) / 255.0;
                    }
                for (int n = 0; n < NOOD; n++)
                    mA += NOODLE_mA * frame[3 * (RING + TENT * TLEN) + n] / 255.0;
                sum[k] += mA;
                if (mA > peak[k]) peak[k] = mA;
            }
        }
        printf("%-18s", NAMES[m]);
        for (int k = 0; k < NMODELS; k++) printf(" | %5.0f %5.0f            ", peak[k], sum[k] / frames);
        printf("\n");
    }
    return 0;
}
