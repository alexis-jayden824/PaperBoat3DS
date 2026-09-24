#!/usr/bin/env python3

import argparse
import struct
import zipfile
from pathlib import Path
from typing import Optional


SHAPE_NAME = "shapes/mac_00_shape"
COLLISION_NAME = "collisions/mac_00_hit"
VERTEX_NAME = "shapes/mac_00_shape/vtx"
DISPLAY_LIST_NAME = "shapes/mac_00_shape/dlist_20"
BACKGROUND_NAME = "backgrounds/nok_bg"
PALETTE_NAME = "backgrounds/nok_bg_pal0"
MAP_TEXTURE_NAME = "textures/mac_tex/test_tex"
PLAYER_FRAME_0_NAME = "sprites/player_sprite_0_raster_0"
PLAYER_FRAME_2_NAME = "sprites/player_sprite_0_raster_2"
PLAYER_PALETTE_NAME = "sprites/player_sprite_0_pal_0"
STAR_PIECE_NAME = "icons/anim/star_piece_0"
STAR_PIECE_PALETTE_NAME = "icons/anim/star_piece_0.pal"

BLOB_TYPE = 0x4F424C42
DISPLAY_LIST_TYPE = 0x4F444C54
VERTEX_TYPE = 0x4F565458
TEXTURE_TYPE = 0x4F544558


def resource_header(resource_type: int) -> bytes:
    header = bytearray(64)
    header[0] = 0
    header[4:8] = struct.pack("<I", resource_type)
    header[8:12] = struct.pack("<I", 0)
    return bytes(header)


def blob_resource(data: bytes) -> bytes:
    return resource_header(BLOB_TYPE) + struct.pack("<I", len(data)) + data


def shape_resource(valid: bool = True) -> bytes:
    data = bytearray(256)
    struct.pack_into("<IIIII", data, 0, 32, 64, 200, 208, 216)
    struct.pack_into("<iIiII", data, 32, 7, 0, 0, 0, 128)
    struct.pack_into("<iIiII", data, 96, 2, 180, 0, 0, 0)
    struct.pack_into("<IIiiI", data, 128, 0, 0, 0, 1, 152)
    struct.pack_into("<I", data, 152, 96)
    struct.pack_into("<II", data, 180, 0x20 if valid else 0, 0)
    struct.pack_into("<I", data, 200, 224)
    struct.pack_into("<I", data, 208, 227)
    struct.pack_into("<I", data, 216, 230)
    data[224:233] = b"db\0db\0db\0"
    return blob_resource(bytes(data))


def write_hit_section(data: bytearray, header: int, collider: int,
                      vertices: int, bounds: int, triangle: int) -> None:
    struct.pack_into("<h2xIh2xIh2xI", data, header,
                     1, collider, 3, vertices, 7, bounds)
    struct.pack_into("<hhhHI", data, collider, 0, -1, -1, 1, triangle)
    struct.pack_into("<hhhhhhhhh", data, vertices,
                     0, 0, 0, 100, 0, 0, 0, 0, 100)
    struct.pack_into("<7I", data, bounds, *([0] * 7))
    struct.pack_into("<I", data, triangle, 0 | (1 << 10) | (2 << 20))


def collision_resource(valid: bool = True) -> bytes:
    data = bytearray(256)
    struct.pack_into("<II", data, 0, 8, 128 if valid else 300)
    write_hit_section(data, 8, 32, 44, 64, 92)
    write_hit_section(data, 128, 152, 164, 184, 212)
    return blob_resource(bytes(data))


def vertex_resource(valid: bool = True) -> bytes:
    body = bytearray(struct.pack("<I", 3 if valid else 4))
    for x, y, z in ((0, 0, 0), (100, 0, 0), (0, 0, 100)):
        body.extend(struct.pack("<hhhHhhBBBB", x, y, z, 0, 0, 0,
                                255, 255, 255, 255))
    return resource_header(VERTEX_TYPE) + bytes(body)


def display_list_resource(valid: bool = True) -> bytes:
    body = bytearray((4, 0, 0, 0, 0, 0, 0, 0))
    body.extend(struct.pack("<II", 0xE7000000, 0))
    if valid:
        body.extend(struct.pack("<II", 0xDF000000, 0))
    return resource_header(DISPLAY_LIST_TYPE) + bytes(body)


def scene_shape_resource(map_id: str) -> bytes:
    data = bytearray(768)
    struct.pack_into("<IIIII", data, 0, 32, 64, 300, 320, 380)
    struct.pack_into("<iIiII", data, 32, 2, 64, 2, 80, 0)
    struct.pack_into("<II", data, 64, 0x20, 0)
    struct.pack_into("<III", data, 80, 0x5E, 0, 200)
    struct.pack_into("<III", data, 92, 0x5C, 0, 1)
    data[200:209] = b"test_tex\0"

    colliders = (["ground", "deilie", "sign"] if map_id == "mac_00"
                 else ["ground", "deiliw", "unused"])
    string_offset = 600
    data[string_offset:string_offset + 3] = b"db\0"
    struct.pack_into("<I", data, 300, string_offset)
    struct.pack_into("<I", data, 380, string_offset)
    string_offset += 4
    for index, name in enumerate(colliders + ["db"]):
        encoded = name.encode("ascii") + b"\0"
        struct.pack_into("<I", data, 320 + index * 4, string_offset)
        data[string_offset:string_offset + len(encoded)] = encoded
        string_offset += len(encoded)
    return blob_resource(bytes(data))


