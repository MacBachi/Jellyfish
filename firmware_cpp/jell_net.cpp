#include "jell_net.hpp"

#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pico/stdlib.h"
#include "pico/cyw43_arch.h"
#include "pico/unique_id.h"
#include "pico/rand.h"
#include "lwip/udp.h"
#include "lwip/pbuf.h"
#include "lwip/netif.h"
#include "lwip/ip_addr.h"

extern "C"
{
#include "dhcpserver.h" // plain C from pico-examples
}
#include "jell_config.hpp"
#include "jell_state.hpp"

namespace
{
    constexpr uint32_t COUNTRY = CYW43_COUNTRY_WORLDWIDE;
    constexpr size_t LINE_MAX = 64;
    constexpr int RX_QUEUE_SIZE = 8;

    // ---- receive queue: filled by the lwIP callback (IRQ context), drained by poll() ----
    struct RxLine
    {
        char text[LINE_MAX];
        uint64_t rx_us; // when it arrived, for the time sync
    };
    RxLine rx_queue[RX_QUEUE_SIZE];
    volatile int rx_head = 0; // next slot to write
    volatile int rx_tail = 0; // next slot to read

    // ---- this jelly ----
    Net::Role role = Net::Role::Scanning;
    bool radio_ok = false;
    JellState state;            // core 0's master copy, published to core 1 with publish()
    char my_id[5] = "0000";     // last two bytes of the unique board id, as hex
    uint32_t id_hash = 0;
    udp_pcb* pcb = nullptr;
    dhcp_server_t dhcp;
    uint64_t current_rx_us = 0; // arrival time of the line being handled, 0 for local lines

    // ---- timers (all local time_us_64) ----
    uint64_t scan_deadline_us = 0;
    uint64_t join_deadline_us = 0;
    bool final_scan = false;         // waiting time is over, listening one last time before becoming AP
    bool final_scan_started = false;
    uint64_t next_state_us = 0;
    uint64_t last_state_sent_us = 0;
    uint64_t next_hello_us = 0;
    uint64_t hello_reply_due_us = 0;
    uint64_t ident_clear_us = 0;
    bool ssid_seen = false;
    bool state_dirty = false;   // AP: send a STATE soon
    uint32_t last_local_beats = 0;
    int led_state = -1;

    // ---- time sync (station) ----
    bool have_offset = false;
    int64_t offset_us = 0;
    int offset_log_counter = 0;

    // ---- roster (AP): who is here and which colour slot they got ----
    struct Member
    {
        char id[5];
        int slot;
        uint64_t last_seen_us;
    };
    Member roster[JellConfig::NET_MAX_JELLIES];
    int roster_count = 0;

    // ------------------------------------------------------------------ helpers

    void log(const char* fmt, ...)
    {
        printf("[%8lu ms] net: ", (unsigned long)(time_us_64() / 1000));
        va_list args;
        va_start(args, fmt);
        vprintf(fmt, args);
        va_end(args);
        printf("\n");
    }

    void publish()
    {
        g_state.write(state);
    }

    int itf()
    {
        return role == Net::Role::AccessPoint ? CYW43_ITF_AP : CYW43_ITF_STA;
    }

    bool interface_up()
    {
        return radio_ok && (role == Net::Role::AccessPoint || role == Net::Role::Station);
    }

    const char* my_ip()
    {
        if (!interface_up())
            return "0.0.0.0";
        return ip4addr_ntoa(netif_ip4_addr(&cyw43_state.netif[itf()]));
    }

