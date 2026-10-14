#!/usr/bin/env python3
"""Sign JellyFloatOS images for over-the-air updates, and write the firmware's public key.

    tools/ota/ota_sign.py sign JellyFloatOS.bin [more.bin ...]   writes <file>.sig next to each
    tools/ota/ota_sign.py pubkey                                 rewrites firmware_cpp/ota/ota_pubkey.h

The private key lives outside the repository, in ~/.jellyfloat/ota-signing-key.pem (P-256,
made with: openssl ecparam -name prime256v1 -genkey -noout -out <file>). A .sig is the ECDSA
signature over the SHA-256 of the whole .bin as 128 hex digits (r then s, 32 bytes each), the
form the jellies check before they boot an update (firmware_cpp/jell_ota.cpp).
Needs only the openssl command line tool.
"""
import os, subprocess, sys

KEY = os.path.expanduser(os.environ.get("JELLYFLOAT_OTA_KEY", "~/.jellyfloat/ota-signing-key.pem"))
HERE = os.path.dirname(os.path.abspath(__file__))
PUBKEY_H = os.path.join(HERE, "..", "..", "firmware_cpp", "ota", "ota_pubkey.h")


def der_ints(der):
    """The two INTEGERs of a DER ECDSA-Sig-Value, as 32-byte big-endian each."""
    assert der[0] == 0x30, "not a DER sequence"
    i = 2 if der[1] < 0x80 else 2 + (der[1] & 0x7F)
    out = []
    for _ in range(2):
        assert der[i] == 0x02, "expected INTEGER"
        n = der[i + 1]
        v = der[i + 2:i + 2 + n].lstrip(b"\x00")
        assert len(v) <= 32, "integer too long for P-256"
        out.append(v.rjust(32, b"\x00"))
        i += 2 + n
    return out[0] + out[1]


def sign(path):
    der = subprocess.run(["openssl", "dgst", "-sha256", "-sign", KEY, path],
                         check=True, capture_output=True).stdout
    raw = der_ints(der)
    with open(path + ".sig", "w") as f:
        f.write(raw.hex() + "\n")
    # Check it with the public key right away, so a bad key never ships.
    pub = subprocess.run(["openssl", "ec", "-in", KEY, "-pubout"], check=True, capture_output=True).stdout
    pubfile = path + ".pub.tmp"
    with open(pubfile, "wb") as f:
        f.write(pub)
    try:
        ok = subprocess.run(["openssl", "dgst", "-sha256", "-verify", pubfile, "-signature", "/dev/stdin", path],
                            input=der, capture_output=True)
    finally:
        os.remove(pubfile)
    if ok.returncode != 0:
        sys.exit(f"{path}: signature does not verify")
    print(f"{path}.sig")


def pubkey():
    der = subprocess.run(["openssl", "ec", "-in", KEY, "-pubout", "-outform", "DER"],
                         check=True, capture_output=True).stdout
    point = der[-65:]
    assert point[0] == 0x04, "expected an uncompressed P-256 point"
    rows = ["    " + ", ".join(f"0x{b:02x}" for b in point[i:i + 13]) + "," for i in range(0, 65, 13)]
    with open(PUBKEY_H, "w") as f:
        f.write("#pragma once\n// The public half of ~/.jellyfloat/ota-signing-key.pem (P-256, uncompressed point).\n"
                "// Written by tools/ota/ota_sign.py pubkey; an update must be signed with the matching\n"
                "// private key or the jelly refuses to boot it.\n#include <cstdint>\n\n"
                "static const uint8_t OTA_PUBKEY[65] = {\n" + "\n".join(rows) + "\n};\n")
    print(os.path.normpath(PUBKEY_H))


if __name__ == "__main__":
    if len(sys.argv) >= 3 and sys.argv[1] == "sign":
        for p in sys.argv[2:]:
            sign(p)
    elif len(sys.argv) == 2 and sys.argv[1] == "pubkey":
        pubkey()
    else:
        sys.exit(__doc__)
