#pragma once
#include <cstdint>
#include "hardware/pio.h"
#include "hardware/dma.h"
#include "jell_beat.hpp"

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

    float dt_s; // seconds since the previous capture (capped), for time-based smoothing in effects

    // The three bands of the filter bank, each 0..1: how loud this band is against what it
    // has been doing lately, with its own attack and release. Bass is the kick and the bass
    // line, mid the pads and the voice, treble the hats and the shakers. They are what the
    // sound-reactive effects follow; `level` is the whole signal and says little about music.
    float bass;
    float mid;
    float melody;   // 300 Hz to 800 Hz: the voice, the lead, the part you hum
    float treble;

    // What the music is doing over longer stretches.
    float rise;     // 0..1, how far the last few seconds sit above the last half minute
    float quiet;    // 0..1, 1 once the room has been silent for a few seconds
    float tempo_phase;      // 0..1, zero on the beat, keeps turning through a gap
    float tempo_bpm;
    float tempo_conf;       // 0 = no pulse found, 1 = steady
};

// One band of the filter bank: tracks how loud the band has been, stretches the current
// value into 0..1 against that, and puts an attack/release envelope on the result.
class AudioBand
{
public:
    float update(float rms, float dt_s, float range_tau_s, float attack_s, float release_s);

private:
    float hi_ = 0.0f;
    float lo_ = 0.0f;
    float env_ = 0.0f;
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

    // The I2S program spends 128 PIO cycles per sample at the system clock over 32.
    static constexpr float SAMPLE_RATE_HZ = 36621.0f;
    // Where the bands meet. Two cascaded one-pole filters per corner, so 12 dB per octave:
    // enough to tell a kick from a hi-hat, gentle enough that nothing rings.
    static constexpr float BASS_CORNER_HZ = 160.0f;
    static constexpr float TREBLE_CORNER_HZ = 1200.0f;
    // The melody band sits inside the mids, where a lead or a voice lives.
    static constexpr float MELODY_LOW_HZ = 300.0f;
    static constexpr float MELODY_HIGH_HZ = 800.0f;
    // A build-up is the last few seconds standing above the last half minute.
    static constexpr float RISE_FAST_TAU_S = 3.0f;
    static constexpr float RISE_SLOW_TAU_S = 25.0f;
    // Silence has to hold for a while to count; music ends it at once.
    static constexpr float QUIET_LEVEL = 0.12f;
    static constexpr float QUIET_ENTER_TAU_S = 4.0f;
    static constexpr float QUIET_LEAVE_TAU_S = 0.7f;
    // Rise fast enough to see the beat, fall slowly enough to stay calm.
    static constexpr float BASS_ATTACK_S = 0.02f, BASS_RELEASE_S = 0.40f;
    static constexpr float MID_ATTACK_S = 0.05f, MID_RELEASE_S = 0.55f;
    static constexpr float MELODY_ATTACK_S = 0.04f, MELODY_RELEASE_S = 0.45f;
    static constexpr float TREBLE_ATTACK_S = 0.01f, TREBLE_RELEASE_S = 0.15f;


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

    // Filter bank state. It carries across buffers, so the 7 ms window does not limit how
    // low the bass filter can reach.
    float k_bass = 0.0f, k_treble = 0.0f;
    float lp_bass_1 = 0.0f, lp_bass_2 = 0.0f;
    float lp_split_1 = 0.0f, lp_split_2 = 0.0f;
    float k_mel_lo = 0.0f, k_mel_hi = 0.0f;
    float lp_mel_lo_1 = 0.0f, lp_mel_lo_2 = 0.0f;
    float lp_mel_hi_1 = 0.0f, lp_mel_hi_2 = 0.0f;
    AudioBand band_bass, band_mid, band_melody, band_treble;

    // The longer view: the build-up, the silence and the pulse.
    float rise_fast_ = 0.0f, rise_slow_ = 0.0f, quiet_ = 0.0f;
    TempoTracker tempo_;

    uint audio_sm;
    PIO audio_pio;

};
