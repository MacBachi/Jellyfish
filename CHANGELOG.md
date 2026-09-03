# Changelog

All notable changes to JellyFloatOS. The project continues the Sheffield-by-the-Sea Jellyfish firmware;
entries start where the fork diverged.

## Unreleased

## 0.5.0 - 2026-09-03

First release with the JellyFloat iPhone app and the shared version number.

### Added
- Per-build options `JELL_RING_LEDS`, `JELL_RING_ORDER`, `JELL_TENTACLE_ORDER`, `JELL_WIFI_SSID`,
  `JELL_WIFI_PASSWORD` as CMake variables and as inputs of the CI workflow's manual run, so one source
  tree serves rings of any size and strips of any colour order.
- Colour orders RBG, BRG and BGR in the LED driver.
- Releases carry two firmware files: the default ring and the 39-LED GRB ring with RGB tentacles.
- One version number for firmware and app, kept in `VERSION`. The firmware compiles it in, prints it at
  boot and announces it with its mode count in every `HELLO`; the app stamps it into its build.
- The app shows each jelly's firmware version against the newest it knows, flags modes that not every
  jelly has (on the tile and under the mode name), and says which mode an older jelly shows instead.
  Mixed versions keep working; nothing is refused.
- The AP answers an app's `HELLO` with the whole roster and passes jelly `HELLO`s on to apps; stations
  say hello every 30 s so the roster stays fresh.
- SOS mode: red Morse "SOS" on the shared clock (mode 19).
- JellyFloat iOS app in `ios/`: mode tiles, brightness/hue/cycle, roll call and identify, Wi-Fi
  joining with remembered network, and the phone as a virtual jelly rendering the same effects.
- App subscribers: `HELLO <id> APP ...` registers a phone for unicast copies of every line the AP
  sends, plus a `LEVEL` microphone stream, so iOS apps work without the multicast entitlement.

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
