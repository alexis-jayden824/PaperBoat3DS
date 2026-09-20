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
        z.writestr('bad/version', resource(0x4F424C42, b'', version=1))
        z.writestr('bad/type', resource(0x12345678, b''))
        z.writestr('bad/blob', resource(0x4F424C42, struct.pack('<I', 100)+b'x'))
        z.writestr('bad/vertex', resource(0x4F565458, struct.pack('<I', 2)+bytes(16)))
        z.writestr('bad/texture', resource(0x4F544558, struct.pack('<IIII', 2, 2, 2, 1)+b'x'))
        z.writestr('bad/dl', resource(0x4F444C54, bytes((4, 0, 0, 0, 0, 0, 0, 0))+struct.pack('<II', 0x33000000, 0)))