    // Send one line to everyone on the jelly network. No-op until an interface is up.
    void send_line(const char* line)
    {
        if (!interface_up() || pcb == nullptr)
            return;

        const size_t len = strlen(line);

        cyw43_arch_lwip_begin();
        pbuf* p = pbuf_alloc(PBUF_TRANSPORT, (u16_t)len, PBUF_RAM);
        if (p != nullptr)
        {
            memcpy(p->payload, line, len);

            ip_addr_t bcast;
            IP4_ADDR(ip_2_ip4(&bcast), 192, 168, 4, 255);
            const err_t err = udp_sendto_if(pcb, p, &bcast, JellConfig::NET_PORT, &cyw43_state.netif[itf()]);
            pbuf_free(p);

            if (err != ERR_OK)
                log("send failed (%d): %s", (int)err, line);
        }
        else
        {
            log("pbuf_alloc failed, dropped: %s", line);
        }
        cyw43_arch_lwip_end();

        if (strncmp(line, "STATE", 5) != 0)
            log("tx %s", line);
    }

    void send_state()
    {
        char line[LINE_MAX];
        snprintf(line, sizeof line, "STATE %d %.2f %.0f %.1f %llu %s",
                 (int)state.mode, state.brightness, state.hue_offset, state.cycle_period_s,
                 (unsigned long long)time_us_64(), my_id);
        send_line(line);
        last_state_sent_us = time_us_64();
        next_state_us = last_state_sent_us + (uint64_t)JellConfig::NET_STATE_PERIOD_MS * 1000;
        state_dirty = false;
    }

    void send_hello()
    {
        char line[LINE_MAX];
        snprintf(line, sizeof line, "HELLO %s %s %d %s",
                 my_id, state.is_ap ? "AP" : "STA", state.slot, my_ip());
        send_line(line);
    }

    // ------------------------------------------------------------------ lwIP / cyw43 callbacks

    // IRQ context: copy the datagram into the queue, nothing else.
    void on_udp_recv(void*, udp_pcb*, pbuf* p, const ip_addr_t* from, u16_t)
    {
        if (p == nullptr)
            return;

        // Never act on our own broadcasts, should the radio ever reflect them back.
        // (The AP re-sends a BEAT it receives, which would otherwise loop forever.)
        if (interface_up() && from != nullptr
            && ip4_addr_cmp(ip_2_ip4(from), netif_ip4_addr(&cyw43_state.netif[itf()])))
        {
            pbuf_free(p);
            return;
        }

        const int next = (rx_head + 1) % RX_QUEUE_SIZE;
        if (next != rx_tail)
        {
            RxLine& slot = rx_queue[rx_head];
            const u16_t n = pbuf_copy_partial(p, slot.text, LINE_MAX - 1, 0);
            slot.text[n] = 0;
            for (char* c = slot.text; *c; ++c)
                if (*c == '\r' || *c == '\n')
                {
                    *c = 0;
                    break;
                }
            slot.rx_us = time_us_64();
            rx_head = next;
        }
        pbuf_free(p);
    }

    int on_scan_result(void*, const cyw43_ev_scan_result_t* result)
    {
        if (result == nullptr)
            return 0;

        const size_t want = strlen(JellConfig::WIFI_SSID);
        if (result->ssid_len == want && memcmp(result->ssid, JellConfig::WIFI_SSID, want) == 0)
        {
            if (!ssid_seen)
                log("found %s (rssi %d)", JellConfig::WIFI_SSID, (int)result->rssi);
            ssid_seen = true;
        }
        return 0;
    }

    void start_scan()
    {
        cyw43_wifi_scan_options_t opts = {};
        const int err = cyw43_wifi_scan(&cyw43_state, &opts, nullptr, on_scan_result);
        if (err != 0)
            log("scan start failed (%d)", err);
    }

    // ------------------------------------------------------------------ role transitions

    // Start (or restart) the election: listen for an existing network for a random
    // 10..120 s, joining it as soon as it shows up. Only if nothing appears in that time,
    // and one more full scan afterwards, does this jelly become the AP.
    void set_scanning()
    {
        role = Net::Role::Scanning;
        ssid_seen = false;
        final_scan = false;
        final_scan_started = false;

        const uint32_t span_ms = JellConfig::NET_ELECTION_MAX_MS - JellConfig::NET_ELECTION_MIN_MS;
        const uint32_t wait_ms = JellConfig::NET_ELECTION_MIN_MS + get_rand_32() % (span_ms + 1);
        scan_deadline_us = time_us_64() + (uint64_t)wait_ms * 1000;

        log("election: listening for %s, becoming AP in %lu ms unless one appears",
            JellConfig::WIFI_SSID, (unsigned long)wait_ms);
        start_scan();
    }

