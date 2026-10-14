#pragma once
// Over-the-air update: a signed image arrives over HTTP in 4 KB chunks, goes into the flash
// partition the jelly is not running from, and is booted only once it is complete and its
// signature checks out. The new image runs "on trial" (the RP2350 bootrom's try-before-you-buy):
// it confirms itself once its LEDs and radio are up; if it hangs before that, the watchdog
// restarts the jelly and the bootrom falls back to the old image.
//
// Needs the A/B partition table (firmware_cpp/ota/pt.json), installed once over USB with
// tools/ota/ota_prepare.sh. A jelly without it keeps working; it just cannot update itself.
//
// HTTP (jell_web.cpp):
//   GET  /api/ota.json           status: capable, running/target partition, state, written, size
//   POST /api/ota/begin          body "<size> <signature, 128 hex digits>": start a new update
//   POST /api/ota/chunk          body: 4-byte little-endian offset, then up to 4096 bytes of image
//   POST /api/ota/finish         check the signature and reboot into the new image
// Chunks must come in order; after a break, read "written" from the status and carry on there.
#include <cstddef>
#include <cstdint>

namespace OTA
{
    constexpr uint32_t CHUNK = 4096;

    // Core 0, once at start: which partition we booted from, and whether we run on trial.
    void init();

    // Core 0, main loop: writes received chunks, verifies, confirms a trial boot.
    void poll();

    // From the web server (lwIP context). Each returns false when the request is refused;
    // the reason is in the status JSON.
    bool begin(const char* args);
    bool chunk(uint32_t offset, const uint8_t* data, uint32_t len);
    bool finish();

    // Status as JSON, for /api/ota.json and the jelly's status.
    size_t write_json(char* buf, size_t n);

    // True when this jelly can take an update (it booted from an A/B partition).
    bool capable();
}
