#pragma once
#include "jell_led.hpp"
#include "jell_pmw_light.hpp"


struct Point3
{
    float x;
    float y;
    float z;
};

class Canvas
{
public:
    Canvas(
        LedString& ring,
        LedString* spokes,
        PwmLight* noodles);

        Point3 ring_position(int pixel) const;
        Point3 spoke_position(int spoke, int pixel) const;

        void clear();

        void show();

        // Let everything drawn so far fade with time constant tau_s, given dt_s seconds
        // have passed since the last frame. Only effects that want afterglow call this.
        void fade(float dt_s, float tau_s);

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
};