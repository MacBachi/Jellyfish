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

Planned next: enabling the Pico 2 W's WLAN, then looking at more strips.

## License

This project is licensed under the MIT License - see the LICENSE.md file for details

## Acknowledgments

- [Pimoroni](https://shop.pimoroni.com) - for providing us with a few parts salvaged from their floor 