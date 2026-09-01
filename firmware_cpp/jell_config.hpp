#ifndef JELLYOS_JELLCONFIG_HPP
#define JELLYOS_JELLCONFIG_HPP

#include "jell_led.hpp" // ColourOrder

class JellConfig {

public:

    enum class DisplayMode
    {
        micLevelCheck,
        LEDChannelTest,
        Mic_NField,
        Mic_Drops,
        Ambient_Rainbow,
        Ambient_Deepsea,

        Count
    };

    static constexpr int NUMBER_LEDS_IN_RING = 96;

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

    static constexpr float BRIGHTNESS_MODIFIER = 1.0f;
    static constexpr auto DEFAULT_DISPLAY_MODE = DisplayMode::Mic_NField;
};
#endif
