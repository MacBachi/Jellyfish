#include "jell_beat.hpp"
#include <algorithm>
#include "pico/time.h"

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
