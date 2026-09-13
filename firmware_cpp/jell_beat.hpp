#pragma once
#include <cstdint>

// Follows the pulse of the music: every bass onset is an interval, and the tracker pulls a
// running period and a phase towards them, like a loop that locks on. What comes out is a
// phase that keeps turning between the beats and through a gap in the music, so effects can
// breathe with the track instead of twitching at every kick.
class TempoTracker
{
public:
    static constexpr float MIN_PERIOD_S = 0.40f; // 150 BPM
    static constexpr float MAX_PERIOD_S = 1.20f; // 50 BPM
    static constexpr float ONSET_LEVEL = 0.45f;  // how loud the bass band must be...
    static constexpr float ONSET_RISE = 0.12f;   // ...and how much it must have jumped

    // Call once per audio frame with the bass envelope.
    void update(float bass, float dt_s);

    float phase() const { return phase_; }        // 0..1, zero on the beat
    float bpm() const { return 60.0f / period_s_; }
    float confidence() const { return conf_; }    // 0 = no pulse found, 1 = steady

private:
    float last_bass_ = 0.0f;
    float since_onset_s_ = 0.0f;
    float period_s_ = 0.6f;
    float phase_ = 0.0f;
    float conf_ = 0.0f;
};

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
