#pragma once
#include <cstdint>

// Detects beats in the microphone level: a sharp rise to a high level, not too soon after
// the previous one. Call update() once per frame with the instantaneous level.
class BeatDetector
{
public:
    static constexpr float MIN_LEVEL = 0.5f;  // level a frame needs to count as a beat...
    static constexpr float MIN_RISE = 0.15f;  // ...and how much it must have jumped since the last frame
    static constexpr float HOLDOFF_S = 0.25f; // no two beats closer than this

    // Returns true on the frame a beat is detected.
    bool update(float level);

private:
    uint64_t last_us = 0;
    float last_level = 0.0f;
    float since_beat_s = 1000.0f;
};
