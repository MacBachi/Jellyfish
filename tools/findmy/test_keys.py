#!/usr/bin/env python3
"""Cross-checks p224.js against the Python `cryptography` library (the OpenHaystack
reference uses it): for fixed and random private scalars, the JavaScript public x
coordinate, the base64 encodings and the hashed key must match byte for byte.
Needs node and `pip install cryptography`."""
import base64, hashlib, json, os, secrets, subprocess, sys
from cryptography.hazmat.primitives.asymmetric import ec

here = os.path.dirname(os.path.abspath(__file__))
N = 0xffffffffffffffffffffffffffff16a2e0b8f03e13dd29455c5c2a3d
scalars = [1, 2, 3, 0xdeadbeef, N - 1, N - 2] + [secrets.randbelow(N - 1) + 1 for _ in range(20)]
js = "const P=require(%r);const out=%s.map(s=>{const k=P.generate(BigInt(s));return [k.privateKey,k.advertisementKey,k.hashedKey];});console.log(JSON.stringify(out));" % (
    os.path.join(here, "p224.js"), json.dumps([str(s) for s in scalars]))
got = json.loads(subprocess.check_output(["node", "-e", js]))
bad = 0
for s, (priv_b64, adv_b64, hash_b64) in zip(scalars, got):
    x = ec.derive_private_key(s, ec.SECP224R1()).public_key().public_numbers().x
    priv = base64.b64encode(s.to_bytes(28, "big")).decode()
    adv = base64.b64encode(x.to_bytes(28, "big")).decode()
    hashed = base64.b64encode(hashlib.sha256(x.to_bytes(28, "big")).digest()).decode()
    ok = (priv, adv, hashed) == (priv_b64, adv_b64, hash_b64)
    bad += not ok
    print(("ok  " if ok else "BAD ") + f"d={s:#x}" if s < 1 << 40 else ("ok  " if ok else "BAD ") + f"d=…{s & 0xffffffff:08x}")
    if not ok:
        print("  python", priv, adv, hashed)
        print("  js    ", priv_b64, adv_b64, hash_b64)
print(f"{len(scalars) - bad} of {len(scalars)} match")
sys.exit(1 if bad else 0)
