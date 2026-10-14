#include "jell_ota.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pico/stdlib.h"
#include "pico/bootrom.h"
#include "pico/flash.h"
#include "hardware/flash.h"
#include "hardware/watchdog.h"
#include "hardware/regs/addressmap.h"
#include "boot/picobin.h"
#include "boot/picoboot_constants.h"
#include "boot/bootrom_constants.h"

#include "mbedtls/sha256.h"
#include "mbedtls/ecdsa.h"
#include "mbedtls/ecp.h"
#include "mbedtls/bignum.h"

#include "jell_config.hpp"
#include "jell_state.hpp"
#include "ota/ota_pubkey.h"

namespace
{
    enum class State : uint8_t { Idle, Receiving, Verifying, Failed, Rebooting };

    const char* state_name(State s)
    {
        switch (s)
        {
        case State::Receiving: return "receiving";
        case State::Verifying: return "verifying";
        case State::Failed: return "failed";
        case State::Rebooting: return "rebooting";
        default: return "idle";
        }
    }

    int g_running = -1;            // partition we booted from; -1 = no partition table
    bool g_buy_pending = false;    // booted on trial, not confirmed yet
    uint64_t g_boot_us = 0;

    volatile State g_state = State::Idle;
    const char* volatile g_error = "";
    uint32_t g_size = 0;
    uint8_t g_sig[64];
    volatile uint32_t g_written = 0;

    // One chunk handed over from the web server (lwIP context) to the main loop.
    uint8_t g_chunk[OTA::CHUNK];
    volatile uint32_t g_chunk_off = 0;
    volatile uint32_t g_chunk_len = 0;
    volatile bool g_chunk_ready = false;
    volatile bool g_finish_requested = false;
    uint64_t g_reboot_at_us = 0;

    uint32_t offset_of(int partition)
    {
        return partition == 0 ? JellConfig::OTA_PART_A_OFFSET : JellConfig::OTA_PART_B_OFFSET;
    }
    uint32_t target_offset() { return offset_of(g_running == 0 ? 1 : 0); }
    int target_partition() { return g_running == 0 ? 1 : 0; }

    // Flash as stored, without the address translation the running partition sees.
    const uint8_t* raw_flash(uint32_t offset)
    {
        return reinterpret_cast<const uint8_t*>(XIP_NOCACHE_NOALLOC_NOTRANSLATE_BASE + offset);
    }

    void fail(const char* why)
    {
        g_error = why;
        g_state = State::Failed;
        printf("ota: failed: %s\n", why);
    }

    // ---- flash, with core 1 parked between frames (see jell_settings.cpp)

    struct FlashOp
    {
        uint32_t offset;     // sector-aligned flash offset
        const uint8_t* data; // nullptr = erase only
        uint32_t len;        // multiple of FLASH_PAGE_SIZE
    };

    void do_flash_op(void* param)
    {
        const FlashOp* op = static_cast<const FlashOp*>(param);
        flash_range_erase(op->offset, FLASH_SECTOR_SIZE);
        if (op->data != nullptr && op->len > 0)
            flash_range_program(op->offset, op->data, op->len);
    }

    bool flash_op(const FlashOp& op)
    {
        g_core1_park = true;
        const uint64_t start = time_us_64();
        while (!g_core1_parked && time_us_64() - start < 200000)
            tight_loop_contents();
        const int rc = flash_safe_execute(do_flash_op, const_cast<FlashOp*>(&op), 2000);
        g_core1_park = false;
        watchdog_update();
        return rc == PICO_OK;
    }

    // ---- the image

    // The IMAGE_DEF block the SDK puts near the start of every binary: marker, then an
    // IMAGE_TYPE item whose flags carry the try-before-you-buy bit. Returns the offset of
    // the flags halfword, or -1 when this does not look like an RP2350 image.
    int image_type_flags_at(const uint8_t* image, uint32_t len)
    {
        const uint32_t limit = len < 1024 ? len : 1024;
        for (uint32_t i = 0; i + 8 <= limit; i += 4)
        {
            uint32_t word;
            memcpy(&word, image + i, 4);
            if (word == PICOBIN_BLOCK_MARKER_START && image[i + 4] == PICOBIN_BLOCK_ITEM_1BS_IMAGE_TYPE)
                return (int)(i + 6);
        }
        return -1;
    }

