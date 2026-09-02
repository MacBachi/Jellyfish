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

// --- The calm modes. All of them are slow; `time` is the shared master time. ---

// A 6 s pulse that starts at the ring and runs down the tentacles.
void effect_breathe(Canvas& canvas, float time);

// Near dark, with sparse cyan/green sparks that glow up and fade.
void effect_glimmer(Canvas& canvas);

// Slowly drifting bands of green, teal and violet over the ring; tentacles dimmer.
void effect_aurora(Canvas& canvas, float time);

// A gentle brightness wave travelling up the tentacles.
void effect_current(Canvas& canvas, float time);

// Warm amber with a barely-there candle flicker.
void effect_lantern(Canvas& canvas, float time);

// Very dim, cool blue-white, hardly moving. A night light.
void effect_moonlight(Canvas& canvas, float time);

// Single slow drops trickling down the tentacles with trails, no beat needed.
void effect_drizzle(Canvas& canvas);

// Single tentacle LEDs rising and falling slowly in yellow-green.
void effect_fireflies(Canvas& canvas);

// A slow pulse that visits one jelly after the other in colour-slot order,
// on each jelly's palette colour. Needs the shared master time.
void effect_swarm(Canvas& canvas, float time, int slot);

// Slow microphone response: brightness follows the sound level over seconds,
// the colour drifts from cool when quiet to warm when lively.
void effect_whisper(Canvas& canvas, const AudioFrame& audio, float time);
    
void effect_ambientNField(Canvas& canvas, float time, float noisescale, float huebase, float huerange, float timescale);