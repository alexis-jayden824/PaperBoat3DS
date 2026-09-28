#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_directory=${1:-"$project_root/build/m13-tests"}
host_cc=${HOST_CC:-cc}
zlib_root="$project_root/.cache/upstream/PaperBoat/external/torch/lib/StormLib/src/zlib"
fixture_directory="$build_directory/fixtures"
test_binary="$build_directory/test_world_boot"

if [ ! -f "$zlib_root/inflate.c" ]; then
    echo "M13 world-boot test: fetch pinned upstream sources first" >&2
    exit 1
fi

mkdir -p "$build_directory"
python3 "$project_root/tests/make_m13_fixture.py" "$fixture_directory"

zlib_objects=""
for source in adler32 inffast inflate inftrees zutil; do
    object="$build_directory/zlib-$source.o"
    "$host_cc" -std=c11 -O2 -Wall -Wextra -Werror \
        -Wno-endif-labels -Wno-shift-negative-value \
        -Wno-implicit-fallthrough -DNO_GZIP -I"$zlib_root" \
        -c "$zlib_root/$source.c" -o "$object"
    zlib_objects="$zlib_objects $object"
done

"$host_cc" -std=c11 -O2 -Wall -Wextra -Werror \
    -I"$project_root/tests/mocks" -I"$project_root/include" \
    -I"$zlib_root" \
    "$project_root/source/compat.c" \
    "$project_root/source/memory.c" \
    "$project_root/source/o2r.c" \
    "$project_root/source/texture.c" \
    "$project_root/source/world_boot.c" \
    "$project_root/tests/test_world_boot.c" \
    $zlib_objects -o "$test_binary"

if [ "$#" -ge 2 ]; then
    "$test_binary" "$fixture_directory" "$2"
else
    "$test_binary" "$fixture_directory"
fi
