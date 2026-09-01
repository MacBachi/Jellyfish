#pragma once
#include "jell_canvas.hpp"
#include "jell_audio.hpp"
#include "jell_field.hpp"

void effect_miclevelCheck(
    Canvas& canvas,
    const AudioFrame& audio);

void effect_LEDchanneltest(Canvas& canvas);

void effect_micNField(Canvas& canvas, const AudioFrame& audio, float time);

// Beat-triggered drops running down the tentacles, with a ring flash. Uses Canvas::fade
// for the trails, so it only ever draws the head of each drop. `beat` is true on the frame
// a beat happened, detected locally or received from the network.
void effect_micDrops(Canvas& canvas, const AudioFrame& audio, float time, bool beat);

// Base hue of this jelly in the palette modes, from its colour slot (AP = 0). With
// `cycle` set, every jelly moves one palette step further every cycle_period_s seconds,
// blending smoothly during the last JellConfig::CYCLE_BLEND_S seconds of each period.
// `time` must be the shared master time so all jellies switch together.
float palette_hue(int slot, float time, float cycle_period_s, bool cycle);

// The palette modes: the ambient noise animation around one base hue.
void effect_palette(Canvas& canvas, float time, float hue);
    
void effect_ambientNField(Canvas& canvas, float time, float noisescale, float huebase, float huerange, float timescale);