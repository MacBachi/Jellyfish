// Power board for a bloom of jellies: an ESP32 with eight relays, each switching the 5 V
// of one USB-C socket. It opens its own network and serves one page to switch the ports.
//
// After power-up the ports come on one after another, STAGGER_MS apart, and stay on: eight
// jellies starting at once would pull their inrush from the supply at the same moment.
// Nothing about the on/off state is stored; only the port names survive a restart.

#include <Arduino.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>

namespace cfg
{
    constexpr char SSID[] = "\xE2\x9A\xA1\xEF\xB8\x8F"; // the high-voltage emoji, U+26A1 U+FE0F
    constexpr char PASSWORD[] = "PowerMeOn";
    constexpr uint8_t WIFI_CHANNEL = 11;       // the jellies' own network sits on channel 3
    constexpr uint8_t MAX_CLIENTS = 8;
    const IPAddress AP_IP(192, 168, 5, 1);     // not 192.168.4.1: that is the jellies' page
    const IPAddress AP_MASK(255, 255, 255, 0);
    constexpr char MDNS_NAME[] = "power";      // http://power.local

    // Relay 1..8 as printed on the ESP32_Relay_X8 board. A HIGH pin pulls the relay in.
    constexpr uint8_t RELAY_PINS[] = {32, 33, 25, 26, 27, 14, 12, 13};
    constexpr bool RELAY_ACTIVE_HIGH = true;
    constexpr uint8_t LED_PIN = 23;

    constexpr uint32_t STAGGER_MS = 2000;      // one port after another, this far apart
    constexpr uint32_t CYCLE_OFF_MS = 3000;    // "restart": this long without power
    constexpr size_t PORT_NAME_BYTES = 24;            // bytes of UTF-8
}

constexpr int PORTS = sizeof cfg::RELAY_PINS / sizeof cfg::RELAY_PINS[0];

extern const uint8_t index_html_start[] asm("_binary_web_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_web_index_html_end");

struct Port
{
    bool on = false;
    bool pending = false;        // waiting for its turn in the staggered start
    bool cycling = false;        // switched off for a restart, comes back at cycle_until_ms
    uint32_t cycle_until_ms = 0;
    char name[cfg::PORT_NAME_BYTES + 1] = "";
};

static Port ports[PORTS];
static uint32_t next_step_ms = 0; // when the staggered start switches its next port
static WebServer server(80);
static Preferences prefs;

// millis() wraps after 49 days; a signed difference stays right across the wrap.
static bool due(uint32_t now, uint32_t at) { return (int32_t)(now - at) >= 0; }

static void drive(int i, bool on)
{
    ports[i].on = on;
    digitalWrite(cfg::RELAY_PINS[i], on == cfg::RELAY_ACTIVE_HIGH ? HIGH : LOW);
    Serial.printf("[%7lu ms] port %d %s\n", (unsigned long)millis(), i + 1, on ? "on" : "off");
}

static bool any_pending()
{
    for (const Port& p : ports)
        if (p.pending) return true;
    return false;
}

// Queue every port that is off for the staggered start. first_in_ms: delay before the first.
static void start_all(uint32_t first_in_ms)
{
    const bool idle = !any_pending();
    for (Port& p : ports)
        if (!p.on && !p.cycling) p.pending = true;
    if (idle) next_step_ms = millis() + first_in_ms;
}

static void sequencer(uint32_t now)
{
    for (int i = 0; i < PORTS; i++)
    {
        Port& p = ports[i];
        if (p.cycling && due(now, p.cycle_until_ms))
        {
            p.cycling = false;
            drive(i, true);
        }
    }
    if (!any_pending() || !due(now, next_step_ms)) return;
    for (int i = 0; i < PORTS; i++)
    {
        if (!ports[i].pending) continue;
        ports[i].pending = false;
        drive(i, true);
        next_step_ms = now + cfg::STAGGER_MS;
        break;
    }
}

// ---------------------------------------------------------------- names (the only stored state)

static void load_names()
{
    prefs.begin("power", false); // read-write: a read-only open of a namespace that does not exist yet logs an error
    for (int i = 0; i < PORTS; i++)
    {
        char key[4];
        snprintf(key, sizeof key, "n%d", i + 1);
        prefs.getString(key, ports[i].name, sizeof ports[i].name);
    }
    prefs.end();
}

// Keep what is safe inside a JSON string and fits, cut on a UTF-8 character boundary.
static void set_name(int i, const String& raw)
{
    char clean[cfg::PORT_NAME_BYTES + 1];
    size_t n = 0, k = 0;
    for (; k < raw.length() && n < cfg::PORT_NAME_BYTES; k++)
    {
        const uint8_t c = (uint8_t)raw[k];
        if (c < 0x20 || c == '"' || c == '\\' || c == 0x7f) continue;
        clean[n++] = (char)c;
    }
    if (k < raw.length() && n > 0)
    {
        // Cut short: if the last character is incomplete, drop all of it.
        size_t lead = n - 1;
        while (lead > 0 && ((uint8_t)clean[lead] & 0xC0) == 0x80) lead--;
        const uint8_t b = (uint8_t)clean[lead];
        const size_t need = b >= 0xF0 ? 4 : b >= 0xE0 ? 3 : b >= 0xC0 ? 2 : 1;
        if (n - lead < need) n = lead;
    }
    clean[n] = 0;
    strlcpy(ports[i].name, clean, sizeof ports[i].name);

    char key[4];
    snprintf(key, sizeof key, "n%d", i + 1);
    prefs.begin("power", false);
    if (n) prefs.putString(key, clean); else prefs.remove(key);
    prefs.end();
}

