#!/bin/sh
set -eu
root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
output=${1:-"$root/build/m13-resource-tests"}
mkdir -p "$output/sdmc"
python3 - "$output/sdmc/paperboat.o2r" <<'PY'
import struct
import sys
import zipfile

def otr(kind, body):
    header = bytearray(64)
    header[4:8] = struct.pack('<I', kind)
    return bytes(header) + body

with zipfile.ZipFile(sys.argv[1], 'w', compression=zipfile.ZIP_STORED) as archive:
    archive.writestr('data/one', otr(0x4f424c42, struct.pack('<I', 4) + b'ABCD'))
    archive.writestr('textures/one', otr(0x4f544558,
        struct.pack('<IIII', 1, 1, 1, 2) + bytes.fromhex('f801')))
    archive.writestr('lists/one', otr(0x4f444c54,
        bytes([4, 0, 0, 0, 0, 0, 0, 0]) + struct.pack('<II', 0xdf000000, 0)))
    archive.writestr('invalid/one', otr(0x4f424c42, struct.pack('<I', 99) + b'ABCD'))
PY
${HOST_CC:-cc} -std=c11 -O2 -Wall -Wextra -Werror -DPB3DS_GAME_OBJECTS \
    -I"$root/include" "$root/source/fs.c" \
    "$root/source/game_resources.c" "$root/tests/test_game_resources.c" \
    -o "$output/test_game_resources"
"$output/test_game_resources" "$output/sdmc"
