# Power board

An ESP32 board with eight relays that switches the USB supply of up to eight jellies. Each relay
carries the 5 V of one USB-C socket; the jelly's own plug goes into that socket. The board opens
its own network and serves one page to switch the ports, in the look of the jellies' page.

| | |
|---|---|
| Network | `⚡️`, password `PowerMeOn`, channel 11 (the jellies' `🪼` sits on channel 3) |
| Page | <http://192.168.5.1> or <http://power.local> |
| After power-up | the ports come on one after another, 2 s apart, and stay on |
| Stored | only the port names; never the on/off state |

## What the page does

- Tap a port to switch it. A hand on the switch always wins over a start that is still running.
- **Restart** takes the power away for 3 s and brings it back: the remedy for a jelly that no
  longer answers, without walking over to pull its plug.
- **Name** labels a port with the jelly that hangs on it (kept across restarts of the board).
- **All on, one by one** starts every port that is off, 2 s apart, so eight jellies do not pull
  their inrush current from the supply at the same moment. **All off** is immediate.

## Hardware

The common "ESP32_Relay_X8" board: ESP32-WROOM-32, eight Songle SRD-05VDC relays, LM2596 buck
regulator (7 to 30 V in) or 5 V direct, EN and IO0 buttons, a six-pin programming header without
auto-reset. Relays 1 to 8 are driven by GPIO 32, 33, 25, 26, 27, 14, 12, 13 (a HIGH pin pulls the
relay in), the status LED by GPIO 23. All of it lives in `namespace cfg` at the top of
`src/main.cpp`.

Wire each socket through the relay's **COM** and **NO** contacts, so that a port that is off, and
a board without power, means a jelly without power. Only the 5 V line needs to be switched; the
ground of all sockets stays connected to the supply. Size the 5 V supply for the jellies, not for
the board: a 94-LED jelly can draw 2 A and more (see the power table in the main README).

## Build and flash

```bash
brew install platformio
pio run -d powerboard
```

The board has no auto-reset, so the download mode is entered by hand: hold **IO0**, tap **EN**,
release IO0. Then, with a 3.3 V USB serial adapter on the programming header (adapter RX to the
board's TX, adapter TX to RX, GND to GND):

```bash
pio run -d powerboard -t upload --upload-port /dev/cu.usbserial-XXXX
```

Tap **EN** once more to start the new firmware. The console runs at 115200 baud and reports every
switch of a port.

## Working on the page without the board

```bash
python3 powerboard/tools/mock_power.py
```

serves `web/index.html` and the same `/api` as the firmware on <http://localhost:8099>.

## API

| | |
|---|---|
| `GET /api/status` | version, uptime, clients, and per port: `on`, `pending`, `cycling`, `in_ms`, `name` |
| `POST /api/relay` | `ch=1..8`, `on=0\|1` |
| `POST /api/cycle` | `ch=1..8`: off now, on again after 3 s |
| `POST /api/all` | `on=1` staggered start of every port that is off, `on=0` everything off at once |
| `POST /api/name` | `ch=1..8`, `name=` up to 24 bytes of UTF-8, empty clears |

Arguments are form-encoded. Every POST answers with the new status.
