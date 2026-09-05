# App Review notes

The English text below answers Apple's Guideline 2.1 "Information Needed" questionnaire, which
every new developer account gets on its first submission. It belongs in two places: the reply to
the review message, and the "Notes" field of App Review Information, where it stays for later
submissions. The screen recording Apple asks for is made on the iPhone itself, see the shot list at the end.
Both fields take at most 4000 characters, which the text below stays under.

---

## 1. Screen recording

Attached, captured on a physical iPhone, starting with the app launch. The app has no account, no login, no account deletion, no user-generated content and no paid features, so none of those flows appear.

## 2. Purpose and target audience

JellyFloat is the remote control for hand-built LED jellyfish lamps: a Raspberry Pi Pico 2 W with addressable LED strips, running our open-source firmware (github.com/MacBachi/Jellyfish). The lamps have two small buttons, and several of them run as one synchronised group over their own Wi-Fi network. The app replaces the buttons: it shows the whole group and offers twenty lighting modes, brightness, a colour shift and the palette speed, with every lamp following at once. Audience: people who built or bought such a lamp, and makers interested in the project. The demo mode below lets anyone try the app without hardware.

## 3. Setting up and accessing the main features

No account, no login, no credentials, no sample files. To see everything without hardware:

1. Launch the app. A sheet appears: "Join the jelly network?"
2. Tap "No jellyfish yet? Try the demo bloom" at the bottom of that sheet (the same switch is in Settings, "Demo bloom (no hardware)").
3. The status pill reads "Demo bloom": the app now talks to a simulated group of lamps inside the app, speaking the same protocol as the real ones.
4. Bloom tab: a jellyfish is drawn on screen and animates. Tap any mode tile to change it, drag the brightness and colour sliders.
5. Jellies tab: the simulated group with colour slot, address and firmware version. "Roll call" asks who is there, "Identify" makes the lamps blink so they can be told apart.
6. Settings tab: which Wi-Fi network to join and when, the demo switch, version information.

With real hardware only the first step differs: "Join" asks iOS to switch Wi-Fi to the lamps' network, and iOS shows its own dialog first (NEHotspotConfiguration, hence the Hotspot Configuration entitlement). On the first command iOS also asks for local network permission. The app is localised in English and German and follows the device language.

## 4. External services, tools and platforms

None. No third-party SDK, no analytics, no advertising, no authentication service, no payment processor, no AI service, no server of ours, no backend of any kind. The app never connects to the internet. It sends short lines of text over UDP to the lamps at 192.168.4.1, port 4210, inside the Wi-Fi network the lamps open themselves. It uses two Apple frameworks: NetworkExtension to join that network, Network for the socket. Settings stay in UserDefaults on the device; a custom Wi-Fi password, if the user chooses to keep one, goes to the device Keychain. The app therefore collects no data, as declared in the privacy questionnaire.

## 5. Regional differences

None. The app behaves identically in every region and storefront: no geographically restricted content, no region-specific feature, no server that could differ. The only adaptation is the language, German on German-language devices and English everywhere else.

## 6. Regulated industry, protected material

Neither applies. All code and artwork in the app were written by the developer. The wider project, including the lamp design the app controls, is open source under the MIT licence and continues the Sheffield-by-the-Sea Jellyfish project, credited in the app description and the repository as that licence requires.

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
