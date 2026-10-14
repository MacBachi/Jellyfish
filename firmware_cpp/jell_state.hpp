#pragma once
#include <cstdint>
#include "pico/stdlib.h"
#include "hardware/sync.h" // __dmb
#include "jell_config.hpp"

// Everything core 1 needs to render a frame. Core 0 owns it: the buttons and the network
// write it, core 1 only ever reads a consistent snapshot once per frame.
struct JellState
{
    JellConfig::DisplayMode mode = JellConfig::DEFAULT_DISPLAY_MODE;
    float brightness = JellConfig::DEFAULT_BRIGHTNESS;          // 0..1, scales every pixel
    float hue_offset = JellConfig::DEFAULT_HUE_OFFSET;          // degrees added to every hue
    float cycle_period_s = JellConfig::DEFAULT_CYCLE_PERIOD_S;  // Palette_Cycle period
    bool noodle_follow = JellConfig::DEFAULT_NOODLE_FOLLOW;     // brightness dims the noodles too
    float sensitivity = JellConfig::DEFAULT_SENSITIVITY;        // SENS: bloom-wide microphone gain
    int slot = 0;                        // colour slot of this jelly; AP = 0, -1 = none assigned yet
    bool is_ap = true;                   // until the network says otherwise this jelly is on its own
    bool follow_network_beats = false;   // station: drops fire on beat_count, not on its own microphone
    int64_t time_offset_us = 0;          // master_us = time_us_64() + time_offset_us
    uint32_t beat_count = 0;             // the bloom's beat count: the AP's, adopted by every station
    uint32_t tempo_period_us = 0;        // the AP's tempo, from its TEMPO line; 0 = none known
    int64_t tempo_anchor_master_us = 0;  // master time of one of the AP's beats
    float tempo_conf = 0.0f;             // the AP's confidence in that tempo
    uint64_t tempo_rx_local_us = 0;      // local time the last TEMPO arrived (for the fade)
    int64_t ident_start_master_us = 0;   // 0 = not identifying, else the master time the blinking started
};

// The mode actually rendered for a state at a master time. The playlist resolves to one
// of its entries from the master time, in integer microseconds so every jelly (and the
// web page's caption) picks the same one.
inline JellConfig::DisplayMode effective_mode(const JellState& s, int64_t master_us)
{
    if (s.mode != JellConfig::DisplayMode::Playlist)
        return s.mode;

    constexpr int64_t step_us = (int64_t)(JellConfig::PLAYLIST_STEP_S * 1e6f);
    constexpr int n = JellConfig::PLAYLIST_SIZE;
    const int index = (int)(((master_us / step_us) % n + n) % n);
    return JellConfig::PLAYLIST[index];
}

// Single-writer seqlock. write() is only ever called from core 0, read() from core 1.
// The sequence counter is odd while a write is in progress; a reader retries until it
// has copied the struct under an even, unchanged counter.
class SharedState
{
public:
    void write(const JellState& s)
    {
        seq = seq + 1;
        __dmb();
        data = s;
        __dmb();
        seq = seq + 1;
    }

    JellState read() const
    {
        while (true)
        {
            const uint32_t before = seq;
            if (before & 1u)
                continue;
            __dmb();
            JellState copy = data;
            __dmb();
            if (seq == before)
                return copy;
        }
    }

private:
    volatile uint32_t seq = 0;
    JellState data;
};

extern SharedState g_state;

// Core 1 -> core 0: number of beats this jelly detected with its own microphone.
extern volatile uint32_t g_local_beat_count;

// Core 1 -> core 0: frames rendered so far. Core 0 feeds the watchdog only while this moves.
extern volatile uint32_t g_core1_frames;

// Core 0 asks core 1 to pause between two frames (a flash write must not interrupt an LED
// push); core 1 answers with parked while it waits.
extern volatile bool g_core1_park;
extern volatile bool g_core1_parked;

// This jelly's own microphone trim (TRIM), restored from flash; multiplies the level.
extern volatile float g_trim;

// Core 1 -> core 0: the current smoothed microphone level (0..1), for the LEVEL stream.
extern volatile float g_local_level;
extern volatile float g_local_bass;
extern volatile float g_local_mid;
extern volatile float g_local_treble;
extern volatile float g_local_melody;
extern volatile float g_local_rise;
extern volatile float g_local_quiet;
extern volatile float g_local_phase;
extern volatile float g_local_conf;
extern volatile float g_local_tempo_period_s; // 0 until the tracker has a period
extern volatile float g_local_floor;          // the microphone's calibration, raw counts
extern volatile float g_local_ceiling;
