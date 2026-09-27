#!/usr/bin/env python3
"""Write HOME-menu banner PNG/WAV used by bannertool (stdlib only)."""

from __future__ import annotations

import argparse
import struct
import zlib
from pathlib import Path


def png_chunk(tag: bytes, data: bytes) -> bytes:
    crc = zlib.crc32(tag + data) & 0xFFFFFFFF
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)


def write_png(path: Path, width: int, height: int, rgb: tuple[int, int, int]) -> None:
    raw = b"".join(b"\x00" + bytes(rgb) * width for _ in range(height))
    ihdr = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    body = b"\x89PNG\r\n\x1a\n"
    body += png_chunk(b"IHDR", ihdr)
    body += png_chunk(b"IDAT", zlib.compress(raw, 9))
    body += png_chunk(b"IEND", b"")
    path.write_bytes(body)


def write_wav(path: Path, seconds: float = 0.25, rate: int = 22050) -> None:
    frames = int(rate * seconds)
    data = b"\x00\x00" * frames
    path.write_bytes(
        b"RIFF"
        + struct.pack("<I", 36 + len(data))
        + b"WAVEfmt "
        + struct.pack("<IHHIIHH", 16, 1, 1, rate, rate * 2, 2, 16)
        + b"data"
        + struct.pack("<I", len(data))
        + data
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("directory")
    args = parser.parse_args()
    directory = Path(args.directory)
    directory.mkdir(parents=True, exist_ok=True)
    write_png(directory / "banner.png", 256, 128, (26, 51, 68))
    write_wav(directory / "banner.wav")


if __name__ == "__main__":
    main()
