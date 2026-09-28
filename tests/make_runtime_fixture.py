"""Synthetic native-resource bridge cases; no proprietary input."""
import struct
import sys
import zipfile
from pathlib import Path
from make_m13_fixture import shape_resource, display_list_resource


def path_crc64(text: str) -> int:
    value = (1 << 64) - 1
    for byte in text.encode("utf-8"):
        value ^= byte << 56
        for _ in range(8):
            value = ((value << 1) ^ 0x42F0E1EBA9EA3693
                     if value & (1 << 63) else value << 1)
            value &= (1 << 64) - 1
    return value


def hash_command(opcode: int, name: str, word1: int = 0) -> bytes:
    value = path_crc64(name)
    return struct.pack("<IIII", opcode << 24, word1,
                       value >> 32, value & 0xFFFFFFFF)


def closure_display_list(*commands: bytes) -> bytes:
    body = bytearray((4, 0, 0, 0, 0, 0, 0, 0))
    for command in commands:
        body.extend(command)
    body.extend(struct.pack("<II", 0xDF000000, 0))
    return resource(0x4F444C54, body)


def resource(kind, body, big=False, version=0):
    h = bytearray(64)
    h[0] = int(big)
    struct.pack_into('>II' if big else '<II', h, 4, kind, version)
    return h + body


def sparse_player_sprite(width=8, height=8, image_offset=0x1000,
                         palette_offset=44):
    """One raster whose image lives in the separate player-raster archive."""
    blob = bytearray()
    blob += struct.pack('<IIii', 20, 28, 1, 1)
    blob += struct.pack('<I', 0xFFFFFFFF)  # no animations (back sprite)
    blob += struct.pack('<II', 36, 0xFFFFFFFF)
    blob += struct.pack('<II', palette_offset, 0xFFFFFFFF)
    blob += struct.pack('<IBBbb', image_offset, width, height, 0, -1)
    blob += bytes(32)  # embedded palette fallback
    return resource(0x4F424C42, struct.pack('<I', len(blob)) + blob)


def component_sprite(command_offset=52, command_size=2):
    """One animation/component, with caller-controlled command bounds."""
    blob = bytearray()
    blob += struct.pack('<IIii', 24, 28, 1, 1)
    blob += struct.pack('<II', 32, 0xFFFFFFFF)
    blob += struct.pack('<I', 0xFFFFFFFF)  # no rasters
    blob += struct.pack('<I', 0xFFFFFFFF)  # no palettes
    blob += struct.pack('<II', 40, 0xFFFFFFFF)
    blob += struct.pack('<Ihhhh', command_offset, command_size, 0, 0, 0)
    blob += struct.pack('<H', 1)  # wait one frame
    return resource(0x4F424C42, struct.pack('<I', len(blob)) + blob)