def packed_triangle(first: int, second: int, third: int) -> int:
    return first | (second << 10) | (third << 20)


def scene_collision_resource(map_id: str) -> bytes:
    data = bytearray(512)
    section = 8
    collider_offset = 32
    vertex_offset = 96
    bounds_offset = 160
    triangle_offset = 256
    struct.pack_into("<II", data, 0, section, section)
    struct.pack_into("<H2xIH2xIH2xI", data, section,
                     3, collider_offset, 8, vertex_offset,
                     21, bounds_offset)

    if map_id == "mac_00":
        exit_min_x, exit_max_x = 550, 700
        sign_bounds = (-130.0, 0.0, -360.0,
                       -70.0, 80.0, -330.0)
    else:
        exit_min_x, exit_max_x = -700, -550
        sign_bounds = (0.0, 0.0, 0.0, 10.0, 10.0, 10.0)
    vertices = [
        (-800, 0, -800), (800, 0, -800),
        (800, 0, 800), (-800, 0, 800),
        (exit_min_x, 0, -100), (exit_max_x, 0, -100),
        (exit_max_x, 0, 100), (exit_min_x, 0, 100),
    ]
    for index, vertex in enumerate(vertices):
        struct.pack_into("<hhh", data, vertex_offset + index * 6, *vertex)

    struct.pack_into("<hhhHI", data, collider_offset,
                     0, -1, -1, 2, triangle_offset)
    struct.pack_into("<hhhHI", data, collider_offset + 12,
                     7, -1, -1, 2, triangle_offset + 8)
    struct.pack_into("<hhhHI", data, collider_offset + 24,
                     14, -1, -1, 0, 0)

    bounds = [
        (-800.0, -1.0, -800.0, 800.0, 1.0, 800.0),
        (float(exit_min_x), -1.0, -100.0,
         float(exit_max_x), 1.0, 100.0),
        sign_bounds,
    ]
    for index, values in enumerate(bounds):
        struct.pack_into("<6fI", data, bounds_offset + index * 28,
                         *values, 0)

    triangles = [
        packed_triangle(0, 1, 2), packed_triangle(0, 2, 3),
        packed_triangle(4, 5, 6), packed_triangle(4, 6, 7),
    ]
    for index, triangle in enumerate(triangles):
        struct.pack_into("<I", data, triangle_offset + index * 4, triangle)
    return blob_resource(bytes(data))


def scene_vertex_resource(map_id: str) -> bytes:
    if map_id == "mac_00":
        positions = ((-250, 0, -500), (50, 0, -500), (-100, 0, -250))
    else:
        positions = ((-750, 0, -150), (-450, 0, -150), (-600, 0, 150))
    body = bytearray(struct.pack("<I", 3))
    texcoords = ((0, 0), (8 * 32, 0), (4 * 32, 8 * 32))
    for (x, y, z), (s, t) in zip(positions, texcoords):
        body.extend(struct.pack("<hhhHhhBBBB", x, y, z, 0, s, t,
                                255, 255, 255, 255))
    return resource_header(VERTEX_TYPE) + bytes(body)


def scene_display_list_resource() -> bytes:
    body = bytearray((4, 0, 0, 0, 0, 0, 0, 0))
    body.extend(struct.pack("<II", 0x33000000, 0))
    body.extend(struct.pack("<II", 0, 0x1234))
    body.extend(struct.pack("<II", 0x32003006, 0))
    body.extend(struct.pack("<II", 0, 0))
    body.extend(struct.pack("<II", 0x05000204, 0))
    body.extend(struct.pack("<II", 0xDF000000, 0))
    return resource_header(DISPLAY_LIST_TYPE) + bytes(body)


def texture_resource(texture_format: int, width: int, height: int,
                     image: bytes) -> bytes:
    return resource_header(TEXTURE_TYPE) + struct.pack(
        "<IIII", texture_format, width, height, len(image)
    ) + image


def background_resource(valid: bool = True) -> bytes:
    pixels = bytes((x + y) & 0xFF
                   for y in range(200) for x in range(296))
    return texture_resource(4, 296 if valid else 295, 200, pixels)


def palette_resource(valid: bool = True) -> bytes:
    colors = bytearray()
    for index in range(256):
        red = (index >> 3) & 0x1F
        green = (index >> 3) & 0x1F
        blue = (index >> 3) & 0x1F
        colors.extend(struct.pack(">H", (red << 11) | (green << 6) |
                                  (blue << 1) | 1))
    return texture_resource(2, 256 if valid else 255, 1, bytes(colors))


