#include "jell_settings.hpp"
#include <cstdio>
#include <cstring>
#include "pico/stdlib.h"
#include "pico/flash.h"
#include "hardware/flash.h"
#include "hardware/watchdog.h"
#include "jell_config.hpp"

namespace
{
    // The last sector of the flash; the linker script keeps the program below the last four.
    constexpr uint32_t SECTOR_OFFSET = PICO_FLASH_SIZE_BYTES - 1u * FLASH_SECTOR_SIZE;

    struct Record
    {
        char magic[4];
        uint8_t version;
        uint8_t mode;
        uint8_t noodle_follow;
        uint8_t pad;
        float brightness;
        float hue_offset;
        float cycle_period_s;
        float sensitivity;
        float trim;          // this jelly's own microphone trim, never part of STATE
        uint32_t check;
    };
    static_assert(sizeof(Record) == 32, "record layout");
    constexpr char MAGIC[4] = {'J', 'S', 'E', 'T'};
    constexpr uint8_t VERSION = 1;

    const Record* stored() { return reinterpret_cast<const Record*>(XIP_BASE + SECTOR_OFFSET); }

    uint32_t checksum(const Record& r)
    {
        uint32_t h = 0x811c9dc5u; // FNV-1a over everything before the checksum
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&r);
        for (size_t i = 0; i < offsetof(Record, check); i++)
            h = (h ^ p[i]) * 16777619u;
        return h;
    }

    bool valid(const Record& r)
    {
        return memcmp(r.magic, MAGIC, 4) == 0 && r.version == VERSION && r.check == checksum(r);
    }

    bool same(const Record& a, const Record& b)
    {
        return memcmp(&a, &b, offsetof(Record, check)) == 0;
    }

    Record g_wanted = {};          // what should be in flash
    bool g_have_wanted = false;
    uint64_t g_changed_us = 0;     // when g_wanted last changed
    uint64_t g_written_us = 0;     // when flash was last written
    int g_failures = 0;

    void fill(Record& r, const JellState& s, float trim)
    {
        memset(&r, 0, sizeof r);
        memcpy(r.magic, MAGIC, 4);
        r.version = VERSION;
        r.mode = (uint8_t)s.mode;
        r.noodle_follow = s.noodle_follow ? 1 : 0;
        r.brightness = s.brightness;
        r.hue_offset = s.hue_offset;
        r.cycle_period_s = s.cycle_period_s;
        r.sensitivity = s.sensitivity;
        r.trim = trim;
        r.check = checksum(r);
    }

    void write_record(void* param)
    {
        // Runs with interrupts off and core 1 paused (flash_safe_execute).
        static uint8_t page[FLASH_PAGE_SIZE];
        memset(page, 0xFF, sizeof page);
        memcpy(page, param, sizeof(Record));
        flash_range_erase(SECTOR_OFFSET, FLASH_SECTOR_SIZE);
        flash_range_program(SECTOR_OFFSET, page, sizeof page);
    }

    void changed()
    {
        g_have_wanted = true;
        g_changed_us = time_us_64();
    }
}

bool Settings::load(JellState& state, float& trim)
{
    const Record& r = *stored();
    if (!valid(r))
        return false;
    if (r.mode < (uint8_t)JellConfig::DisplayMode::Count)
        state.mode = (JellConfig::DisplayMode)r.mode;
    state.noodle_follow = r.noodle_follow != 0;
    if (r.brightness >= 0.0f && r.brightness <= 1.0f) state.brightness = r.brightness;
    if (r.hue_offset >= 0.0f && r.hue_offset < 360.0f) state.hue_offset = r.hue_offset;
    if (r.cycle_period_s >= 1.0f && r.cycle_period_s <= 3600.0f) state.cycle_period_s = r.cycle_period_s;
    if (r.sensitivity >= JellConfig::SENS_MIN && r.sensitivity <= JellConfig::SENS_MAX) state.sensitivity = r.sensitivity;
    if (r.trim >= JellConfig::TRIM_MIN && r.trim <= JellConfig::TRIM_MAX) trim = r.trim;
    g_wanted = r;
    g_have_wanted = true;
    g_changed_us = 0; // nothing to write
    printf("settings: restored mode %d bright %.2f hue %.0f cycle %.1f noodle %d sens %.2f trim %.2f\n",
           (int)state.mode, state.brightness, state.hue_offset, state.cycle_period_s,
           state.noodle_follow ? 1 : 0, state.sensitivity, trim);
    return true;
}

void Settings::note(const JellState& state)
{
    Record r;
    fill(r, state, g_have_wanted ? g_wanted.trim : JellConfig::DEFAULT_TRIM);
    if (g_have_wanted && same(r, g_wanted))
        return;
    g_wanted = r;
    changed();
}

void Settings::note_trim(float trim)
{
    if (!g_have_wanted)
        fill(g_wanted, JellState{}, trim);
    if (g_wanted.trim == trim)
        return;
    g_wanted.trim = trim;
    g_wanted.check = checksum(g_wanted);
    changed();
}

void Settings::poll()
{
    if (!g_have_wanted || g_changed_us == 0)
        return;
    const uint64_t now = time_us_64();
    if (now - g_changed_us < (uint64_t)JellConfig::SETTINGS_SETTLE_MS * 1000)
        return;
    if (g_written_us != 0 && now - g_written_us < (uint64_t)JellConfig::SETTINGS_MIN_INTERVAL_MS * 1000)
        return;
    if (g_failures >= 2)
        return; // logged below; the look is cosmetic, no reboot over it
    if (valid(*stored()) && same(*stored(), g_wanted))
    {
        g_changed_us = 0;
        return;
    }

    // Park core 1 between two frames so the flash lockout cannot land mid LED push.
    g_core1_park = true;
    const uint64_t wait_start = time_us_64();
    while (!g_core1_parked && time_us_64() - wait_start < 200000)
        tight_loop_contents();
    const int rc = flash_safe_execute(write_record, &g_wanted, 2000);
    g_core1_park = false;
    watchdog_update();

    g_written_us = time_us_64();
    if (rc != PICO_OK)
    {
        g_failures++;
        printf("settings: flash write failed (%d)%s\n", rc, g_failures >= 2 ? ", giving up until the next restart" : "");
        return;
    }
    g_failures = 0;
    g_changed_us = 0;
    printf("settings: saved mode %d bright %.2f sens %.2f trim %.2f\n",
           (int)g_wanted.mode, g_wanted.brightness, g_wanted.sensitivity, g_wanted.trim);
}