out = Path(sys.argv[1])
out.mkdir(parents=True, exist_ok=True)
for compression, label in ((zipfile.ZIP_STORED, 'stored'), (zipfile.ZIP_DEFLATED, 'deflate')):
    with zipfile.ZipFile(out / f'{label}.o2r', 'w', compression=compression) as z:
        z.writestr('shapes/mac_00_shape', shape_resource())
        z.writestr('shapes/mac_00_shape/dlist_20', display_list_resource())
        z.writestr('shapes/closure_shape',
                   shape_resource(texture_name='test_tex'))
        z.writestr('shapes/closure_shape/dlist_20', closure_display_list(
            hash_command(0x32, 'le/vertex'),
            hash_command(0x36, 'le/matrix'),
            hash_command(0x42, 'le/lights', (10 << 24) | (48 << 16)),
            hash_command(0x42, 'le/viewport', 8 << 24),
            hash_command(0x20, 'le/texture'),
            hash_command(0x31, 'closure/nested'),
        ))
        z.writestr('closure/nested', closure_display_list())
        z.writestr('shapes/missing_shape', shape_resource())
        z.writestr('shapes/wrong_shape', shape_resource())
        z.writestr('shapes/wrong_shape/dlist_20', closure_display_list(
            hash_command(0x32, 'le/blob'),
        ))
        z.writestr('shapes/hash_missing_shape', shape_resource())
        z.writestr('shapes/hash_missing_shape/dlist_20', closure_display_list(
            hash_command(0x31, 'closure/not-present'),
        ))
        map_raster = struct.pack('<IIII', 3, 8, 8, 32) + bytes(32)
        map_palette = struct.pack('<IIII', 2, 16, 1, 32) + bytes(32)
        z.writestr('textures/runtime_tex/test_tex',
                   resource(0x4F544558, map_raster))
        z.writestr('textures/runtime_tex/test_tex_tlut',
                   resource(0x4F544558, map_palette))
        for big in (False, True):
            endian, tag = ('>', 'be') if big else ('<', 'le')
            z.writestr(f'{tag}/blob', resource(0x4F424C42, struct.pack(endian+'I', 4)+b'abc\0', big))
            v = struct.pack(endian+'IhhhHhhBBBB', 1, -7, 300, -123, 9, -32, 96, 1, 2, 3, 255)
            z.writestr(f'{tag}/vertex', resource(0x4F565458, v, big))
            t = struct.pack(endian+'IIII', 2, 1, 1, 2) + b'\xf8\x01'
            z.writestr(f'{tag}/texture', resource(0x4F544558, t, big))
            matrix = struct.pack(
                endian + '16I',
                0x00010000, 0x00000000, 0x00000000, 0x00000000,
                0x00000000, 0x00010000, 0x00000000, 0x00000000,
                0x00000000, 0x00000000, 0x00010000, 0x00000000,
                0x00000000, 0x00000000, 0x00000000, 0x00010000,
            )
            z.writestr(f'{tag}/matrix', resource(0x4F4D5458, matrix, big))
            vectors = struct.pack(
                endian + 'Ihhhhhh',
                2, -1, 2, -300, 32767, -32768, 1234,
            )
            z.writestr(f'{tag}/vec3s', resource(0x56433353, vectors, big))
            lights = bytes(range(24))
            z.writestr(f'{tag}/lights', resource(0x46669697, lights, big))
            viewport = struct.pack(endian + 'hhhhhhhh',
                                   640, 480, 511, 0,
                                   640, 480, 511, 0)
            z.writestr(f'{tag}/viewport',
                       resource(0x4F565054, viewport, big))
            # A hash payload with the ENDDL high byte must not terminate the list.
            d = bytes((4, 0, 0, 0, 0, 0, 0, 0)) + struct.pack(endian+'IIIIII', 0x33000000, 0, 0xDF123456, 0xCAFEBABE, 0xDF000000, 0)
            z.writestr(f'{tag}/dl', resource(0x4F444C54, d, big))
            # Multi-packet payload words that resemble ENDDL must never be
            # decoded as opcodes. TEXRECT consumes three packets and READFB
            # consumes two before the real terminator.
            span = bytes((4, 0, 0, 0, 0, 0, 0, 0)) + struct.pack(
                endian + 'IIIIIIIIIIII',
                0xE4000000, 0,
                0xDF111111, 0x22222222,
                0x33333333, 0x44444444,
                0x3E000001, 0x1000,
                0xDF000000, 0x00400020,
                0xDF000000, 0,
            )
            z.writestr(f'{tag}/span-dl', resource(0x4F444C54, span, big))
        z.writestr('bad/version', resource(0x4F424C42, b'', version=1))
        z.writestr('bad/type', resource(0x12345678, b''))
        z.writestr('bad/blob', resource(0x4F424C42, struct.pack('<I', 100)+b'x'))
        z.writestr('bad/vertex', resource(0x4F565458, struct.pack('<I', 2)+bytes(16)))
        z.writestr('bad/texture', resource(0x4F544558, struct.pack('<IIII', 2, 2, 2, 1)+b'x'))
        z.writestr('bad/matrix', resource(0x4F4D5458, bytes(63)))
        z.writestr('bad/lights', resource(0x46669697, bytes(23)))
        z.writestr('bad/viewport', resource(0x4F565054, bytes(15)))
        z.writestr('bad/vec3s', resource(
            0x56433353, struct.pack('<Ihhh', 2, 1, 2, 3)
        ))
        z.writestr('bad/dl', resource(0x4F444C54, bytes((4, 0, 0, 0, 0, 0, 0, 0))+struct.pack('<II', 0x33000000, 0)))
        z.writestr('bad/truncated-span-dl', resource(
            0x4F444C54,
            bytes((4, 0, 0, 0, 0, 0, 0, 0)) +
            struct.pack('<II', 0x3E000001, 0x1000),
        ))
        image_base = 0x100
        player_sets = tuple(range(14))
        player_descriptors = tuple(
            (0x20 << 16) | (image_base + index * 0x20)
            for index in range(13)
        )
        z.writestr('sprites/player_raster_header', resource(
            0x4F424C42,
            struct.pack('<I', 12) + struct.pack('>III', 0, 0, image_base),
        ))
        z.writestr('sprites/player_raster_sets', resource(
            0x4F424C42,
            struct.pack('<I', len(player_sets) * 4) +
            struct.pack('>' + 'I' * len(player_sets), *player_sets),
        ))
        z.writestr('sprites/player_raster_load_descriptors', resource(
            0x4F424C42,
            struct.pack('<I', len(player_descriptors) * 4) +
            struct.pack('>' + 'I' * len(player_descriptors),
                        *player_descriptors),
        ))
        player_images = bytes(len(player_descriptors) * 0x20)
        z.writestr('sprites/player_raster_image_data', resource(
            0x4F424C42,
            struct.pack('<I', len(player_images)) + player_images,
        ))
        z.writestr('sprites/player_sprite_1', sparse_player_sprite())
        # Companion sprite rasters are real Torch textures. Conversion now
        # decodes their metadata so malformed sprite art cannot pass startup.
        raster = struct.pack('<IIII', 3, 8, 8, 32) + bytes([0x12]) * 32
        z.writestr('sprites/player_sprite_1_raster_0', resource(0x4F544558, raster))
        z.writestr('sprites/player_sprite_2', sparse_player_sprite())
        z.writestr('sprites/player_sprite_3', sparse_player_sprite(255, 255))
        z.writestr('sprites/player_sprite_4', sparse_player_sprite())
        wrong_raster = struct.pack('<IIII', 3, 4, 4, 8) + bytes(8)
        z.writestr('sprites/player_sprite_4_raster_0', resource(0x4F544558, wrong_raster))
        z.writestr('sprites/npc_sprite_001', sparse_player_sprite())
        z.writestr('sprites/npc_sprite_002', component_sprite(53, 4))
        z.writestr('sprites/npc_sprite_003', component_sprite())
        # Both offsets begin inside the blob but their complete payloads do
        # not fit; preflight must validate the range, not only the first byte.
        z.writestr('sprites/npc_sprite_004',
                   sparse_player_sprite(image_offset=72))
        z.writestr('sprites/npc_sprite_005',
                   sparse_player_sprite(image_offset=44,
                                        palette_offset=72))
