#pragma once

// The web page: a small HTTP server (lwIP httpd) on port 80 that serves the control page
// from flash and answers three requests:
//   GET  /api/state.json   this jelly, its settings, its roster
//   GET  /api/frame.bin    the colours on the LEDs right now (see Canvas::copy_frame)
//   POST /api/cmd          one command line, handled exactly like a line from the network
// The page only ever triggers runtime commands; nothing it does is persistent.
namespace Web
{
    // Call once after the radio is up. The server listens on every interface, so it
    // works whether this jelly runs the network or has joined one.
    void init();
}
