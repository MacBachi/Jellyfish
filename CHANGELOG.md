# Changelog

All notable changes to JellyFloatOS. The project continues the Sheffield-by-the-Sea Jellyfish firmware;
entries start where the fork diverged.

## 0.1.0 - 2026-09-02

First release under the JellyFloatOS name.

### Added
- WLAN between jellies: automatic access point election (random 10–120 s listen, final scan, then AP),
  DHCP server on the AP, re-election when the AP disappears.
- UDP text protocol on port 4210: `MODE`, `NEXT`, `PREV`, `BRIGHT`, `HUE`, `CYCLE`, `BEAT`, `IDENT`,
  `HELLO`, `SLOT`, `STATE` heartbeat with the AP's clock for time sync.
- Sync of mode, brightness, hue offset, cycle period, animation clock and beats across the bloom;
  buttons on any jelly act on all.
- Identify command: the AP blinks red three times, all others blue, in step.
- Colour slots per jelly and the modes Palette and Palette cycle.
- Ten calm modes: Breathe, Glimmer, Aurora, Current, Lantern, Moonlight, Drizzle, Fireflies, Swarm, Whisper.
- Playlist mode cycling through the calm modes on the shared clock.
- Crossfade on every mode change.
- Beat-triggered Drops mode with explicit, time-based afterglow.
- Runtime brightness and hue offset applied at output time.
- All seven tentacle headers driven, 16 LEDs per tentacle maximum.

### Changed
- Firmware renamed from JellyOS to JellyFloatOS; releases are cut from `v*` tags only.
- Calm modes first in the mode order, test modes last.
- Audio smoothing and the LED channel test run on wall-clock time instead of frame counts.
- WS2812 PIO program loaded once per PIO block; state machines are claimed.

### Fixed
- Out-of-bounds read in the default mode's tentacle loop.
- Noodle index bug in the ambient effect with more than four tentacles.
- Noodle PWM level clamped; driver classes made non-copyable; sign extension of microphone samples.

### Removed
- The abandoned MicroPython firmware and the stale committed UF2 binary.
