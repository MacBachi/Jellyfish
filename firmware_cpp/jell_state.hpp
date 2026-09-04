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
    float brightness = JellConfig::DEFAULT_BRIGHTNESS;          // 0..1, scales every pixel and noodle
    float hue_offset = JellConfig::DEFAULT_HUE_OFFSET;          // degrees added to every hue
    float cycle_period_s = JellConfig::DEFAULT_CYCLE_PERIOD_S;  // Palette_Cycle period
    int slot = 0;                        // colour slot of this jelly; AP = 0, -1 = none assigned yet
    bool is_ap = true;                   // until the network says otherwise this jelly is on its own
    bool follow_network_beats = false;   // station: drops fire on beat_count, not on its own microphone
    int64_t time_offset_us = 0;          // master_us = time_us_64() + time_offset_us
    uint32_t beat_count = 0;             // bumped by core 0 for every BEAT received from the AP
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

// Core 1 -> core 0: the current smoothed microphone level (0..1), for the LEVEL stream.
extern volatile float g_local_level;
