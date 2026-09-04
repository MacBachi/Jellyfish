#pragma once
#include <cstddef>
#include <cstdint>

// Find My: the jelly advertises as an OpenHaystack tag over BLE, so a phone in Apple's Find
// My network can report where it is. The P-224 public key comes from the web page, once;
// the private key never reaches the jelly. See README "Find My".
//
// The key lives in its own flash sector (the fourth from the end: BTstack's own storage
// takes the two before the last, the last stays free), outside the program image, so it
// survives firmware updates. Written once: after that the sector is locked until the
// factory-reset image (reset/JellyFloatReset.cpp) erases it.
namespace FindMy
{
    enum class State : uint8_t
    {
        Unset = 0,          // sector erased: nothing stored, provisioning possible
        LockedValid = 1,    // a public key: advertise
        LockedDisabled = 2  // the sentinel: never advertise, never accept a key
    };

    constexpr size_t KEY_BYTES = 28;
    constexpr uint32_t TRIGGER_WINDOW_S = 60;    // a button press this long after boot opens...
    constexpr uint32_t PROVISION_WINDOW_S = 600; // ...the provisioning window for this long

    // Core 1 calls this first thing: core 0 pauses it while it writes flash.
    void core1_init();
    // Core 0, after the radio is up: reads the record and starts advertising if there is a key.
    void init();
    // Core 0 main loop: performs a queued write, closes an expired window.
    void poll();
    // The physical trigger, from the main loop's button handling.
    void button_pressed();

    State state();
    bool provisioning_open();
    uint32_t provisioning_seconds_left();

    // From the web page: a base64 public key (28 bytes) or the word "disable". Validates and
    // queues the write for poll(). Returns 0 when queued, else an HTTP-like code:
    // 400 bad key, 403 window closed, 409 already locked or a write pending.
    int provision(const char* body);

    // JSON: the "findmy":{...} member of state.json, and the reply to POST /api/findmy.
    size_t write_json(char* buf, size_t n);
    size_t write_result_json(char* buf, size_t n);
}
