#include "jell_audio.hpp"
#include "i2s_microphone.pio.h"
#include "pico/time.h"
#include "math.h"
#include <algorithm>

// How loud is this band right now, against what it has been doing? hi and lo follow the
// band's loud and quiet ends; the current value is stretched between them. A band that
// sits still (a room hum, or silence) keeps a wide floor under it and so stays dark.
float AudioBand::update(float rms, float dt_s, float range_tau_s, float attack_s, float release_s)
{
    const float range_alpha = 1.0f - expf(-dt_s / range_tau_s);
    if (rms > hi_) hi_ = rms; else hi_ += (rms - hi_) * range_alpha;
    if (rms < lo_) lo_ = rms; else lo_ += (rms - lo_) * range_alpha;

    const float span = fmaxf(hi_ - lo_, hi_ * 0.35f);
    float v = span > 1e-4f ? (rms - lo_) / span : 0.0f;
    v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);

    const float tau = v > env_ ? attack_s : release_s;
    env_ += (v - env_) * (1.0f - expf(-dt_s / tau));
    return env_;
}

Microphone::Microphone(int32_t samp_size)
{
    sample_size = samp_size;

    // One-pole coefficients for the two corners of the filter bank.
    k_bass = 1.0f - expf(-2.0f * (float)M_PI * BASS_CORNER_HZ / SAMPLE_RATE_HZ);
    k_treble = 1.0f - expf(-2.0f * (float)M_PI * TREBLE_CORNER_HZ / SAMPLE_RATE_HZ);
    k_mel_lo = 1.0f - expf(-2.0f * (float)M_PI * MELODY_LOW_HZ / SAMPLE_RATE_HZ);
    k_mel_hi = 1.0f - expf(-2.0f * (float)M_PI * MELODY_HIGH_HZ / SAMPLE_RATE_HZ);

    // Set up two buffers for ping-ponging with DMA
    buffer_0 = new int32_t[sample_size];
    buffer_1 = new int32_t[sample_size];

    next_buffer_to_fill = buffer_1;
}

Microphone::~Microphone()
{
    delete[] buffer_0;
    delete[] buffer_1;

    dma_channel_unclaim(dma_chan);
}

void Microphone::init(PIO pio, uint sm, uint pin_bclk, uint pin_din)
{
    audio_input_init(pio, sm, pin_bclk, pin_din);
}

void Microphone::audio_input_init(PIO pio, uint sm, uint pin_bclk, uint pin_din)
{
    audio_pio = pio;
    audio_sm = sm;

    // Mark the state machine as taken so the CYW43 WLAN driver can't grab it.
    pio_sm_claim(pio, sm);

    // Load the PIO program
    uint offset = pio_add_program(pio, &i2s_microphone_mono_24_program);

    // Configure the state machine
    i2s_microphone_mono_24_program_init(
        pio,
        audio_sm,
        offset,
        pin_bclk,
        pin_din);

    // Configure DMA
    dma_chan = dma_claim_unused_channel(true);

    dma_channel_config dma_cfg =
        dma_channel_get_default_config(dma_chan);

    channel_config_set_transfer_data_size(
        &dma_cfg,
        DMA_SIZE_32);

    channel_config_set_read_increment(
        &dma_cfg,
        false);

    channel_config_set_write_increment(
        &dma_cfg,
        true);

    channel_config_set_dreq(
        &dma_cfg,
        pio_get_dreq(pio, audio_sm, false));

    dma_channel_configure(
        dma_chan,
        &dma_cfg,
        buffer_0,
        &audio_pio->rxf[audio_sm],
        sample_size,
        true);
}