    void set_joining()
    {
        const int err = cyw43_arch_wifi_connect_async(JellConfig::WIFI_SSID, JellConfig::WIFI_PASSWORD,
                                                      CYW43_AUTH_WPA2_AES_PSK);
        if (err != 0)
        {
            log("connect_async failed (%d), scanning again", err);
            set_scanning();
            return;
        }
        role = Net::Role::Joining;
        join_deadline_us = time_us_64() + (uint64_t)JellConfig::NET_JOIN_TIMEOUT_MS * 1000;
        log("joining %s", JellConfig::WIFI_SSID);
    }

    void set_station()
    {
        role = Net::Role::Station;
        state.is_ap = false;
        state.follow_network_beats = true;
        state.slot = -1;
        state.time_offset_us = 0;
        have_offset = false;
        publish();
        log("joined as station, ip %s", my_ip());
        send_hello();
        next_hello_us = time_us_64() + (uint64_t)JellConfig::NET_HELLO_RETRY_MS * 1000;
    }

    // The AP is gone: keep showing the last state on our own and start a new election.
    void lost_ap()
    {
        state.follow_network_beats = false; // back to our own microphone until a new AP is found
        state.time_offset_us = 0;
        state.ident_start_master_us = 0;
        publish();
        set_scanning();
    }

    void become_ap()
    {
        cyw43_arch_disable_sta_mode();
        cyw43_arch_enable_ap_mode(JellConfig::WIFI_SSID, JellConfig::WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK);

        ip_addr_t gw, mask;
        IP4_ADDR(ip_2_ip4(&gw), 192, 168, 4, 1);
        IP4_ADDR(ip_2_ip4(&mask), 255, 255, 255, 0);
        dhcp_server_init(&dhcp, &gw, &mask);

        role = Net::Role::AccessPoint;
        state.is_ap = true;
        state.follow_network_beats = false;
        state.slot = 0;
        state.time_offset_us = 0;
        publish();

        roster_count = 0;
        int max_stas = 0;
        cyw43_wifi_ap_get_max_stas(&cyw43_state, &max_stas);
        log("no AP found, now access point %s at %s (max stations %d)", JellConfig::WIFI_SSID, my_ip(), max_stas);

        send_hello();
        next_state_us = time_us_64() + 200000; // first heartbeat shortly after
    }

    // ------------------------------------------------------------------ roster (AP)

    Member* roster_find(const char* id)
    {
        for (int i = 0; i < roster_count; i++)
            if (strcmp(roster[i].id, id) == 0)
                return &roster[i];
        return nullptr;
    }

    void roster_seen(const char* id, bool is_station)
    {
        Member* m = roster_find(id);
        if (m == nullptr)
        {
            if (roster_count >= JellConfig::NET_MAX_JELLIES)
            {
                log("roster full, ignoring %s", id);
                return;
            }
            m = &roster[roster_count];
            strncpy(m->id, id, sizeof m->id - 1);
            m->id[sizeof m->id - 1] = 0;
            m->slot = roster_count + 1; // slot 0 is the AP itself
            roster_count++;
            log("new jelly %s gets slot %d", id, m->slot);
        }
        m->last_seen_us = time_us_64();

        if (is_station)
        {
            char line[LINE_MAX];
            snprintf(line, sizeof line, "SLOT %s %d", id, m->slot);
            send_line(line);
        }
    }

    // ------------------------------------------------------------------ parsing
    // Small cursor-based tokenizers; strtof/strtol instead of sscanf, which may lack
    // floating point support in the C library the SDK links.