    bool signature_ok(const uint8_t* image, uint32_t len)
    {
        uint8_t hash[32];
        watchdog_update();
        if (mbedtls_sha256(image, len, hash, 0) != 0)
            return false;
        watchdog_update();

        mbedtls_ecp_group grp;
        mbedtls_ecp_point q;
        mbedtls_mpi r, s;
        mbedtls_ecp_group_init(&grp);
        mbedtls_ecp_point_init(&q);
        mbedtls_mpi_init(&r);
        mbedtls_mpi_init(&s);
        int rc = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_SECP256R1);
        if (rc == 0) rc = mbedtls_ecp_point_read_binary(&grp, &q, OTA_PUBKEY, sizeof OTA_PUBKEY);
        if (rc == 0) rc = mbedtls_mpi_read_binary(&r, g_sig, 32);
        if (rc == 0) rc = mbedtls_mpi_read_binary(&s, g_sig + 32, 32);
        if (rc == 0) rc = mbedtls_ecdsa_verify(&grp, hash, sizeof hash, &q, &r, &s);
        mbedtls_mpi_free(&s);
        mbedtls_mpi_free(&r);
        mbedtls_ecp_point_free(&q);
        mbedtls_ecp_group_free(&grp);
        watchdog_update();
        return rc == 0;
    }

    void write_pending_chunk()
    {
        const uint32_t off = g_chunk_off;
        const uint32_t len = g_chunk_len;
        if (g_state != State::Receiving || off != g_written || off + len > g_size)
        {
            g_chunk_ready = false;
            return; // refused in chunk() already; nothing to do
        }
        // Flash programs whole pages: pad the last chunk with erased bytes.
        const uint32_t padded = (len + FLASH_PAGE_SIZE - 1) / FLASH_PAGE_SIZE * FLASH_PAGE_SIZE;
        memset(g_chunk + len, 0xFF, padded - len);
        const FlashOp op = {target_offset() + off, g_chunk, padded};
        if (!flash_op(op))
            fail("flash write failed");
        else
            g_written = off + len;
        g_chunk_ready = false;
    }

    void verify_and_reboot()
    {
        if (g_state != State::Receiving)
            return;
        if (g_written != g_size)
        {
            fail("image incomplete");
            return;
        }
        g_state = State::Verifying;
        const uint8_t* image = raw_flash(target_offset());
        printf("ota: %lu bytes in partition %d, checking the signature\n", (unsigned long)g_size, target_partition());

        const int flags_at = image_type_flags_at(image, g_size);
        if (flags_at < 0)
        {
            fail("not a JellyFloatOS image");
            return;
        }
        if (!signature_ok(image, g_size))
        {
            fail("signature does not match");
            return;
        }

        // Mark it try-before-you-buy, so the bootrom falls back if it never confirms itself.
        memcpy(g_chunk, image, OTA::CHUNK);
        uint16_t flags;
        memcpy(&flags, g_chunk + flags_at, 2);
        flags |= PICOBIN_IMAGE_TYPE_EXE_TBYB_BITS;
        memcpy(g_chunk + flags_at, &flags, 2);
        const FlashOp op = {target_offset(), g_chunk, OTA::CHUNK};
        if (!flash_op(op))
        {
            fail("flash write failed");
            return;
        }

        printf("ota: signature good, rebooting into partition %d on trial\n", target_partition());
        g_state = State::Rebooting;
        g_reboot_at_us = time_us_64() + 800000; // let the status answer go out first
    }

    // A trial boot has proven itself: LEDs running, radio up, a few seconds in. Confirm it,
    // then retire the old image so the bootrom never picks it again.
    void buy_if_due()
    {
        if (!g_buy_pending)
            return;
        if (time_us_64() - g_boot_us < 5000000 || g_core1_frames < 100)
            return;
        g_buy_pending = false;
        static uint8_t work[4096];
        g_core1_park = true;
        const uint64_t start = time_us_64();
        while (!g_core1_parked && time_us_64() - start < 200000)
            tight_loop_contents();
        const int rc = rom_explicit_buy(work, sizeof work);
        g_core1_park = false;
        watchdog_update();
        if (rc != BOOTROM_OK)
        {
            printf("ota: confirming the update failed (%d)\n", rc);
            return;
        }
        const int other = g_running == 0 ? 1 : 0;
        const FlashOp op = {offset_of(other), nullptr, 0};
        flash_op(op);
        printf("ota: update confirmed, partition %d retired\n", other);
    }
}

