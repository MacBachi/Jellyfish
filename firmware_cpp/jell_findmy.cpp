#include "jell_findmy.hpp"
#include <cstdio>
#include <cstring>
#include "pico/stdlib.h"
#include "pico/flash.h"
#include "hardware/flash.h"
#include "btstack.h"

namespace
{
    // Fourth sector from the end; the linker script keeps the program below the last four.
    constexpr uint32_t SECTOR_OFFSET = PICO_FLASH_SIZE_BYTES - 4u * FLASH_SECTOR_SIZE;

    struct Record
    {
        char magic[4];
        uint8_t state;
        uint8_t reserved[3];
        uint8_t key[FindMy::KEY_BYTES];
        uint32_t check;
    };
    static_assert(sizeof(Record) == 40, "record layout");
    constexpr char MAGIC[4] = {'J', 'F', 'M', '1'};

    const Record* stored() { return reinterpret_cast<const Record*>(XIP_BASE + SECTOR_OFFSET); }

    uint32_t checksum(const Record& r)
    {
        uint32_t h = 0x811c9dc5u; // FNV-1a over everything before the checksum
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&r);
        for (size_t i = 0; i < offsetof(Record, check); i++)
            h = (h ^ p[i]) * 16777619u;
        return h;
    }

    bool erased(const Record& r)
    {
        const uint8_t* p = reinterpret_cast<const uint8_t*>(&r);
        for (size_t i = 0; i < sizeof r; i++)
            if (p[i] != 0xFF) return false;
        return true;
    }

    bool key_plausible(const uint8_t* k)
    {
        bool all0 = true, all1 = true;
        for (size_t i = 0; i < FindMy::KEY_BYTES; i++)
        {
            all0 &= k[i] == 0x00;
            all1 &= k[i] == 0xFF;
        }
        return !all0 && !all1;
    }

    // Base64 (standard alphabet, optional padding). Returns the byte count or -1.
    int base64_decode(const char* s, uint8_t* out, size_t max)
    {
        size_t n = 0;
        uint32_t acc = 0;
        int bits = 0;
        for (; *s; ++s)
        {
            const char c = *s;
            int v;
            if (c >= 'A' && c <= 'Z') v = c - 'A';
            else if (c >= 'a' && c <= 'z') v = c - 'a' + 26;
            else if (c >= '0' && c <= '9') v = c - '0' + 52;
            else if (c == '+') v = 62;
            else if (c == '/') v = 63;
            else if (c == '=' || c == ' ' || c == '\r' || c == '\n') continue;
            else return -1;
            acc = (acc << 6) | (uint32_t)v;
            bits += 6;
            if (bits >= 8)
            {
                bits -= 8;
                if (n >= max) return -1;
                out[n++] = (uint8_t)(acc >> bits);
                acc &= (1u << bits) - 1u;
            }
        }
        return (int)n;
    }

    FindMy::State g_state = FindMy::State::Unset;
    uint8_t g_key[FindMy::KEY_BYTES];

    bool g_window_open = false;
    uint64_t g_window_until_us = 0;

    // The write queued by the web page, done by poll() on the main loop: lwIP's callbacks
    // run from the radio's background context, no place to stop the other core from.
    volatile bool g_pending = false;
    Record g_pending_record;
    int g_last_result = -1; // -1 none yet, 0 ok, else the HTTP-like code
    const char* g_last_error = "";

    // ---- BLE

    btstack_packet_callback_registration_t g_hci_cb;
    uint8_t g_adv[31];
    bd_addr_t g_addr;
    bool g_ble_started = false;

    // The OpenHaystack layout: the address carries the key's first six bytes (top two bits
    // set: static random address), the payload bytes 6..27, and the key's top two bits.
    void build_advertisement()
    {
        memset(g_adv, 0, sizeof g_adv);
        g_adv[0] = 0x1e;             // length
        g_adv[1] = 0xff;             // manufacturer specific data
        g_adv[2] = 0x4c; g_adv[3] = 0x00; // Apple
        g_adv[4] = 0x12; g_adv[5] = 0x19; // offline finding, 25 bytes
        g_adv[6] = 0x00;             // state
        memcpy(&g_adv[7], &g_key[6], 22);
        g_adv[29] = g_key[0] >> 6;
        g_adv[30] = 0x00;            // hint
        g_addr[0] = g_key[0] | 0xC0;
        for (int i = 1; i < 6; i++) g_addr[i] = g_key[i];
    }

    void packet_handler(uint8_t packet_type, uint16_t, uint8_t* packet, uint16_t)
    {
        if (packet_type != HCI_EVENT_PACKET) return;
        if (hci_event_packet_get_type(packet) != BTSTACK_EVENT_STATE) return;
        if (btstack_event_state_get_state(packet) != HCI_STATE_WORKING) return;
        bd_addr_t none = {};
        gap_random_address_set_mode(GAP_RANDOM_ADDRESS_TYPE_STATIC);
        gap_random_address_set(g_addr);
        // 1 to 2 s, non-connectable undirected, all three channels, no filter: as OpenHaystack does.
        gap_advertisements_set_params(0x0640, 0x0C80, 0x03, 0, none, 0x07, 0x00);
        gap_advertisements_set_data(sizeof g_adv, g_adv);
        gap_advertisements_enable(1);
        printf("FindMy: advertising as %02x:%02x:%02x:%02x:%02x:%02x\n",
               g_addr[0], g_addr[1], g_addr[2], g_addr[3], g_addr[4], g_addr[5]);
    }

    void start_ble()
    {
        if (g_ble_started) return;
        g_ble_started = true;
        build_advertisement();
        l2cap_init();
        sm_init();
        g_hci_cb.callback = &packet_handler;
        hci_add_event_handler(&g_hci_cb);
        hci_power_control(HCI_POWER_ON);
    }

    // ---- flash

    void write_record(void* param)
    {
        // Runs with interrupts off and core 1 paused (flash_safe_execute).
        static uint8_t page[FLASH_PAGE_SIZE];
        memset(page, 0xFF, sizeof page);
        memcpy(page, param, sizeof(Record));
        flash_range_erase(SECTOR_OFFSET, FLASH_SECTOR_SIZE);
        flash_range_program(SECTOR_OFFSET, page, sizeof page);
    }

    void load()
    {
        const Record& r = *stored();
        if (erased(r))
        {
            g_state = FindMy::State::Unset;
            return;
        }
        if (memcmp(r.magic, MAGIC, 4) == 0 && r.check == checksum(r) &&
            (r.state == (uint8_t)FindMy::State::LockedValid || r.state == (uint8_t)FindMy::State::LockedDisabled))
        {
            g_state = (FindMy::State)r.state;
            memcpy(g_key, r.key, sizeof g_key);
            if (g_state == FindMy::State::LockedValid && !key_plausible(g_key))
                g_state = FindMy::State::LockedDisabled;
            return;
        }
        // Something else in the sector: treat it as locked, never as writable.
        printf("FindMy: unreadable record, treating as disabled\n");
        g_state = FindMy::State::LockedDisabled;
    }

    const char* state_name(FindMy::State s)
    {
        switch (s)
        {
        case FindMy::State::LockedValid: return "valid";
        case FindMy::State::LockedDisabled: return "disabled";
        default: return "unset";
        }
    }
}

