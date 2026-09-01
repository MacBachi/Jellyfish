#pragma once

// WLAN for a bloom of jellies. One jelly is the access point, the others join it; laptop
// or phone join the same network. Everything - jelly-to-jelly sync and remote control -
// is plain text lines over UDP broadcast on JellConfig::NET_PORT. All of this runs on
// core 0; core 1 only ever sees the result through g_state (jell_state.hpp).
namespace Net
{
    enum class Role
    {
        Scanning,    // looking for an existing jelly network
        Joining,     // found one, waiting for the link and a DHCP lease
        Station,     // joined; follows the AP's state, time and beats
        AccessPoint  // runs the network; the source of state, time and beats
    };

    // Call once from main() after core 1 is running. Brings the radio up and starts
    // the role election; returns quickly, the rest happens in poll().
    void init();

    // Call every iteration of the core-0 loop (~20 ms). Drives the role state machine,
    // handles received lines, sends heartbeats and beats.
    void poll();

    Role role();

    // The single entry point for commands: received datagrams, the buttons, later the
    // web page. Applies the line to the state and, with `local` set, also sends it to
    // everyone else. Lines: MODE n | NEXT | PREV | BRIGHT f | HUE f | CYCLE f | BEAT |
    // IDENT [t] | STATE ... | HELLO [id role slot ip] | SLOT id n
    void handle_line(const char* line, bool local);
}