    bool next_word(const char*& p, char* out, size_t n)
    {
        while (*p == ' ')
            p++;
        size_t i = 0;
        while (*p && *p != ' ' && i < n - 1)
            out[i++] = *p++;
        out[i] = 0;
        return i > 0;
    }

    bool next_int(const char*& p, int& out)
    {
        char* end;
        const long v = strtol(p, &end, 10);
        if (end == p)
            return false;
        p = end;
        out = (int)v;
        return true;
    }

    bool next_float(const char*& p, float& out)
    {
        char* end;
        const float v = strtof(p, &end);
        if (end == p)
            return false;
        p = end;
        out = v;
        return true;
    }

    bool next_u64(const char*& p, uint64_t& out)
    {
        char* end;
        const unsigned long long v = strtoull(p, &end, 10);
        if (end == p)
            return false;
        p = end;
        out = v;
        return true;
    }

    // ------------------------------------------------------------------ commands

    void set_mode(int mode, bool announce)
    {
        constexpr int count = (int)JellConfig::DisplayMode::Count;
        mode = ((mode % count) + count) % count;
        if ((int)state.mode != mode)
        {
            state.mode = (JellConfig::DisplayMode)mode;
            publish();
            state_dirty = true;
        }
        if (announce)
        {
            char line[LINE_MAX];
            snprintf(line, sizeof line, "MODE %d", mode);
            send_line(line);
        }
    }

    void apply_state_line(const char* args)
    {
        int mode;
        float bright, hue, cycle;
        uint64_t ap_time;
        char ap_id[8];
        const char* p = args;
        if (!next_int(p, mode) || !next_float(p, bright) || !next_float(p, hue) || !next_float(p, cycle)
            || !next_u64(p, ap_time) || !next_word(p, ap_id, sizeof ap_id))
        {
            log("bad STATE: %s", args);
            return;
        }

        if (role != Net::Role::Station)
            return; // the AP is the source of truth; other APs are a phase-C problem

        bool changed = false;
        constexpr int count = (int)JellConfig::DisplayMode::Count;
        if (mode >= 0 && mode < count && (int)state.mode != mode)
        {
            state.mode = (JellConfig::DisplayMode)mode;
            changed = true;
        }
        if (state.brightness != bright) { state.brightness = bright; changed = true; }
        if (state.hue_offset != hue) { state.hue_offset = hue; changed = true; }
        if (state.cycle_period_s != cycle) { state.cycle_period_s = cycle; changed = true; }

        // Time sync: the AP's clock at send time versus our clock at arrival.
        if (current_rx_us != 0)
        {
            const int64_t fresh = (int64_t)ap_time - (int64_t)current_rx_us;
            if (!have_offset)
            {
                offset_us = fresh;
                have_offset = true;
                log("time offset %lld us", (long long)offset_us);
            }
            else
            {
                const int64_t delta = fresh - offset_us;
                if (delta > 50000 || delta < -50000)
                    offset_us = fresh; // snap, something jumped
                else
                    offset_us += delta / 4; // slew
                if (++offset_log_counter % 10 == 0)
                    log("time offset %lld us (delta %lld)", (long long)offset_us, (long long)delta);
            }
            if (state.time_offset_us != offset_us)
            {
                state.time_offset_us = offset_us;
                changed = true;
            }
        }

        if (changed)
            publish();
    }

    void start_ident(int64_t start_master_us)
    {
        state.ident_start_master_us = start_master_us;
        publish();
        ident_clear_us = time_us_64()
            + (uint64_t)(JellConfig::IDENT_BLINK_PERIOD_S * JellConfig::IDENT_BLINKS * 1e6f) + 500000;
    }

    // Status on the onboard LED: fast blink = looking, solid = AP, slow blink = station.
    void update_led(uint64_t now_us)
    {
        if (!radio_ok)
            return;
        int on;
        switch (role)
        {
        case Net::Role::AccessPoint: on = 1; break;
        case Net::Role::Station: on = (int)((now_us / 500000) % 2); break;
        default: on = (int)((now_us / 100000) % 2); break;
        }
        if (on != led_state)
        {
            cyw43_arch_gpio_put(CYW43_WL_GPIO_LED_PIN, on);
            led_state = on;
        }
    }
}

