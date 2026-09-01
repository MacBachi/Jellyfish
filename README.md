# Playground fork of the Sheffield-by-the-Sea Jellyfish

> **This is not the original project.** It is a personal fork for playing around with the firmware.
>
> The real thing lives at **https://github.com/Sheffield-by-the-sea/Jellyfish**. All credit for the idea, the
> design, the 3D models, the circuit board and the firmware goes to Alex and the Sheffield-by-the-Sea crew.
> If you want to build a Jellyfish, start from the original repository. Nothing in this fork has been
> reviewed, endorsed or supported by the original authors, and firmware built by this fork's CI is not
> official firmware.
>
> See [How this fork differs from the original](#how-this-fork-differs-from-the-original) for what has
> been changed here.

---

# The Sheffield-by-the-Sea EMF2026 audio-reactive light-up Jellyfish! 
Everything you need to print parts, order circuit boards, and flash the firmware is here. 

![Completed Jellies](images/JellyBloomTest.jpeg)

There is an excellent write-up on the project over at [Pimoroni Learn](https://learn.pimoroni.com/article/building-sound-reactive-jellyfish)


## About the project 

These Jellyfish are the brainchild of Alex ([Mastodon](https://mastodon.social/@GlitchEngine), [GitHub](https://github.com/AlexJMcIntyre)) and were created with a little help (and a not so little amount of sewing) from friends. 

They were created for display at [Electromagnetic Field](https://www.emfcamp.org) but would look good basically anywhere.

This project contains everything you need to make a Jelly of your very own, including the 3D print files, firmware and optional (but encouraged) circuit boards. 

## Hardware

As this is microcontroller code it is very dependent on the hardware, which in this case is the [Raspberry Pi Pico 2 W
](https://shop.pimoroni.com/products/raspberry-pi-pico-2-w?variant=54852252991867).

We don't use the wireless functionality of Pico 2 W in this project so we could have used the non-wireless version. However, using the 'W' version means we have the option to repurpose the Jellyfish as smart lighting, notification lights or other fun connected projects after the festival.

## How this fork differs from the original

Only the C++ firmware in `firmware_cpp/` is being changed here. The 3D print files, the PCB and the
bill of materials are exactly as in the original repository.

Changes so far, relative to the original `main`:

- **All seven tentacle headers are driven.** The firmware always outputs to NeoPix2..NeoPix8 (GPIO 3..9),
  with a fixed maximum of 16 LEDs per tentacle. Headers without a strip simply send into nothing, so one
  firmware works whether four or seven tentacles are plugged in.
- **New display mode "Drops".** On a detected beat a bright head runs down every tentacle and the ring
  flashes. Reachable with the mode buttons, one step after the default mode.
- **Afterglow is explicit.** The brightness decay that used to happen silently inside the LED driver is now
  `Canvas::fade(dt, tau)`, time-based and only used by effects that ask for it. Existing modes look the same.
- **Preparation for WLAN.** LED strips and the microphone claim their PIO state machines, and the WS2812
  program is loaded once per PIO block, so the CYW43 driver can take a free state machine later.
- **Audio smoothing in seconds instead of frames**, so the response no longer depends on how many LEDs
  are configured.
- **Small fixes.** An out-of-bounds read in the default mode, a noodle index bug in the ambient effect that
  only showed with more than four tentacles, a clamp on the noodle PWM level, non-copyable driver classes.
- **WLAN between jellies.** One jelly becomes the access point, the others join it, and everything is kept in
  step: display mode, brightness, hue offset, the animation clock, and beats. Buttons on any jelly switch all
  of them. See below for how to use it.
- **Two palette modes.** Every jelly gets its own base colour from an 8-entry palette (the AP hands out colour
  slots as jellies join); the cycle variant rotates all jellies one colour further every 10 s with a smooth
  blend, in sync.
- **Identify.** A command makes the AP jelly blink red three times while all others blink blue in the same
  rhythm, so you can tell which one runs the network.

Planned next: a small web page served by the AP jelly for phones (runtime control only, never persistent
configuration changes), and re-election details.

### Using the WLAN

Every jelly runs the same firmware. On power-up it listens for the jelly network for a random 10 to 120
seconds. If it hears one it joins as a station; if not, it listens through one more scan and then becomes
the access point itself. So: switch the first jelly on, wait for its onboard LED to go solid, then switch
on the rest. If the AP jelly disappears, the others keep their last state and start a new election.

- Network name is the jellyfish emoji 🪼, password `FroschUndMaus`. Both can be changed at build time with
  the compile definitions `JELL_WIFI_SSID` and `JELL_WIFI_PASSWORD` (see `jell_config.hpp`).
- Joining the network is the only access control. Anyone on it can control the bloom.
- Onboard LED: fast blink = looking for a network, solid = access point, slow blink = station.

Control is one plain-text line per UDP datagram on port 4210. From a laptop on the jelly network:

```
nc -u 192.168.4.1 4210          # then type commands, one per line
nc -lu 4210                     # in a second terminal: watch heartbeats, beats and replies
```

| Command | Effect |
|---|---|
| `MODE n`, `NEXT`, `PREV` | switch the display mode on every jelly |
| `BRIGHT 0..1` | overall brightness |
| `HUE deg` | shift every colour by this many degrees |
| `CYCLE s` | period of the palette cycle mode |
| `IDENT` | AP blinks red three times, all others blue |
| `HELLO` | roll call: every jelly answers with its id, role, colour slot and IP |
| `BEAT` | trigger a beat on all jellies (for testing the drops mode) |

Modes in button order: mic level check, LED channel test, mic field (default), drops, palette, palette cycle,
ambient rainbow, ambient deep sea.

The USB serial console prints the election, role changes, every command sent or received, and the time
offset a station keeps to the AP's clock.

## License

This project is licensed under the MIT License - see the LICENSE.md file for details

## Acknowledgments

- [Pimoroni](https://shop.pimoroni.com) - for providing us with a few parts salvaged from their floor 