// ---------------------------------------------------------------- web

static void send_status()
{
    const uint32_t now = millis();
    String j;
    j.reserve(900);
    j += "{\"version\":\"" POWER_VERSION "\",\"uptime\":";
    j += now / 1000;
    j += ",\"clients\":";
    j += WiFi.softAPgetStationNum();
    j += ",\"stagger_ms\":";
    j += cfg::STAGGER_MS;
    j += ",\"cycle_ms\":";
    j += cfg::CYCLE_OFF_MS;
    j += ",\"ports\":[";
    int32_t wait = (int32_t)(next_step_ms - now);
    if (wait < 0) wait = 0;
    for (int i = 0; i < PORTS; i++)
    {
        const Port& p = ports[i];
        int32_t in_ms = 0;
        if (p.pending) { in_ms = wait; wait += cfg::STAGGER_MS; }
        else if (p.cycling) { in_ms = (int32_t)(p.cycle_until_ms - now); if (in_ms < 0) in_ms = 0; }
        if (i) j += ',';
        j += "{\"ch\":"; j += i + 1;
        j += ",\"on\":"; j += p.on ? "true" : "false";
        j += ",\"pending\":"; j += p.pending ? "true" : "false";
        j += ",\"cycling\":"; j += p.cycling ? "true" : "false";
        j += ",\"in_ms\":"; j += in_ms;
        j += ",\"name\":\""; j += p.name; j += "\"}";
    }
    j += "]}";
    server.sendHeader("Cache-Control", "no-store");
    server.send(200, "application/json", j);
}

// The port a request names, 0-based, or -1 after answering 400.
static int port_arg()
{
    const int ch = server.arg("ch").toInt();
    if (ch >= 1 && ch <= PORTS) return ch - 1;
    server.send(400, "text/plain", "ch must be 1.." + String(PORTS));
    return -1;
}

static void setup_web()
{
    server.on("/", HTTP_GET, [] {
        server.sendHeader("Cache-Control", "no-cache");
        server.send_P(200, "text/html; charset=utf-8", (const char*)index_html_start,
                      index_html_end - index_html_start - 1); // embed_txtfiles adds a NUL
    });
    server.on("/api/status", HTTP_GET, send_status);

    server.on("/api/relay", HTTP_POST, [] {
        const int i = port_arg();
        if (i < 0) return;
        ports[i].pending = false; // a hand on the switch overrules the staggered start
        ports[i].cycling = false;
        drive(i, server.arg("on").toInt() != 0);
        send_status();
    });
    server.on("/api/cycle", HTTP_POST, [] {
        const int i = port_arg();
        if (i < 0) return;
        ports[i].pending = false;
        ports[i].cycling = true;
        ports[i].cycle_until_ms = millis() + cfg::CYCLE_OFF_MS;
        drive(i, false);
        send_status();
    });
    server.on("/api/all", HTTP_POST, [] {
        if (server.arg("on").toInt() != 0)
            start_all(0);
        else
            for (int i = 0; i < PORTS; i++)
            {
                ports[i].pending = ports[i].cycling = false;
                if (ports[i].on) drive(i, false);
            }
        send_status();
    });
    server.on("/api/name", HTTP_POST, [] {
        const int i = port_arg();
        if (i < 0) return;
        set_name(i, server.arg("name"));
        send_status();
    });
    server.onNotFound([] { server.send(404, "text/plain", "not here"); });
    server.begin();
}

// ---------------------------------------------------------------- main

void setup()
{
    // Relays first, before anything slow: every port off until its turn comes.
    for (const uint8_t pin : cfg::RELAY_PINS)
    {
        digitalWrite(pin, cfg::RELAY_ACTIVE_HIGH ? LOW : HIGH);
        pinMode(pin, OUTPUT);
    }
    pinMode(cfg::LED_PIN, OUTPUT);

    Serial.begin(115200);
    Serial.printf("\npower board " POWER_VERSION ", %d ports, one every %lu ms\n", PORTS, (unsigned long)cfg::STAGGER_MS);

    load_names();
    start_all(cfg::STAGGER_MS);

    WiFi.persistent(false);
    WiFi.mode(WIFI_AP);
    WiFi.softAPConfig(cfg::AP_IP, cfg::AP_IP, cfg::AP_MASK);
    const bool ap = WiFi.softAP(cfg::SSID, cfg::PASSWORD, cfg::WIFI_CHANNEL, 0, cfg::MAX_CLIENTS);
    Serial.printf("network %s on channel %d: %s, http://%s or http://%s.local\n", cfg::SSID, cfg::WIFI_CHANNEL,
                  ap ? "up" : "FAILED", WiFi.softAPIP().toString().c_str(), cfg::MDNS_NAME);
    if (MDNS.begin(cfg::MDNS_NAME)) MDNS.addService("http", "tcp", 80);

    setup_web();
}

void loop()
{
    const uint32_t now = millis();
    sequencer(now);
    server.handleClient();
    // LED: blinking while ports are still to come, steady once everything asked for is on.
    digitalWrite(cfg::LED_PIN, (any_pending() ? (now / 250) % 2 : 1) ? HIGH : LOW);
    delay(2);
}
