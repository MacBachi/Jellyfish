# App Review notes

The English text below answers Apple's Guideline 2.1 "Information Needed" questionnaire, which
every new developer account gets on its first submission. It belongs in two places: the reply to
the review message, and the "Notes" field of App Review Information, where it stays for later
submissions. The screen recording Apple asks for is made on the iPhone itself, see the shot list
at the end.

---

## 1. Screen recording

Attached. It was captured on a physical iPhone and starts with launching the app.

The app has no account registration, no login, no account deletion, no user-generated content and
no paid content or features, so none of those flows appear in the recording.

## 2. Purpose and target audience

JellyFloat is the remote control for hand-built LED jellyfish lamps. The lamps are a maker project:
a Raspberry Pi Pico 2 W microcontroller with addressable LED strips, running open-source firmware
called JellyFloatOS. The firmware, the circuit board and the 3D-printed parts are published at
github.com/MacBachi/Jellyfish, and this app is part of that project.

The lamps have only two small buttons, and several of them run as a group over their own Wi-Fi
network, in step with each other. The app replaces the buttons: it shows the whole group, lets the
user pick one of twenty lighting modes, set brightness, shift the colours and change how fast the
colour palette rotates, and every lamp follows at once. It also shows which lamp is which, and what
firmware each of them runs.

Target audience: people who have built or bought one of these lamps, and makers interested in the
project. The demo mode described below lets anyone try the app without hardware.

## 3. Setting up and accessing the main features

No account, no login, no credentials, no sample files. Everything in the app is reachable
immediately after launch.

Without any hardware, which is how the reviewer can see all features:

1. Launch the app. A sheet appears: "Join the jelly network?"
2. Tap "No jellyfish yet? Try the demo bloom" at the bottom of that sheet. (The same switch is in
   Settings, "Demo bloom (no hardware)".)
3. The status pill at the top right reads "Demo bloom". The app now talks to a simulated group of
   lamps that lives inside the app and speaks the same protocol as the real ones.
4. Bloom tab: a jellyfish is drawn on screen and animates. Tap any mode tile to change the
   animation; drag the brightness and colour sliders; use the stepper for the colour cycle.
5. Jellies tab: the simulated group (0451, 1b3c, c7d1, 9e02) with colour slot, address and firmware
   version. "Roll call" asks who is there, "Identify" makes the lamps blink so the user can tell
   them apart; on screen the drawn jellyfish blinks.
6. Settings tab: which Wi-Fi network to join, when to join it, the size of the on-screen jellyfish,
   the demo switch, and version information.

With real hardware, the only difference is the first step: "Join" asks iOS to switch Wi-Fi to the
lamps' network. iOS shows its own confirmation dialog first (the app uses NEHotspotConfiguration,
which is why the app has the Hotspot Configuration entitlement). On the first command, iOS also
asks for local network permission. Both prompts are expected and appear in the recording only if
hardware is present.

The app is localised in English and German and follows the device language.

## 4. External services, tools and platforms

None. The app uses no third-party SDK, no analytics, no advertising, no authentication service, no
payment processor, no AI service and no server of ours. There is no backend at all.

The app never connects to the internet. It only sends short lines of text over UDP to the lamps at
192.168.4.1, port 4210, inside the local Wi-Fi network the lamps open themselves. Apart from that
it uses two Apple system frameworks: NetworkExtension to ask iOS to join that Wi-Fi network, and
Network for the UDP socket. Settings are kept in UserDefaults on the device; if the user chooses to
remember a custom Wi-Fi password, it goes into the device Keychain and nowhere else.

Consequently the app collects no data, which is what the privacy questionnaire declares.

## 5. Regional differences

None. The app behaves identically in every region and every storefront. There is no geographically
restricted content, no region-specific feature and no server that could differ by region. The only
regional adaptation is the interface language: German on German-language devices, English
everywhere else.

## 6. Regulated industry, protected material

The app is not part of a regulated industry and contains no protected third-party material. All
code and artwork in the app were written by the developer. The wider project it belongs to,
including the lamp design it controls, is open source under the MIT licence and continues the
Sheffield-by-the-Sea Jellyfish project, which is credited in the app description and in the
repository, as the MIT licence requires.

---

## Shot list for the recording

Roughly ninety seconds, recorded on the iPhone with the system screen recorder (Control Centre),
portrait, no cuts:

1. Home screen, tap the JellyFloat icon, let it launch.
2. On the sheet, tap "No jellyfish yet? Try the demo bloom". Wait until the jellyfish animates.
3. Tap three different mode tiles, pausing a moment on each, so the animation visibly changes.
4. Drag the brightness slider down and up, then the colour slider.
5. Tap the Jellies tab. Tap "Roll call", then "Identify"; wait for the blinking.
6. Tap the Settings tab and scroll to the bottom.
7. If a real lamp is at hand: switch off "Demo bloom", go back, tap "Join", accept the iOS dialogs,
   and change a mode with the lamp in shot.
