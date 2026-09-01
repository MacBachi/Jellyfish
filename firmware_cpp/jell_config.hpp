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
        Palette,
        Palette_Cycle,
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
};
#endif
