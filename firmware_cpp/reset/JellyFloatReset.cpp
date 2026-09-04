// JellyFloatReset: the factory-reset image for Find My. Its only job is to erase the flash
// sector that holds the Find My key (the fourth from the end, see jell_findmy.cpp), which
// puts the jelly back to "not set up". Flash it like any UF2, watch the onboard LED, then
// flash JellyFloatOS again. Not part of the normal firmware; nothing else on the jelly
// changes (the firmware keeps nothing else in flash).
//
// LED: three slow blinks = erased and verified; fast blinking forever = the sector still
// holds something, try again.
#include <cstdio>
#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "hardware/flash.h"
#include "hardware/sync.h"

int main()
{
    stdio_init_all();
    constexpr uint32_t offset = PICO_FLASH_SIZE_BYTES - 4u * FLASH_SECTOR_SIZE;

    const uint32_t irq = save_and_disable_interrupts();
    flash_range_erase(offset, FLASH_SECTOR_SIZE);
    restore_interrupts(irq);

    bool clean = true;
    const uint8_t* p = reinterpret_cast<const uint8_t*>(XIP_BASE + offset);
    for (uint32_t i = 0; i < FLASH_SECTOR_SIZE; i++)
        if (p[i] != 0xFF) clean = false;

    const bool led = cyw43_arch_init() == 0;
    while (true)
    {
        printf("JellyFloatReset: Find My sector at 0x%08lx %s\n", (unsigned long)offset,
               clean ? "erased, flash JellyFloatOS now" : "NOT erased");
        if (clean)
        {
            for (int i = 0; i < 3; i++)
            {
                if (led) cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
                sleep_ms(400);
                if (led) cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
                sleep_ms(400);
            }
            sleep_ms(1500);
        }
        else
        {
            for (int i = 0; i < 10; i++)
            {
                if (led) cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, true);
                sleep_ms(80);
                if (led) cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, false);
                sleep_ms(80);
            }
        }
    }
}
