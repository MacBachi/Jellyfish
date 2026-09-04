#!/usr/bin/env python3
"""Where is the jelly? Fetches the Find My location reports for an OpenHaystack key pair
with FindMy.py and prints them, newest last, with an Apple Maps link for the latest.

    python3 -m venv venv && venv/bin/pip install findmy   # once, Python 3.12 or newer
    venv/bin/python -W ignore locate.py [--keys FILE] [--name NAME]

FILE is the JSON from tools/findmy/keys.html or the one on the Desktop (fields privateKey,
advertisementKey). The Apple ID login happens once and is kept in account.json next to this
script, the emulated device state with it; delete that file to log in afresh. Everything
runs locally: the anisette library imitates an Apple device in this process, no server."""
import argparse, getpass, json, os, sys
from datetime import datetime, timezone
from pathlib import Path

HERE = Path(__file__).resolve().parent
ACCOUNT = HERE / "account.json"
LIBS = HERE / "anisette-libs"

def load_account():
    from findmy.reports import AppleAccount, LocalAnisetteProvider, LoginState
    if ACCOUNT.exists():
        try:
            acc = AppleAccount.from_json(ACCOUNT, anisette_libs_path=LIBS)
            if acc.login_state in (LoginState.LOGGED_IN, LoginState.AUTHENTICATED):
                return acc
            print("Saved session is not logged in any more; logging in again.")
        except Exception as e:  # noqa: BLE001
            print(f"Could not restore the saved session ({e}); logging in again.")
    anisette = LocalAnisetteProvider(libs_path=LIBS)
    acc = AppleAccount(anisette)
    print("Apple ID login (use a spare Apple ID; the password goes to Apple only, nothing is stored but the session).")
    email = input("Apple ID: ").strip()
    password = getpass.getpass("Password: ")
    state = acc.login(email, password)
    if state == LoginState.REQUIRE_2FA:
        methods = acc.get_2fa_methods()
        for i, m in enumerate(methods):
            label = f"SMS to {m.phone_number}" if hasattr(m, "phone_number") else "code on a trusted device"
            print(f"  [{i}] {label}")
        choice = int(input("Second factor: ").strip() or "0")
        method = methods[choice]
        method.request()
        code = input("Code: ").strip()
        state = method.submit(code)
    if state not in (LoginState.LOGGED_IN, LoginState.AUTHENTICATED):
        sys.exit(f"Login did not complete (state {state}).")
    acc.to_json(ACCOUNT)
    os.chmod(ACCOUNT, 0o600)
    print(f"Logged in; session saved to {ACCOUNT.name}.")
    return acc

def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--keys", default=str(Path.home() / "Desktop" / "jellyfloat-0451-findmy-keys.json"), help="key file (JSON with privateKey)")
    ap.add_argument("--name", default="Jelly", help="name for the map link")
    args = ap.parse_args()
    from findmy import KeyPair
    keys = json.loads(Path(args.keys).read_text())
    key = KeyPair.from_b64(keys["privateKey"])
    if keys.get("advertisementKey") and key.adv_key_b64 != keys["advertisementKey"]:
        sys.exit("The private key in the file does not match its advertisement key.")
    print(f"Key {key.hashed_adv_key_b64[:12]}…  (advertisement key {key.adv_key_b64[:12]}…)")

    acc = load_account()
    try:
        reports = acc.fetch_location_history(key)
    finally:
        try:
            acc.to_json(ACCOUNT)  # keep refreshed tokens
        except Exception:  # noqa: BLE001
            pass
        acc.close()
    reports = sorted(reports, key=lambda r: r.timestamp)
    if not reports:
        print("No reports yet. The jelly must have advertised near an iPhone, and Apple keeps reports for about a week.")
        return
    print(f"{len(reports)} reports:")
    print(f"{'when (local)':<20} {'latitude':>10} {'longitude':>11} {'±m':>5} {'conf':>4}")
    for r in reports:
        t = r.timestamp.astimezone().strftime("%Y-%m-%d %H:%M:%S")
        print(f"{t:<20} {r.latitude:>10.5f} {r.longitude:>11.5f} {r.horizontal_accuracy:>5.0f} {r.confidence:>4}")
    last = reports[-1]
    age = datetime.now(timezone.utc) - last.timestamp.astimezone(timezone.utc)
    print(f"\nLatest: {age.total_seconds()/60:.0f} min ago")
    print(f"https://maps.apple.com/?ll={last.latitude:.6f},{last.longitude:.6f}&q={args.name}")

if __name__ == "__main__":
    main()
