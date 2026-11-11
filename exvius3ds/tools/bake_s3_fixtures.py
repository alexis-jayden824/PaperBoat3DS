#!/usr/bin/env python3
"""Bake synthetic S3 RomFS fixtures (no Square Enix content)."""
from __future__ import annotations

import argparse
import struct
import zlib
from pathlib import Path

# Mirror C AES via a quick pure-Python CBC using PyCryptodome if present,
# otherwise emit plaintext-only + a separately compiled C bake path.
# For repo determinism we build plaintext MST/TEX here and an encrypted MST
# using the project's host test binary when available; fallback: plaintext only
# plus a hand-rolled AES if cryptography is installed.

IV = b"dZMjkk8gFDzKHlsx"
KEY_TOKEN = b"4foXVwWd"


def key16(token: bytes) -> bytes:
    raw = token[:16]
    return raw + b"\x00" * (16 - len(raw))


def pkcs7(data: bytes) -> bytes:
    pad = 16 - (len(data) % 16)
    return data + bytes([pad]) * pad


def aes_cbc_encrypt(key: bytes, iv: bytes, data: bytes) -> bytes:
    try:
        from cryptography.hazmat.primitives.ciphers import Cipher, algorithms, modes
    except ImportError as exc:  # pragma: no cover
        raise SystemExit("cryptography package required to bake encrypted MST") from exc
    enc = Cipher(algorithms.AES(key), modes.CBC(iv)).encryptor()
    return enc.update(data) + enc.finalize()


def pack_records(records: list[tuple[int, str]]) -> bytes:
    out = bytearray()
    for rid, name in records:
        nb = name.encode("utf-8")[:31].ljust(32, b"\0")
        out += struct.pack("<I", rid) + nb
    return bytes(out)


def write_mst(path: Path, records: list[tuple[int, str]], encrypted: bool) -> None:
    payload = pack_records(records)
    flags = 1 if encrypted else 0
    if encrypted:
        payload = aes_cbc_encrypt(key16(KEY_TOKEN), IV, pkcs7(payload))
    header = b"EX3MST1\0" + struct.pack("<II", 1, flags) + KEY_TOKEN + struct.pack(
        "<II", len(records), len(payload)
    )
    path.write_bytes(header + payload)


def write_tex(path: Path, width: int = 16, height: int = 16) -> None:
    pixels = bytearray()
    for y in range(height):
        for x in range(width):
            # Gold-ish diagnostic tile
            pixels += bytes((0xD4, 0xA0, 0x17, 0xFF if (x + y) % 2 == 0 else 0xCC))
    path.write_bytes(b"EX3TEX1\0" + struct.pack("<II", width, height) + pixels)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--out",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "romfs" / "fixtures",
    )
    args = ap.parse_args()
    args.out.mkdir(parents=True, exist_ok=True)

    records = [(1, "Rain"), (2, "Lasswell"), (3, "Fina")]
    write_mst(args.out / "diag.mst", records, encrypted=False)
    write_mst(args.out / "diag_enc.mst", records, encrypted=True)
    write_tex(args.out / "diag.tex")
    manifest = {
        "diag.mst": "plaintext MST fixture",
        "diag_enc.mst": "AES-CBC MST fixture (key 4foXVwWd)",
        "diag.tex": "16x16 gold checker texpack",
    }
    (args.out / "MANIFEST.txt").write_text(
        "\n".join(f"{k}: {v}" for k, v in manifest.items()) + "\n", encoding="utf-8"
    )
    for name in ("diag.mst", "diag_enc.mst", "diag.tex"):
        data = (args.out / name).read_bytes()
        print(f"{name}: {len(data)} bytes crc32={zlib.crc32(data) & 0xffffffff:08x}")


if __name__ == "__main__":
    main()