void OTA::init()
{
    g_boot_us = time_us_64();
    boot_info_t info;
    if (rom_get_boot_info(&info))
    {
        g_running = info.partition;
        g_buy_pending = (info.tbyb_and_update_info & BOOT_TBYB_AND_UPDATE_FLAG_BUY_PENDING) != 0;
    }
    if (g_running == 0 || g_running == 1)
        printf("ota: running from partition %d%s\n", g_running, g_buy_pending ? ", on trial" : "");
    else
        printf("ota: no partition table, updates need USB (tools/ota/ota_prepare.sh)\n");
}

bool OTA::capable()
{
    return g_running == 0 || g_running == 1;
}

void OTA::poll()
{
    buy_if_due();
    if (g_chunk_ready)
        write_pending_chunk();
    if (g_finish_requested)
    {
        g_finish_requested = false;
        verify_and_reboot();
    }
    if (g_state == State::Rebooting && time_us_64() >= g_reboot_at_us)
    {
        rom_reboot(REBOOT2_FLAG_REBOOT_TYPE_FLASH_UPDATE | REBOOT2_FLAG_NO_RETURN_ON_SUCCESS, 10,
                   XIP_BASE + target_offset(), 0);
    }
}

bool OTA::begin(const char* args)
{
    if (!capable())
    {
        g_error = "no partition table; update this jelly over USB once";
        return false;
    }
    if (g_state == State::Verifying || g_state == State::Rebooting || g_chunk_ready)
        return false;
    char* end;
    const unsigned long size = strtoul(args, &end, 10);
    while (*end == ' ') end++;
    if (size == 0 || size > JellConfig::OTA_PART_SIZE || strlen(end) < 128)
    {
        fail("bad begin: expected size and 128 hex digits of signature");
        return false;
    }
    for (int i = 0; i < 64; i++)
    {
        char hex[3] = {end[2 * i], end[2 * i + 1], 0};
        char* e;
        g_sig[i] = (uint8_t)strtoul(hex, &e, 16);
        if (*e != 0)
        {
            fail("bad signature digits");
            return false;
        }
    }
    g_size = (uint32_t)size;
    g_written = 0;
    g_error = "";
    g_state = State::Receiving;
    printf("ota: receiving %lu bytes for partition %d\n", size, target_partition());
    return true;
}

bool OTA::chunk(uint32_t offset, const uint8_t* data, uint32_t len)
{
    if (g_state != State::Receiving || g_chunk_ready)
        return false;
    if (len == 0 || len > CHUNK || offset != g_written || offset % CHUNK != 0 || offset + len > g_size)
        return false;
    memcpy(g_chunk, data, len);
    g_chunk_off = offset;
    g_chunk_len = len;
    g_chunk_ready = true;
    return true;
}

bool OTA::finish()
{
    if (g_state != State::Receiving)
        return false;
    g_finish_requested = true;
    return true;
}

size_t OTA::write_json(char* buf, size_t n)
{
    const int w = snprintf(buf, n,
        "{\"capable\":%s,\"running\":%d,\"target\":%d,\"trial\":%s,\"state\":\"%s\",\"written\":%lu,\"size\":%lu,"
        "\"busy\":%s,\"error\":\"%s\",\"version\":\"%s\",\"variant\":\"%s\"}",
        capable() ? "true" : "false", g_running, capable() ? target_partition() : -1, g_buy_pending ? "true" : "false",
        state_name(g_state), (unsigned long)g_written, (unsigned long)g_size, g_chunk_ready ? "true" : "false",
        g_error, JellConfig::VERSION, JellConfig::VARIANT);
    return w < 0 ? 0 : ((size_t)w < n ? (size_t)w : n - 1);
}
