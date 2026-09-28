"""Synthetic native-resource bridge cases; no proprietary input."""
import struct
import sys
import zipfile
from pathlib import Path
from make_m13_fixture import shape_resource, display_list_resource


def resource(kind, body, big=False, version=0):
    h = bytearray(64)
    h[0] = int(big)
    struct.pack_into('>II' if big else '<II', h, 4, kind, version)
    return h + body


def sparse_player_sprite(width=8, height=8):
    """One raster whose image lives in the separate player-raster archive."""
    blob = bytearray()
    blob += struct.pack('<IIii', 20, 28, 1, 1)
    blob += struct.pack('<I', 0xFFFFFFFF)  # no animations (back sprite)
    blob += struct.pack('<II', 36, 0xFFFFFFFF)
    blob += struct.pack('<II', 44, 0xFFFFFFFF)
    blob += struct.pack('<IBBbb', 0x1000, width, height, 0, -1)
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
        for big in (False, True):
            endian, tag = ('>', 'be') if big else ('<', 'le')
            z.writestr(f'{tag}/blob', resource(0x4F424C42, struct.pack(endian+'I', 4)+b'abc\0', big))
            v = struct.pack(endian+'IhhhHhhBBBB', 1, -7, 300, -123, 9, -32, 96, 1, 2, 3, 255)
            z.writestr(f'{tag}/vertex', resource(0x4F565458, v, big))
            t = struct.pack(endian+'IIII', 2, 1, 1, 2) + b'\xf8\x01'
            z.writestr(f'{tag}/texture', resource(0x4F544558, t, big))
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
        z.writestr('bad/dl', resource(0x4F444C54, bytes((4, 0, 0, 0, 0, 0, 0, 0))+struct.pack('<II', 0x33000000, 0)))
        z.writestr('bad/truncated-span-dl', resource(
            0x4F444C54,
            bytes((4, 0, 0, 0, 0, 0, 0, 0)) +
            struct.pack('<II', 0x3E000001, 0x1000),
        ))
        z.writestr('sprites/player_sprite_1', sparse_player_sprite())
        # Metadata presence is enough for sprite conversion; the texture must
        # remain unloaded until the renderer actually requests it.
        z.writestr('sprites/player_sprite_1_raster_0', resource(0x4F424C42, struct.pack('<I', 1)+b'x'))
        z.writestr('sprites/player_sprite_2', sparse_player_sprite())
        z.writestr('sprites/player_sprite_3', sparse_player_sprite(255, 255))
        z.writestr('sprites/npc_sprite_001', sparse_player_sprite())
        z.writestr('sprites/npc_sprite_002', component_sprite(53, 4))
        z.writestr('sprites/npc_sprite_003', component_sprite())