def ci4_resource(width: int, height: int, phase: int = 0) -> bytes:
    pixels = bytearray()
    for texel in range(0, width * height, 2):
        first = ((texel // width) + (texel % width) + phase) & 0xF
        second = (((texel + 1) // width) + ((texel + 1) % width) +
                  phase) & 0xF
        pixels.append((first << 4) | second)
    return texture_resource(3, width, height, bytes(pixels))


def ci4_palette_resource() -> bytes:
    colors = bytearray()
    for index in range(16):
        red = min(31, index * 2)
        green = min(31, 8 + index)
        blue = min(31, 31 - index)
        colors.extend(struct.pack(">H", (red << 11) | (green << 6) |
                                  (blue << 1) | (0 if index == 0 else 1)))
    return texture_resource(2, 16, 1, bytes(colors))


def write_scene_fixture(path: Path, method: int) -> None:
    entries = {
        BACKGROUND_NAME: background_resource(),
        PALETTE_NAME: palette_resource(),
        MAP_TEXTURE_NAME: texture_resource(6, 8, 8, bytes(range(64))),
        PLAYER_FRAME_0_NAME: ci4_resource(32, 56),
        PLAYER_FRAME_2_NAME: ci4_resource(32, 56, 2),
        PLAYER_PALETTE_NAME: ci4_palette_resource(),
        STAR_PIECE_NAME: ci4_resource(32, 32, 4),
        STAR_PIECE_PALETTE_NAME: ci4_palette_resource(),
    }
    for map_id in ("mac_00", "mac_01"):
        entries[f"shapes/{map_id}_shape"] = scene_shape_resource(map_id)
        entries[f"collisions/{map_id}_hit"] = scene_collision_resource(map_id)
        entries[f"shapes/{map_id}_shape/vtx"] = scene_vertex_resource(map_id)
        entries[f"shapes/{map_id}_shape/dlist_20"] = (
            scene_display_list_resource())
    with zipfile.ZipFile(path, "w", allowZip64=True) as archive:
        write_entry(archive, "synthetic/readme", b"public scene test data",
                    method)
        for name, data in entries.items():
            write_entry(archive, name, data, method)


def write_entry(archive: zipfile.ZipFile, name: str, data: bytes,
                method: int) -> None:
    info = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
    info.compress_type = method
    info.external_attr = 0o100644 << 16
    with archive.open(info, "w", force_zip64=True) as destination:
        destination.write(data)


def write_fixture(path: Path, method: int, *, omit: Optional[str] = None,
                  valid_shape: bool = True,
                  valid_collision: bool = True,
                  valid_vertices: bool = True,
                  valid_display_list: bool = True,
                  valid_background: bool = True,
                  valid_palette: bool = True) -> None:
    entries = {
        SHAPE_NAME: shape_resource(valid_shape),
        COLLISION_NAME: collision_resource(valid_collision),
        VERTEX_NAME: vertex_resource(valid_vertices),
        DISPLAY_LIST_NAME: display_list_resource(valid_display_list),
        BACKGROUND_NAME: background_resource(valid_background),
        PALETTE_NAME: palette_resource(valid_palette),
    }
    with zipfile.ZipFile(path, "w", allowZip64=True) as archive:
        write_entry(archive, "synthetic/readme", b"public test data", method)
        for name, data in entries.items():
            if name != omit:
                write_entry(archive, name, data, method)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    write_fixture(args.output / "valid-deflate.o2r", zipfile.ZIP_DEFLATED)
    write_fixture(args.output / "valid-stored.o2r", zipfile.ZIP_STORED)
    write_fixture(args.output / "missing-shape.o2r", zipfile.ZIP_DEFLATED,
                  omit=SHAPE_NAME)
    write_fixture(args.output / "invalid-shape.o2r", zipfile.ZIP_DEFLATED,
                  valid_shape=False)
    write_fixture(args.output / "invalid-collision.o2r",
                  zipfile.ZIP_DEFLATED, valid_collision=False)
    write_fixture(args.output / "invalid-vertices.o2r",
                  zipfile.ZIP_DEFLATED, valid_vertices=False)
    write_fixture(args.output / "invalid-display-list.o2r",
                  zipfile.ZIP_DEFLATED, valid_display_list=False)
    write_fixture(args.output / "invalid-background.o2r",
                  zipfile.ZIP_DEFLATED, valid_background=False)
    write_fixture(args.output / "invalid-palette.o2r",
                  zipfile.ZIP_DEFLATED, valid_palette=False)
    write_scene_fixture(args.output / "scene-deflate.o2r",
                        zipfile.ZIP_DEFLATED)
    write_scene_fixture(args.output / "scene-stored.o2r",
                        zipfile.ZIP_STORED)


if __name__ == "__main__":
    main()