// ---------------------------------------------------------------------- public API

Net::Role Net::role()
{
    return ::role;
}

void Net::handle_line(const char* line, bool local)
{
    char verb[16];
    const char* args = line;
    if (!next_word(args, verb, sizeof verb))
        return;
    for (char* c = verb; *c; ++c)
        *c = (char)toupper((unsigned char)*c);
    while (*args == ' ')
        args++;

    if (!local && strcmp(verb, "STATE") != 0)
        log("rx %s", line);

    char out[LINE_MAX];

    if (strcmp(verb, "MODE") == 0)
    {
        int mode;
        if (next_int(args, mode))
            set_mode(mode, local);
    }
    else if (strcmp(verb, "NEXT") == 0)
    {
        set_mode((int)state.mode + 1, true); // always announce the absolute result
    }
    else if (strcmp(verb, "PREV") == 0)
    {
        set_mode((int)state.mode - 1, true);
    }
    else if (strcmp(verb, "BRIGHT") == 0)
    {
        float v;
        if (next_float(args, v))
        {
            state.brightness = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
            publish();
            state_dirty = true;
            if (local)
            {
                snprintf(out, sizeof out, "BRIGHT %.2f", state.brightness);
                send_line(out);
            }
        }
    }
    else if (strcmp(verb, "HUE") == 0)
    {
        float v;
        if (next_float(args, v))
        {
            v = fmodf(v, 360.0f);
            if (v < 0.0f) v += 360.0f;
            state.hue_offset = v;
            publish();
            state_dirty = true;
            if (local)
            {
                snprintf(out, sizeof out, "HUE %.0f", state.hue_offset);
                send_line(out);
            }
        }
    }
    else if (strcmp(verb, "CYCLE") == 0)
    {
        float v;
        if (next_float(args, v))
        {
            state.cycle_period_s = v < 1.0f ? 1.0f : v;
            publish();
            state_dirty = true;
            if (local)
            {
                snprintf(out, sizeof out, "CYCLE %.1f", state.cycle_period_s);
                send_line(out);
            }
        }
    }
    else if (strcmp(verb, "BEAT") == 0)
    {
        if (state.is_ap)
        {
            // Only the AP ever sends BEAT, so this came from a laptop: pass it on.
            send_line("BEAT");
        }
        else
        {
            state.beat_count++;
            publish();
        }
    }
    else if (strcmp(verb, "IDENT") == 0)
    {
        uint64_t t;
        if (next_u64(args, t))
        {
            start_ident((int64_t)t);
        }
        else if (state.is_ap)
        {
            // Bare IDENT from a laptop: the AP stamps it so everyone blinks in step.
            const uint64_t start = time_us_64() + 200000; // a little ahead, so late receivers still catch the start
            snprintf(out, sizeof out, "IDENT %llu", (unsigned long long)start);
            send_line(out);
            start_ident((int64_t)start);
        }
    }
    else if (strcmp(verb, "STATE") == 0)
    {
        apply_state_line(args);
    }
    else if (strcmp(verb, "HELLO") == 0)
    {
        char id[8], their_role[8];
        int slot;
        if (next_word(args, id, sizeof id) && next_word(args, their_role, sizeof their_role) && next_int(args, slot))
        {
            if (state.is_ap && strcmp(id, my_id) != 0)
                roster_seen(id, strcmp(their_role, "STA") == 0);
        }
        else
        {
            // Bare HELLO: a roll call. Answer after an id-derived delay so replies don't collide.
            hello_reply_due_us = time_us_64() + (uint64_t)(id_hash % 200) * 1000 + 1;
        }
    }
    else if (strcmp(verb, "SLOT") == 0)
    {
        char id[8];
        int slot;
        if (next_word(args, id, sizeof id) && next_int(args, slot) && strcmp(id, my_id) == 0 && !state.is_ap)
        {
            if (state.slot != slot)
            {
                state.slot = slot;
                publish();
                log("my colour slot is %d", slot);
            }
        }
    }
    else
    {
        log("unknown command: %s", line);
    }
}

