#pragma once
#include <cstdint>
#include "hardware/pio.h"
#include "hardware/dma.h"

struct AudioFrame
{
    int32_t* samples;
    int sample_count;

    int32_t mean;
    float rms;
    float level;

    float rms_min; //temp copies for graphing
    float rms_max; 
    float smoothed_peak;
    float smoothed_level;
};

class Microphone
{
public:

    Microphone(int32_t sample_count);
    ~Microphone();

    // Owns DMA buffers, a DMA channel and a PIO state machine: never copy.
    Microphone(const Microphone&) = delete;
    Microphone& operator=(const Microphone&) = delete;

    void init(PIO pio,
              uint sm,
              uint pin_bclk,
              uint pin_din);

    AudioFrame capture();

    int get_sample_size() const;

    float rms_min = 64000.0f;
    float rms_max = 90000.0f;
    float smoothed_peak = 0.0f;
    float smoothed_level = 0.0f;

    // Time constants of the adaptive filters, in seconds. They reproduce the original
    // per-frame factors (0.002 for the range, 0.99 for level and peak) at the ~140 frames
    // per second the firmware ran at with 4 x 12 tentacle LEDs, but no longer depend on
    // how long a frame takes.
    static constexpr float RANGE_TRACK_TAU_S = 3.5f;
    static constexpr float LEVEL_DECAY_TAU_S = 0.7f;

    // Longest gap between captures that is fed into the filters. Anything longer
    // (e.g. after a mode change) is treated as this, so one stall can't collapse the range.
    static constexpr float MAX_FRAME_GAP_S = 0.1f;


private:

    void audio_input_init(PIO pio,
                          uint sm,
                          uint pin_bclk,
                          uint pin_din);

    int sample_size;

    int32_t *buffer_0;
    int32_t *buffer_1;
    int32_t *next_buffer_to_fill;

    int dma_chan;

    uint64_t last_capture_us = 0;

    uint audio_sm;
    PIO audio_pio;

};