AudioFrame Microphone::capture()
{
    // Wait until the current DMA transfer has completed
    dma_channel_wait_for_finish_blocking(dma_chan);

    int32_t* completed_buffer =
        (next_buffer_to_fill == buffer_1)
            ? buffer_0
            : buffer_1;

    // Restart DMA immediately on the other buffer
    dma_channel_set_write_addr(
        dma_chan,
        next_buffer_to_fill,
        true);

    // Swap buffers for next time
    next_buffer_to_fill =
        (next_buffer_to_fill == buffer_0)
            ? buffer_1
            : buffer_0;

    //return completed_buffer;
    AudioFrame frame;

    frame.samples = completed_buffer;
    int32_t* samples = frame.samples;

    frame.sample_count = sample_size;

    // Convert unsigned 24-bit samples to signed
    int64_t sum = 0;
    
    // Pass 1: Convert to signed and calculate mean
    for (int i = 0; i < frame.sample_count; i++)
    {
        // samples[i] -= 0x800000;

        // Shift left to force the 24th bit into the 32nd bit slot, 
        // then arithmetic shift right back down to sign-extend automatically.
        // The left shift happens on the unsigned copy, which is well defined for any value.
        samples[i] = (int32_t)((uint32_t)samples[i] << 8) >> 8;
    
        sum += samples[i];
    }

    // Calculate the DC offset (mean)
    frame.mean = sum / frame.sample_count;

    // Pass 2: Remove DC offset, find peak, and split the signal into three bands. The
    // samples are scaled down first: squaring 24-bit values overflows a float's precision.
    int32_t peak = 0;
    int64_t sum_of_squares = 0;
    float bass_sq = 0.0f, mid_sq = 0.0f, melody_sq = 0.0f, treble_sq = 0.0f;
    for (int i = 0; i < frame.sample_count; i++)
    {
        samples[i] -= frame.mean;
        if (abs(samples[i]) > peak)
            peak = abs(samples[i]);
        sum_of_squares += (int64_t)samples[i] * samples[i];

        const float x = (float)samples[i] * (1.0f / 65536.0f);
        lp_bass_1 += (x - lp_bass_1) * k_bass;
        lp_bass_2 += (lp_bass_1 - lp_bass_2) * k_bass;
        lp_split_1 += (x - lp_split_1) * k_treble;
        lp_split_2 += (lp_split_1 - lp_split_2) * k_treble;

        lp_mel_lo_1 += (x - lp_mel_lo_1) * k_mel_lo;
        lp_mel_lo_2 += (lp_mel_lo_1 - lp_mel_lo_2) * k_mel_lo;
        lp_mel_hi_1 += (x - lp_mel_hi_1) * k_mel_hi;
        lp_mel_hi_2 += (lp_mel_hi_1 - lp_mel_hi_2) * k_mel_hi;

        const float b = lp_bass_2;              // below the bass corner
        const float m = lp_split_2 - lp_bass_2; // between the two corners
        const float e = lp_mel_hi_2 - lp_mel_lo_2; // the melody window inside the mids
        const float t = x - lp_split_2;         // above the treble corner
        bass_sq += b * b;
        mid_sq += m * m;
        melody_sq += e * e;
        treble_sq += t * t;
    }

    // Calculate RMS
    frame.rms = sqrtf((float)sum_of_squares / frame.sample_count);

    // Seconds since the previous capture. The filters below are expressed in time, so
    // their response is the same whether the render loop runs at 60 or 140 frames per second.
    const uint64_t now_us = time_us_64();
    float dt = (last_capture_us == 0) ? 0.0f : (float)(now_us - last_capture_us) * 1e-6f;
    last_capture_us = now_us;
    if (dt > MAX_FRAME_GAP_S)
        dt = MAX_FRAME_GAP_S;
    frame.dt_s = dt;

    const float range_alpha = 1.0f - expf(-dt / RANGE_TRACK_TAU_S);
    const float level_keep = expf(-dt / LEVEL_DECAY_TAU_S);

    if (frame.rms > rms_max)
        rms_max = frame.rms;
    else
        rms_max += (frame.rms - rms_max) * range_alpha;

    if (frame.rms < rms_min)
        rms_min = frame.rms;
    else
        rms_min += (frame.rms - rms_min) * range_alpha;

    frame.level = (frame.rms - rms_min) / (rms_max - rms_min);

    // Clamp the adaptive range
    if (rms_min > 64000.0f)
        rms_min = 64000.0f;

    if (rms_max < 90000.0f)
        rms_max = 90000.0f;

    if (frame.level > smoothed_level)
        smoothed_level = frame.level;
    else
        smoothed_level = smoothed_level * level_keep;

    if (smoothed_level < 0.05)
        smoothed_level = 0.05f;

        
    if ((frame.level > smoothed_peak) and (frame.level > 0.5))
        smoothed_peak = frame.level;
    else
        smoothed_peak = smoothed_peak * level_keep;
        
    // The bands, each stretched and enveloped on its own.
    const float inv_n = 1.0f / (float)frame.sample_count;
    frame.bass = band_bass.update(sqrtf(bass_sq * inv_n), dt, RANGE_TRACK_TAU_S, BASS_ATTACK_S, BASS_RELEASE_S);
    frame.mid = band_mid.update(sqrtf(mid_sq * inv_n), dt, RANGE_TRACK_TAU_S, MID_ATTACK_S, MID_RELEASE_S);
    frame.melody = band_melody.update(sqrtf(melody_sq * inv_n), dt, RANGE_TRACK_TAU_S, MELODY_ATTACK_S, MELODY_RELEASE_S);
    frame.treble = band_treble.update(sqrtf(treble_sq * inv_n), dt, RANGE_TRACK_TAU_S, TREBLE_ATTACK_S, TREBLE_RELEASE_S);

    // A build-up: the last few seconds standing above the last half minute.
    rise_fast_ += (frame.level - rise_fast_) * (1.0f - expf(-dt / RISE_FAST_TAU_S));
    rise_slow_ += (frame.level - rise_slow_) * (1.0f - expf(-dt / RISE_SLOW_TAU_S));
    frame.rise = std::clamp((rise_fast_ - rise_slow_) * 3.0f, 0.0f, 1.0f);

    // Silence: slow to believe, quick to leave, so one quiet bar does not darken the jelly.
    const float q = frame.level < QUIET_LEVEL ? 1.0f : 0.0f;
    const float q_tau = q > quiet_ ? QUIET_ENTER_TAU_S : QUIET_LEAVE_TAU_S;
    quiet_ += (q - quiet_) * (1.0f - expf(-dt / q_tau));
    frame.quiet = quiet_;

    // The pulse of the track.
    tempo_.update(frame.bass, dt);
    frame.tempo_phase = tempo_.phase();
    frame.tempo_bpm = tempo_.bpm();
    frame.tempo_conf = tempo_.confidence();

    //copy persistant mic data for returrn in frame
    frame.rms_min = rms_min;
    frame.rms_max = rms_max;
    frame.smoothed_peak = smoothed_peak;
    frame.smoothed_level = smoothed_level;
   
    return frame;

}

int Microphone::get_sample_size() const
{
    return sample_size;
}

