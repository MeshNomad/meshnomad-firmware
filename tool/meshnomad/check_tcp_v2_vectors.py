#!/usr/bin/env python3
"""Independent check of test/vectors/tcp_v2.json (companion TCP protocol v2, B91).

Recomputes every value from the inputs with pyca/cryptography (not the rweather code the
firmware uses) and compares byte for byte. Run: python3 -I tool/meshnomad/check_tcp_v2_vectors.py
"""

import hashlib
import hmac
import json
import pathlib
import sys

from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric.ed25519 import Ed25519PrivateKey, Ed25519PublicKey
from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PrivateKey
from cryptography.hazmat.primitives.ciphers.aead import ChaCha20Poly1305
from cryptography.hazmat.primitives.kdf.hkdf import HKDF
from cryptography.hazmat.primitives.serialization import Encoding, PublicFormat

RAW = (Encoding.Raw, PublicFormat.Raw)


def nonce(counter: int) -> bytes:
    return b"\0" * 4 + counter.to_bytes(8, "little")


def main() -> int:
    path = pathlib.Path(__file__).resolve().parents[2] / "test" / "vectors" / "tcp_v2.json"
    raw = json.loads(path.read_text())
    password: str = raw["password"]
    v = {k: bytes.fromhex(x) for k, x in raw.items() if k not in ("note", "password")}
    fails = []

    def check(name: str, got: bytes, want: bytes) -> None:
        if got != want:
            fails.append(f"{name}: got {got.hex()} want {want.hex()}")

    node = Ed25519PrivateKey.from_private_bytes(v["node_ed25519_seed"])
    check("node_pub", node.public_key().public_bytes(*RAW), v["node_pub"])
    app = X25519PrivateKey.from_private_bytes(v["app_x25519_priv"])
    radio = X25519PrivateKey.from_private_bytes(v["radio_x25519_priv"])
    a = app.public_key().public_bytes(*RAW)
    r = radio.public_key().public_bytes(*RAW)
    n = v["node_pub"]

    check("hello", b"\xf0\x01\x02" + a, v["hello"])
    ch = v["challenge"]
    check("challenge.R", ch[3:35], r)
    check("challenge.N", ch[35:67], n)
    try:
        Ed25519PublicKey.from_public_bytes(n).verify(ch[67:131], b"MNTCP2-sig" + a + r)
    except Exception as e:  # noqa: BLE001 - any failure is a mismatch
        fails.append(f"signature does not verify: {e!r}")
    check("challenge.S (deterministic Ed25519)", node.sign(b"MNTCP2-sig" + a + r), ch[67:131])

    secret = app.exchange(radio.public_key())
    th = hashlib.sha256(b"MNTCP2-th" + a + r + n).digest()
    check("th", th, v["th"])
    okm = HKDF(algorithm=hashes.SHA256(), length=64, salt=b"MNTCP2", info=th).derive(secret)
    a2r, r2a = ChaCha20Poly1305(okm[:32]), ChaCha20Poly1305(okm[32:])

    mac = hmac.new(password.encode(), b"MNTCP2-login" + th, hashlib.sha256).digest()
    check("login_encrypted", a2r.encrypt(nonce(0), b"\xf0\x02" + mac, None), v["login_encrypted"])
    check("result_encrypted", r2a.encrypt(nonce(0), b"\xf0\x03\x00", None), v["result_encrypted"])
    check("app_frame_encrypted", a2r.encrypt(nonce(1), v["app_frame_plain"], None),
          v["app_frame_encrypted"])
    check("radio_frame_encrypted", r2a.encrypt(nonce(1), v["radio_frame_plain"], None),
          v["radio_frame_encrypted"])

    for f in fails:
        print("FAIL", f)
    print("tcp_v2.json:", "OK" if not fails else f"{len(fails)} mismatch(es)")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
