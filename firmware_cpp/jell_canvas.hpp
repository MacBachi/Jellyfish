#pragma once
#include "jell_led.hpp"
#include "jell_pmw_light.hpp"
#include "jell_config.hpp"


struct Point3
{
    float x;
    float y;
    float z;
};

// One frame buffer for the whole jelly: the ring, the tentacles and the noodles.
// Effects only draw into it; core 1 calls show() once per frame.
class Canvas
{
public:
    Canvas(
        LedString& ring,
        LedString* spokes,
        PwmLight* noodles);

        Point3 ring_position(int pixel) const;
        Point3 spoke_position(int spoke, int pixel) const;

        // Black in the buffers; takes effect with the next show().
        void clear();

        // Push the frame to the hardware. mix < 1 blends from the picture frozen by
        // begin_crossfade() (0) towards the current buffer (1).
        void show(float mix = 1.0f);

        // Freeze what is currently shown so the following frames can blend away from it.
        // current_mix is the blend position of the frame on the LEDs right now (1 if no
        // crossfade is running). The live buffers start over from black.
        void begin_crossfade(float current_mix = 1.0f);

        // Let everything drawn so far fade with time constant tau_s, given dt_s seconds
        // have passed since the last frame. Only effects that want afterglow call this.
        void fade(float dt_s, float tau_s);

        // Global brightness (0..1) and hue offset (degrees), applied when pixels and noodle
        // levels go to the hardware. The stored HSV values are untouched, so fade() and
        // trails are unaffected. Set once per frame from the shared state.
        void set_global(float brightness, float hue_offset);

        void ring_pixel_hsv(
            int pixel,
            float h,
            float s,
            float v);

        void spoke_pixel_hsv(
            int spoke,
            int pixel,
            float h,
            float s,
            float v);

        void noodle_level(
            int noodle,
            float level);

        Point3 noodle_position(int noodle);

        void all_pixels_hsv(
            float h,
            float s,
            float v);

        void all_noodles_level(
            float level);

    private:
        LedString& ring;
        LedString* spokes;
        PwmLight* noodles;

        Point3 bounds_max;
        Point3 bounds_min;

        float brightness_ = JellConfig::BRIGHTNESS_MODIFIER;
        float hue_offset_ = 0.0f;

        float noodle_levels_[JellConfig::NUMBER_OF_NOODLES] = {};
        float noodle_snapshot_[JellConfig::NUMBER_OF_NOODLES] = {};
};