void Net::init()
{
    pico_unique_board_id_t id;
    pico_get_unique_board_id(&id);
    snprintf(my_id, sizeof my_id, "%02x%02x", id.id[6], id.id[7]);
    id_hash = 0;
    for (uint8_t b : id.id)
        id_hash = id_hash * 31u + b;

    state = JellState{};
    publish();

    if (cyw43_arch_init_with_country(COUNTRY) != 0)
    {
        log("radio init failed, running without network");
        return;
    }
    radio_ok = true;
    cyw43_arch_enable_sta_mode();

    cyw43_arch_lwip_begin();
    pcb = udp_new();
    if (pcb != nullptr)
    {
        udp_bind(pcb, IP_ANY_TYPE, JellConfig::NET_PORT);
        udp_recv(pcb, on_udp_recv, nullptr);
    }
    cyw43_arch_lwip_end();
    if (pcb == nullptr)
        log("udp_new failed");

    log("this jelly is %s", my_id);
    set_scanning();
}

void Net::poll()
{
    if (!radio_ok)
        return;

    const uint64_t now = time_us_64();

    // Received lines first, so a command never waits on the state machine below.
    while (rx_tail != rx_head)
    {
        RxLine copy = rx_queue[rx_tail];
        rx_tail = (rx_tail + 1) % RX_QUEUE_SIZE;
        current_rx_us = copy.rx_us;
        handle_line(copy.text, false);
        current_rx_us = 0;
    }

    switch (::role) // the variable; unqualified `role` here is the accessor Net::role()
    {
    case Role::Scanning:
        if (ssid_seen)
        {
            set_joining();
        }
        else if (final_scan)
        {
            // One complete sweep after the waiting time, started fresh, must find nothing.
            if (!final_scan_started)
            {
                if (!cyw43_wifi_scan_active(&cyw43_state))
                {
                    start_scan();
                    final_scan_started = true;
                }
            }
            else if (!cyw43_wifi_scan_active(&cyw43_state))
            {
                become_ap();
            }
        }
        else if (now >= scan_deadline_us)
        {
            log("waiting time over, one last listen before becoming AP");
            final_scan = true;
        }
        else if (!cyw43_wifi_scan_active(&cyw43_state))
        {
            start_scan();
        }
        break;

    case Role::Joining:
        {
            const int status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
            if (status == CYW43_LINK_UP)
                set_station();
            else if (status < 0 || now >= join_deadline_us)
            {
                log("join failed (status %d), scanning again", status);
                set_scanning();
            }
            break;
        }

    case Role::Station:
        {
            const int status = cyw43_tcpip_link_status(&cyw43_state, CYW43_ITF_STA);
            if (status != CYW43_LINK_UP)
            {
                log("link down (status %d), keeping last state, new election", status);
                lost_ap();
                break;
            }

            if (state.slot < 0 && now >= next_hello_us)
            {
                send_hello();
                next_hello_us = now + (uint64_t)JellConfig::NET_HELLO_RETRY_MS * 1000;
            }
            break;
        }

    case Role::AccessPoint:
        if (now >= next_state_us
            || (state_dirty && now - last_state_sent_us >= (uint64_t)JellConfig::NET_STATE_MIN_GAP_MS * 1000))
        {
            send_state();
        }

        if (g_local_beat_count != last_local_beats)
        {
            last_local_beats = g_local_beat_count;
            send_line("BEAT");
        }
        break;
    }

    if (hello_reply_due_us != 0 && now >= hello_reply_due_us)
    {
        hello_reply_due_us = 0;
        send_hello();
    }

    if (state.ident_start_master_us != 0 && now >= ident_clear_us)
    {
        state.ident_start_master_us = 0;
        publish();
    }

    update_led(now);
}
