#include "jell_canvas.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>

#include "hardware/sync.h" // __dmb
#include "jell_config.hpp"

Canvas::Canvas(
    LedString& ring,
    LedString* spokes,
    PwmLight* noodles)
    : ring(ring),
      spokes(spokes),
      noodles(noodles)
{
    bounds_min = {ring.get_min_x(), ring.get_min_y(), ring.get_min_z()};
    bounds_max = {ring.get_max_x(), ring.get_max_y(), ring.get_max_z()};

    for (int s = 0; s < JellConfig::NUMBER_OF_TENTACLES; s++)
    {
        bounds_min.x = std::min(bounds_min.x, spokes[s].get_min_x());
        bounds_min.y = std::min(bounds_min.y, spokes[s].get_min_y());
        bounds_min.z = std::min(bounds_min.z, spokes[s].get_min_z());
        bounds_max.x = std::max(bounds_max.x, spokes[s].get_max_x());
        bounds_max.y = std::max(bounds_max.y, spokes[s].get_max_y());
        bounds_max.z = std::max(bounds_max.z, spokes[s].get_max_z());
    }

    for (int n = 0; n < JellConfig::NUMBER_OF_NOODLES; n++)
    {
        bounds_min.x = std::min(bounds_min.x, noodles[n].get_x());
        bounds_min.y = std::min(bounds_min.y, noodles[n].get_y());
        bounds_min.z = std::min(bounds_min.z, noodles[n].get_z());
        bounds_max.x = std::max(bounds_max.x, noodles[n].get_x());
        bounds_max.y = std::max(bounds_max.y, noodles[n].get_y());
        bounds_max.z = std::max(bounds_max.z, noodles[n].get_z());
    }
}

Point3 Canvas::ring_position(int pixel) const
{
    return {
        ring.get_x(pixel),
        ring.get_y(pixel),
        ring.get_z(pixel)
    };
}

Point3 Canvas::spoke_position(int spoke, int pixel) const
{
    return {
        spokes[spoke].get_x(pixel),
        spokes[spoke].get_y(pixel),
        spokes[spoke].get_z(pixel)
    };
}

void Canvas::show(float mix)
{
    mix = std::clamp(mix, 0.0f, 1.0f);

    ring.paint_string(brightness_, hue_offset_, mix);

    for (int i = 0; i < JellConfig::NUMBER_OF_TENTACLES; i++)
        spokes[i].paint_string(brightness_, hue_offset_, mix);

    for (int n = 0; n < JellConfig::NUMBER_OF_NOODLES; n++)
    {
        const float level = noodle_snapshot_[n] + (noodle_levels_[n] - noodle_snapshot_[n]) * mix;
        noodles[n].set_level(level * brightness_);
        noodle_out_[n] = (uint8_t)(std::clamp(level * brightness_, 0.0f, 1.0f) * 255.0f);
    }

    publish_frame();
}

void Canvas::publish_frame()
{
    frame_seq_ = frame_seq_ + 1; // odd: writing
    __dmb();
    uint8_t* p = frame_;
    memcpy(p, ring.last_rgb(), ring.count() * 3);
    p += ring.count() * 3;
    for (int i = 0; i < JellConfig::NUMBER_OF_TENTACLES; i++)
    {
        memcpy(p, spokes[i].last_rgb(), spokes[i].count() * 3);
        p += spokes[i].count() * 3;
    }
    memcpy(p, noodle_out_, JellConfig::NUMBER_OF_NOODLES);
    __dmb();
    frame_seq_ = frame_seq_ + 1; // even: complete
}

void Canvas::copy_frame(uint8_t* dst) const
{
    // A frame is written every ~15 ms and copied in a few microseconds, so one retry is
    // plenty; after that a torn frame is still a picture, just of two adjacent moments.
    for (int attempt = 0; attempt < 3; attempt++)
    {
        const uint32_t before = frame_seq_;
        __dmb();
        memcpy(dst, frame_, FRAME_BYTES);
        __dmb();
        if ((before & 1u) == 0 && frame_seq_ == before)
            return;
    }
}

void Canvas::begin_crossfade(float current_mix)
{
    current_mix = std::clamp(current_mix, 0.0f, 1.0f);

    ring.snapshot(current_mix);
    ring.off();

    for (int i = 0; i < JellConfig::NUMBER_OF_TENTACLES; i++)
    {
        spokes[i].snapshot(current_mix);
        spokes[i].off();
    }

    for (int n = 0; n < JellConfig::NUMBER_OF_NOODLES; n++)
    {
        noodle_snapshot_[n] = noodle_snapshot_[n] + (noodle_levels_[n] - noodle_snapshot_[n]) * current_mix;
        noodle_levels_[n] = 0.0f;
    }
}

void Canvas::set_global(float brightness, float hue_offset)
{
    brightness_ = std::clamp(brightness, 0.0f, 1.0f) * JellConfig::BRIGHTNESS_MODIFIER;
    hue_offset_ = fmodf(hue_offset, 360.0f);
    if (hue_offset_ < 0.0f)
        hue_offset_ += 360.0f;
}

void Canvas::fade(float dt_s, float tau_s)
{
    const float factor = expf(-dt_s / tau_s);

    ring.fade(factor);

    for (int i = 0; i < JellConfig::NUMBER_OF_TENTACLES; i++)
        spokes[i].fade(factor);
}

void Canvas::clear()
{
    ring.off();
    for (int i = 0; i < JellConfig::NUMBER_OF_TENTACLES; i++)
        spokes[i].off();
    for (int n = 0; n < JellConfig::NUMBER_OF_NOODLES; n++)
        noodle_levels_[n] = 0.0f;
}

void Canvas::ring_pixel_hsv(int pixel, float h, float s, float v)
{
    ring.write_pixel_hsv(pixel, h, s, v);
}

void Canvas::all_pixels_hsv(float h, float s, float v)
{
    for (int i = 0; i < JellConfig::NUMBER_LEDS_IN_RING; i++)
    {
        ring.write_pixel_hsv(i, h, s, v);
    }
    for (int i = 0; i < JellConfig::NUMBER_OF_TENTACLES; i++)
    {
        for (int j = 0; j < JellConfig::NUMBER_LEDS_IN_EACH_TENTACLE; j++)
        {
            spokes[i].write_pixel_hsv(j, h, s, v);
        }
    }
}

void Canvas::all_noodles_level(float level)
{
    for (int i = 0; i < JellConfig::NUMBER_OF_NOODLES; i++)
    {
        noodle_levels_[i] = std::clamp(level, 0.0f, 1.0f);
    }
}

void Canvas::spoke_pixel_hsv(
    int spoke,
    int pixel,
    float h,
    float s,
    float v)
{
    spokes[spoke].write_pixel_hsv(pixel, h, s, v);
}

void Canvas::noodle_level(int noodle, float level)
{
    if (noodle >= 0 && noodle < JellConfig::NUMBER_OF_NOODLES)
        noodle_levels_[noodle] = std::clamp(level, 0.0f, 1.0f);
}

Point3 Canvas::noodle_position(int noodle)
{
    return {
        noodles[noodle].get_x(),
        noodles[noodle].get_y(),
        noodles[noodle].get_z()
    };
}
