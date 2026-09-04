# Changelog

All notable changes to JellyFloatOS. The project continues the Sheffield-by-the-Sea Jellyfish firmware;
entries start where the fork diverged.

## Unreleased

### Added
- The roster, on the web page and in the app's Jellies tab, shows each jelly's address as a link to that
  jelly's own web page.

## 1.0.0 - 2026-09-04

First stable release: firmware and iPhone app under one version number, the app prepared for the
App Store, the project on its own (the GitHub fork relation to the original repository is being cut).

### Added
- Find My: every jelly can be an OpenHaystack tag. `tools/findmy/keys.html` makes the P-224 key pair in
  the browser; a button press within 60 s after boot opens a ten-minute provisioning window; the web page
  writes the public key (or a "never" sentinel) once into a reserved flash sector that updates leave
  alone; the jelly then advertises over BLE through BTstack. `JellyFloatReset.uf2`, a separate image on
  every release, erases the sector. The linker script keeps the program out of the last four sectors.
- `firmware_cpp/tools/power_sim`: builds the effect code for the host and prints the supply current of
  every mode for three measured WS2812B models.
- The iPhone app speaks German as well as English (string catalogues in `ios/JellyFloat/Resources/`).
- The join prompt offers the demo bloom to people without hardware, and to App Review.
- App Store material: `ios/AppStore/` with the listing texts in both languages, screenshots, the
  submission guide and the reviewer notes; `PRIVACY.md`; `ios/scripts/archive.sh` archives and uploads.

### Changed
- The app's `CFBundleShortVersionString` is the numeric part of `VERSION` (App Store Connect rejects
  `1.0.1-dev`); the full string moves to `JellyVersion`, which the app shows and compares.
- The release job tolerates a second CI run for the same tag and refreshes the files instead of failing.
- The iPhone app is developed again, alongside the web page; 0.9.0 had set it aside.

### Removed
- The release tags inherited from the original repository (`v1.0.0` to `v1.0.5`, `V1.0`, `1.0.1`) belong to that project and are dropped here; locally they live on as `upstream/<tag>`. For the record: 1.0.1=fbf8cde, V1.0=c9277e3, v1.0.0=fbf8cde, v1.0.2=384fc7f, v1.0.3=f8b9598, v1.0.4=7e49f25, v1.0.5=d2adb6b.
- README: the power notes now rest on the KiCad file, measured LED currents and the simulation; the
  calm modes peak at 1 to 2.6 A at full brightness, not "2–15 %". A quick-start section for putting
  the current release onto a jelly without installing anything.

## 0.9.0 - 2026-09-04

The web page replaces the iPhone app; verified on jelly 0451.

### Added
- A web page on every jelly (port 80, `http://192.168.4.1` on the one that runs the network): the
  jelly's own LEDs drawn live as a jellyfish, mode tiles, brightness, colour shift and cycle, Identify,
  roll call, the roster with firmware versions and missing modes. It only sends runtime commands.
  `GET /api/state.json`, `GET /api/frame.bin` and `POST /api/cmd` behind it.
- `firmware_cpp/tools/elf_size.py` reports flash and RAM use; the CI adds it to the job summary.

### Changed
- lwIP gets a little more room (8 KB heap, 16 packet buffers, 8 TCP connections) for the page.
- The iPhone app in `ios/` is superseded by the web page and no longer developed.

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
