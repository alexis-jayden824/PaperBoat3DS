#!/usr/bin/env python3

import argparse
import struct
import zipfile
from pathlib import Path
from typing import Optional


TEXTURE_NAME = "backgrounds/title_bg"
PALETTE_NAME = "backgrounds/title_bg_pal0"
TEXTURE_TYPE = 0x4F544558


def resource_header() -> bytes:
    header = bytearray(64)
    header[0] = 0  # Ship::Endianness::Little
    header[4:8] = struct.pack("<I", TEXTURE_TYPE)
    header[8:12] = struct.pack("<I", 0)
    return bytes(header)


def texture_resource(texture_type: int = 4) -> bytes:
    width, height = 296, 200
    image = bytes((x + y) & 3 for y in range(height) for x in range(width))
    return resource_header() + struct.pack(
        "<IIII", texture_type, width, height, len(image)
    ) + image


def palette_resource() -> bytes:
    colors = [0xF801, 0x07C1, 0x003F, 0xFFFF] + [0] * 252
    image = b"".join(struct.pack(">H", color) for color in colors)
    return resource_header() + struct.pack("<IIII", 2, 256, 1, len(image)) + image


def write_entry(archive: zipfile.ZipFile, name: str, data: bytes, method: int) -> None:
    info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
    info.compress_type = method
    info.external_attr = 0o100644 << 16
    with archive.open(info, "w", force_zip64=True) as destination:
        destination.write(data)


def write_fixture(path: Path, method: int, include_palette: bool = True,
                  texture_type: int = 4) -> None:
    with zipfile.ZipFile(path, "w", allowZip64=True) as archive:
        write_entry(archive, "audio/synthetic", b"not game data", method)
        write_entry(archive, TEXTURE_NAME, texture_resource(texture_type), method)
        if include_palette:
            write_entry(archive, PALETTE_NAME, palette_resource(), method)


def corrupt_central_crc(path: Path, entry_name: str) -> None:
    data = bytearray(path.read_bytes())
    offset = 0
    while True:
        offset = data.find(b"PK\x01\x02", offset)
        if offset < 0:
            raise RuntimeError(f"central entry not found: {entry_name}")
        name_size, extra_size, comment_size = struct.unpack_from(
            "<HHH", data, offset + 28
        )
        name_start = offset + 46
        name = bytes(data[name_start:name_start + name_size]).decode("utf-8")
        if name == entry_name:
            crc = struct.unpack_from("<I", data, offset + 16)[0]
            struct.pack_into("<I", data, offset + 16, crc ^ 0xFFFFFFFF)
            path.write_bytes(data)
            return
        offset = name_start + name_size + extra_size + comment_size


def patch_entry(path: Path, entry_name: str, *, method: Optional[int] = None,
                encrypted: bool = False,
                uncompressed_size: Optional[int] = None,
                corrupt_local_name: bool = False) -> None:
    data = bytearray(path.read_bytes())
    offset = 0
    while True:
        offset = data.find(b"PK\x01\x02", offset)
        if offset < 0:
            raise RuntimeError(f"central entry not found: {entry_name}")
        name_size, extra_size, comment_size = struct.unpack_from(
            "<HHH", data, offset + 28
        )
        name_start = offset + 46
        name = bytes(data[name_start:name_start + name_size]).decode("utf-8")
        if name == entry_name:
            local_offset = struct.unpack_from("<I", data, offset + 42)[0]
            if method is not None:
                struct.pack_into("<H", data, offset + 10, method)
                struct.pack_into("<H", data, local_offset + 8, method)
            if encrypted:
                central_flags = struct.unpack_from("<H", data, offset + 8)[0]
                local_flags = struct.unpack_from("<H", data, local_offset + 6)[0]
                struct.pack_into("<H", data, offset + 8, central_flags | 1)
                struct.pack_into("<H", data, local_offset + 6, local_flags | 1)
            if uncompressed_size is not None:
                struct.pack_into("<I", data, offset + 24, uncompressed_size)
            if corrupt_local_name:
                local_name_start = local_offset + 30
                data[local_name_start] ^= 1
            path.write_bytes(data)
            return
        offset = name_start + name_size + extra_size + comment_size


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    write_fixture(args.output / "valid-deflate.o2r", zipfile.ZIP_DEFLATED)
    write_fixture(args.output / "valid-stored.o2r", zipfile.ZIP_STORED)
    write_fixture(args.output / "missing-palette.o2r", zipfile.ZIP_DEFLATED,
                  include_palette=False)
    write_fixture(args.output / "invalid-texture.o2r", zipfile.ZIP_DEFLATED,
                  texture_type=1)
    corrupt_path = args.output / "bad-crc.o2r"
    write_fixture(corrupt_path, zipfile.ZIP_DEFLATED)
    corrupt_central_crc(corrupt_path, TEXTURE_NAME)

    unsupported_path = args.output / "unsupported-method.o2r"
    write_fixture(unsupported_path, zipfile.ZIP_DEFLATED)
    patch_entry(unsupported_path, TEXTURE_NAME, method=12)

    encrypted_path = args.output / "encrypted.o2r"
    write_fixture(encrypted_path, zipfile.ZIP_DEFLATED)
    patch_entry(encrypted_path, TEXTURE_NAME, encrypted=True)

    oversized_path = args.output / "oversized.o2r"
    write_fixture(oversized_path, zipfile.ZIP_DEFLATED)
    patch_entry(oversized_path, TEXTURE_NAME, uncompressed_size=65537)

    local_name_path = args.output / "bad-local-name.o2r"
    write_fixture(local_name_path, zipfile.ZIP_DEFLATED)
    patch_entry(local_name_path, TEXTURE_NAME, corrupt_local_name=True)


if __name__ == "__main__":
    main()
