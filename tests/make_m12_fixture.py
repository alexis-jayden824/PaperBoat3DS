#!/usr/bin/env python3

import argparse
import struct
import zipfile
from pathlib import Path


LOGO_NAME = "title_screen/title_logo_img"
PROMPT_NAME = "title_screen/title_press_start_img"
COPYRIGHT_NAME = "title_screen/title_copyright_img"
TEXTURE_TYPE = 0x4F544558


def resource_header() -> bytes:
    header = bytearray(64)
    header[0] = 0
    header[4:8] = struct.pack("<I", TEXTURE_TYPE)
    header[8:12] = struct.pack("<I", 0)
    return bytes(header)


def texture_resource(texture_type: int, width: int, height: int,
                     image: bytes) -> bytes:
    return resource_header() + struct.pack(
        "<IIII", texture_type, width, height, len(image)
    ) + image


def logo_resource(texture_type: int = 1) -> bytes:
    image = bytearray()
    for y in range(112):
        for x in range(200):
            if y == 0:
                image.extend((255, x & 0xFF, 0, 255))
            elif y == 111:
                image.extend((0, x & 0xFF, 255, 255))
            else:
                image.extend((x & 0xFF, y & 0xFF, 96, 255))
    return texture_resource(texture_type, 200, 112, bytes(image))


def ia8_resource(width: int, height: int) -> bytes:
    image = bytearray(width * height)
    for y in range(height):
        packed = 0xF1 if y == 0 else (0x1F if y == height - 1 else 0x88)
        image[y * width:(y + 1) * width] = bytes([packed]) * width
    return texture_resource(8, width, height, bytes(image))


def write_entry(archive: zipfile.ZipFile, name: str, data: bytes,
                method: int) -> None:
    info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
    info.compress_type = method
    info.external_attr = 0o100644 << 16
    with archive.open(info, "w", force_zip64=True) as destination:
        destination.write(data)


def write_fixture(path: Path, method: int, *, include_prompt: bool = True,
                  logo_type: int = 1, prompt_width: int = 128) -> None:
    with zipfile.ZipFile(path, "w", allowZip64=True) as archive:
        write_entry(archive, "synthetic/readme", b"public test data", method)
        write_entry(archive, LOGO_NAME, logo_resource(logo_type), method)
        if include_prompt:
            write_entry(
                archive, PROMPT_NAME, ia8_resource(prompt_width, 32), method
            )
        write_entry(
            archive, COPYRIGHT_NAME, ia8_resource(144, 32), method
        )


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


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    write_fixture(args.output / "valid-deflate.o2r", zipfile.ZIP_DEFLATED)
    write_fixture(args.output / "valid-stored.o2r", zipfile.ZIP_STORED)
    write_fixture(args.output / "missing-prompt.o2r", zipfile.ZIP_DEFLATED,
                  include_prompt=False)
    write_fixture(args.output / "invalid-logo.o2r", zipfile.ZIP_DEFLATED,
                  logo_type=2)
    write_fixture(args.output / "invalid-prompt.o2r", zipfile.ZIP_DEFLATED,
                  prompt_width=127)
    corrupt_path = args.output / "bad-crc.o2r"
    write_fixture(corrupt_path, zipfile.ZIP_DEFLATED)
    corrupt_central_crc(corrupt_path, LOGO_NAME)


if __name__ == "__main__":
    main()
