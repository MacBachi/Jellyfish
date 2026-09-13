#include "jell_beat.hpp"
#include <algorithm>
#include "pico/time.h"
#include <cmath>

void TempoTracker::update(float bass, float dt_s)
{
    since_onset_s_ += dt_s;
    phase_ += dt_s / period_s_;
    phase_ -= floorf(phase_);

    const bool onset = bass > ONSET_LEVEL && (bass - last_bass_) > ONSET_RISE && since_onset_s_ > MIN_PERIOD_S;
    last_bass_ = bass;

    if (!onset)
    {
        // Without beats the belief in the pulse fades, but the phase keeps turning.
        conf_ -= conf_ * (1.0f - expf(-dt_s / 8.0f));
        return;
    }

    const float interval = since_onset_s_;
    since_onset_s_ = 0.0f;

    if (interval <= MAX_PERIOD_S)
    {
        // How well does this interval fit what we believe? A good fit moves the period a
        // long way, a poor one barely at all, so a stray kick cannot throw the tempo off.
        const float err = interval - period_s_;
        const float match = 1.0f - std::min(fabsf(err) / period_s_, 1.0f);
        period_s_ = std::clamp(period_s_ + err * (0.15f + 0.35f * match), MIN_PERIOD_S, MAX_PERIOD_S);
        conf_ += (match - conf_) * 0.25f;
    }

    // Pull the phase towards the beat instead of snapping it there: the picture stays smooth.
    const float ph_err = phase_ > 0.5f ? phase_ - 1.0f : phase_;
    phase_ -= ph_err * 0.5f;
    if (phase_ < 0.0f)
        phase_ += 1.0f;
}

bool BeatDetector::update(float level)
{
    const uint64_t now_us = time_us_64();
    const float dt = (last_us == 0) ? 0.0f : (float)(now_us - last_us) * 1e-6f;
    last_us = now_us;
    since_beat_s += std::min(dt, 0.1f);

    const bool beat = level > MIN_LEVEL
        && (level - last_level) > MIN_RISE
        && since_beat_s > HOLDOFF_S;
    last_level = level;

    if (beat)
        since_beat_s = 0.0f;

    return beat;
}
