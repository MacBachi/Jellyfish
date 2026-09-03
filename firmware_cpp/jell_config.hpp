#ifndef JELLYOS_JELLCONFIG_HPP
#define JELLYOS_JELLCONFIG_HPP

#include "jell_led.hpp" // ColourOrder

class JellConfig {

public:

    // Order defines the MODE numbers on the network and the button cycle, so every jelly
    // in a bloom must run the same firmware version. The calm modes come first, the two
    // test modes last.
    enum class DisplayMode
    {
        Breathe,
        Glimmer,
        Aurora,
        Current,
        Lantern,
        Moonlight,
        Drizzle,
        Fireflies,
        Swarm,
        Whisper,
        Playlist,
        Mic_NField,
        Mic_Drops,
        Palette,
        Palette_Cycle,
        Ambient_Rainbow,
        Ambient_Deepsea,
        micLevelCheck,
        LEDChannelTest,

        Count
    };

    // Playlist: cycles through these modes, one every PLAYLIST_STEP_S seconds, chosen from
    // the shared master time so every jelly switches at the same moment.
    static constexpr float PLAYLIST_STEP_S = 180.0f;
    static constexpr DisplayMode PLAYLIST[] = {
        DisplayMode::Breathe, DisplayMode::Glimmer, DisplayMode::Aurora, DisplayMode::Current,
        DisplayMode::Lantern, DisplayMode::Moonlight, DisplayMode::Drizzle, DisplayMode::Fireflies,
        DisplayMode::Swarm, DisplayMode::Palette_Cycle};
    static constexpr int PLAYLIST_SIZE = sizeof(PLAYLIST) / sizeof(PLAYLIST[0]);

    static constexpr bool playlist_is_valid()
    {
        for (int i = 0; i < PLAYLIST_SIZE; i++)
            if (PLAYLIST[i] == DisplayMode::Playlist || PLAYLIST[i] == DisplayMode::Count)
                return false;
        return true;
    }

    // Every mode change blends the previous picture into the new one over this long.
    static constexpr float CROSSFADE_S = 1.0f;

    // LEDs in the ring. Also sets the angular spacing of the ring's pixels, so it must match
    // the strip actually fitted. Overridable per build like the network settings below, e.g.
    //   cmake ... -DJELL_RING_LEDS=39
#ifndef JELL_RING_LEDS
#define JELL_RING_LEDS 96
#endif
    static constexpr int NUMBER_LEDS_IN_RING = JELL_RING_LEDS;

    // Tentacles hang off the NeoPix2..NeoPix8 headers of the jellyboard (GPIO 3..9).
    // The firmware always drives all of them; a header without a strip simply sends into nothing.
    static constexpr int NUMBER_OF_TENTACLES = 7;

    // Deliberate upper bound for the tentacle length. A shorter strip ignores the surplus
    // data, a longer strip stays dark beyond this count, so raise it before building longer
    // tentacles. Keep it modest: every configured LED costs ~30 us of output time per frame,
    // whether or not it physically exists.
    static constexpr int NUMBER_LEDS_IN_EACH_TENTACLE = 16;

    // PWM "noodle" LEDs on the LED1..LED4 headers (GPIO 12..15), fixed by the board.
    static constexpr int NUMBER_OF_NOODLES = 4;

    static constexpr auto LED_ORDER_RING  = ColourOrder::RGB;

    static constexpr auto LED_ORDER_TENTACLE  = ColourOrder::RGB;

    // Compile-time cap on the overall brightness, multiplied into every pixel and noodle.
    static constexpr float BRIGHTNESS_MODIFIER = 1.0f;
    static constexpr auto DEFAULT_DISPLAY_MODE = DisplayMode::Mic_NField;

    // Runtime defaults for the values that can later be changed over the network.
    static constexpr float DEFAULT_BRIGHTNESS = 1.0f;
    static constexpr float DEFAULT_HUE_OFFSET = 0.0f;

    // Palette modes: every jelly gets one of these hues (degrees) by its colour slot.
    // With more than PALETTE_SIZE jellies the palette wraps around.
    static constexpr int PALETTE_SIZE = 8;
    static constexpr float PALETTE[PALETTE_SIZE] = {0.0f, 30.0f, 60.0f, 120.0f, 180.0f, 220.0f, 270.0f, 310.0f};

    // Palette_Cycle: every jelly moves one palette step further every DEFAULT_CYCLE_PERIOD_S
    // seconds (changeable at runtime), blending over the last CYCLE_BLEND_S seconds of a period.
    static constexpr float DEFAULT_CYCLE_PERIOD_S = 10.0f;
    static constexpr float CYCLE_BLEND_S = 2.0f;

    // Identify: the AP jelly blinks red, all others blue, IDENT_BLINKS times in step.
    static constexpr float IDENT_BLINK_PERIOD_S = 0.5f;
    static constexpr int IDENT_BLINKS = 3;

    // --- Network ---
    // Every jelly runs the same firmware. On boot it looks for this network; if it finds
    // it, it joins as a station, otherwise it becomes the access point itself. Any device
    // that joins the network can control the bloom; there is no further authentication.
    //
    // Defaults can be overridden without editing this file, per build:
    //   cmake ... -DJELL_WIFI_SSID=bloom -DJELL_WIFI_PASSWORD=secret123
#ifndef JELL_WIFI_SSID
#define JELL_WIFI_SSID "\xF0\x9F\xAA\xBC" // the jellyfish emoji U+1FABC as UTF-8; an SSID is just bytes
#endif
#ifndef JELL_WIFI_PASSWORD
#define JELL_WIFI_PASSWORD "FroschUndMaus" // WPA2, at least 8 characters
#endif
    static constexpr const char* WIFI_SSID = JELL_WIFI_SSID;
    static constexpr const char* WIFI_PASSWORD = JELL_WIFI_PASSWORD;

    static constexpr uint16_t NET_PORT = 4210;               // UDP, one text line per datagram

    // Election: a jelly that finds no network keeps listening for a random time in this
    // range, then listens through one more full scan, and only then becomes the AP.
    static constexpr uint32_t NET_ELECTION_MIN_MS = 10000;
    static constexpr uint32_t NET_ELECTION_MAX_MS = 120000;
    static constexpr uint32_t NET_JOIN_TIMEOUT_MS = 15000;   // give up joining and go back to the election
    static constexpr uint32_t NET_STATE_PERIOD_MS = 1000;    // AP: heartbeat with the full state
    static constexpr uint32_t NET_STATE_MIN_GAP_MS = 50;     // AP: throttle for state-after-change
    static constexpr uint32_t NET_HELLO_RETRY_MS = 5000;     // station: ask for a colour slot until it has one
    static constexpr int NET_MAX_JELLIES = 16;               // roster size on the AP
};

// Checked here, after the class is complete, because a constexpr member function
// cannot be evaluated inside its own class definition.
static_assert(JellConfig::playlist_is_valid(), "the playlist must not contain itself");
#endif