void FindMy::core1_init()
{
    flash_safe_execute_core_init();
}

void FindMy::init()
{
    load();
    printf("FindMy: %s (sector at 0x%08lx)\n", state_name(g_state), (unsigned long)SECTOR_OFFSET);
    if (g_state == State::LockedValid)
        start_ble();
}

void FindMy::button_pressed()
{
    if (g_state != State::Unset || g_window_open) return;
    if (time_us_64() > (uint64_t)TRIGGER_WINDOW_S * 1000000u) return;
    g_window_open = true;
    g_window_until_us = time_us_64() + (uint64_t)PROVISION_WINDOW_S * 1000000u;
    printf("FindMy: provisioning window open for %lu s\n", (unsigned long)PROVISION_WINDOW_S);
}

FindMy::State FindMy::state() { return g_state; }

bool FindMy::provisioning_open()
{
    return g_state == State::Unset && g_window_open && time_us_64() < g_window_until_us;
}

uint32_t FindMy::provisioning_seconds_left()
{
    if (!provisioning_open()) return 0;
    return (uint32_t)((g_window_until_us - time_us_64()) / 1000000u);
}

int FindMy::provision(const char* body)
{
    auto fail = [&](int code, const char* why) { g_last_result = code; g_last_error = why; return code; };
    if (g_state != State::Unset) return fail(409, "already locked");
    if (g_pending) return fail(409, "a write is pending");
    if (!provisioning_open()) return fail(403, "provisioning window closed: press a button within 60 s after power-up");

    Record r{};
    memcpy(r.magic, MAGIC, 4);
    if (strcmp(body, "disable") == 0)
    {
        r.state = (uint8_t)State::LockedDisabled;
        memset(r.key, 0x00, sizeof r.key); // the sentinel: no valid P-224 x coordinate
    }
    else
    {
        uint8_t key[KEY_BYTES + 1];
        const int n = base64_decode(body, key, sizeof key);
        if (n != (int)KEY_BYTES) return fail(400, "the key must be 28 bytes of base64");
        if (!key_plausible(key)) return fail(400, "that is not a key");
        r.state = (uint8_t)State::LockedValid;
        memcpy(r.key, key, KEY_BYTES);
    }
    r.check = checksum(r);
    g_pending_record = r;
    g_last_result = 0;
    g_last_error = "";
    g_pending = true;
    return 0;
}

void FindMy::poll()
{
    if (g_window_open && time_us_64() >= g_window_until_us)
    {
        g_window_open = false;
        printf("FindMy: provisioning window closed\n");
    }
    if (!g_pending) return;
    const int rc = flash_safe_execute(write_record, &g_pending_record, 2000);
    g_pending = false;
    g_window_open = false;
    if (rc != PICO_OK)
    {
        printf("FindMy: flash write failed (%d)\n", rc);
        g_last_result = 500;
        g_last_error = "flash write failed";
        return;
    }
    load();
    printf("FindMy: written, now %s\n", state_name(g_state));
    if (g_state == State::LockedValid)
        start_ble();
}

size_t FindMy::write_json(char* buf, size_t n)
{
    const int w = snprintf(buf, n, "\"findmy\":{\"state\":\"%s\",\"open\":%s,\"left\":%lu,\"pending\":%s}",
                           state_name(g_state), provisioning_open() ? "true" : "false",
                           (unsigned long)provisioning_seconds_left(), g_pending ? "true" : "false");
    return w < 0 ? 0 : ((size_t)w < n ? (size_t)w : n - 1);
}

size_t FindMy::write_result_json(char* buf, size_t n)
{
    const int w = snprintf(buf, n, "{\"ok\":%s,\"code\":%d,\"error\":\"%s\",\"state\":\"%s\",\"pending\":%s}",
                           g_last_result == 0 ? "true" : "false", g_last_result, g_last_error,
                           state_name(g_state), g_pending ? "true" : "false");
    return w < 0 ? 0 : ((size_t)w < n ? (size_t)w : n - 1);
}
