#!/usr/bin/env python3
"""A stand-in for the power board: serves web/index.html and the same /api as the firmware,
so the page can be worked on without the hardware.  python3 tools/mock_power.py [port]"""
import json, sys, threading, time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs

PAGE = Path(__file__).resolve().parent.parent / "web" / "index.html"
STAGGER, CYCLE, N = 2.0, 3.0, 8
T0 = time.time()
lock = threading.Lock()
ports = [dict(on=False, pending=False, cycling=False, until=0.0, name="") for _ in range(N)]
next_step = 0.0


def start_all(first_in):
    global next_step
    idle = not any(p["pending"] for p in ports)
    for p in ports:
        if not p["on"] and not p["cycling"]:
            p["pending"] = True
    if idle:
        next_step = time.time() + first_in


def tick():
    global next_step
    while True:
        time.sleep(0.05)
        now = time.time()
        with lock:
            for p in ports:
                if p["cycling"] and now >= p["until"]:
                    p.update(cycling=False, on=True)
            if any(p["pending"] for p in ports) and now >= next_step:
                p = next(p for p in ports if p["pending"])
                p.update(pending=False, on=True)
                next_step = now + STAGGER


def status():
    now = time.time()
    wait = max(0.0, next_step - now)
    out = []
    for i, p in enumerate(ports):
        in_ms = 0
        if p["pending"]:
            in_ms, wait = wait, wait + STAGGER
        elif p["cycling"]:
            in_ms = max(0.0, p["until"] - now)
        out.append(dict(ch=i + 1, on=p["on"], pending=p["pending"], cycling=p["cycling"], in_ms=int(in_ms * 1000), name=p["name"]))
    return dict(version="mock", uptime=int(now - T0), clients=1, stagger_ms=int(STAGGER * 1000), cycle_ms=int(CYCLE * 1000), ports=out)


class H(BaseHTTPRequestHandler):
    def log_message(self, *a): pass

    def reply(self, code, ctype, body):
        self.send_response(code); self.send_header("Content-Type", ctype)
        self.send_header("Cache-Control", "no-store"); self.send_header("Content-Length", str(len(body)))
        self.end_headers(); self.wfile.write(body)

    def do_GET(self):
        if self.path == "/":
            self.reply(200, "text/html; charset=utf-8", PAGE.read_bytes())
        elif self.path == "/api/status":
            with lock: self.reply(200, "application/json", json.dumps(status()).encode())
        else:
            self.reply(404, "text/plain", b"not here")

    def do_POST(self):
        a = {k: v[0] for k, v in parse_qs(self.rfile.read(int(self.headers.get("Content-Length", 0))).decode(), keep_blank_values=True).items()}
        with lock:
            if self.path == "/api/all":
                if int(a.get("on", 0)): start_all(0)
                else:
                    for p in ports: p.update(on=False, pending=False, cycling=False)
            else:
                ch = int(a.get("ch", 0))
                if not 1 <= ch <= N: return self.reply(400, "text/plain", b"ch must be 1..8")
                p = ports[ch - 1]
                if self.path == "/api/relay": p.update(on=bool(int(a.get("on", 0))), pending=False, cycling=False)
                elif self.path == "/api/cycle": p.update(on=False, pending=False, cycling=True, until=time.time() + CYCLE)
                elif self.path == "/api/name": p["name"] = a.get("name", "").replace('"', "").replace("\\", "")[:24]
                else: return self.reply(404, "text/plain", b"not here")
            self.reply(200, "application/json", json.dumps(status()).encode())


if __name__ == "__main__":
    start_all(STAGGER)
    threading.Thread(target=tick, daemon=True).start()
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8099
    print(f"mock power board on http://localhost:{port}")
    ThreadingHTTPServer(("127.0.0.1", port), H).serve_forever()
