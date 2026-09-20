#!/bin/sh
set -eu
project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
out=${1:-"$project_root/build/runtime-resource-tests"}
cc=${HOST_CC:-cc}
sanitizer_flags=""
if [ "${SANITIZE:-0}" = 1 ]; then
    sanitizer_flags="-g -fno-omit-frame-pointer -fsanitize=address,undefined"
fi
upstream="$project_root/.cache/upstream/PaperBoat"
zlib="$upstream/external/torch/lib/StormLib/src/zlib"
mkdir -p "$out"
python3 "$project_root/tests/make_runtime_fixture.py" "$out/fixtures"
objects=""
for file in adler32 inffast inflate inftrees zutil; do
    "$cc" $sanitizer_flags -O2 -DNO_GZIP -I"$zlib" -c "$zlib/$file.c" -o "$out/z-$file.o"
    objects="$objects $out/z-$file.o"
done
for file in "$upstream/src/port/shape_loader.c" "$project_root/tests/test_runtime_upstream_consumer.c"; do
    object="$out/$(basename "$file").o"
    "$cc" $sanitizer_flags -std=gnu11 -O2 -D_LANGUAGE_C -DPORT -DMODERN_COMPILER \
        -DVERSION=us -DVERSION_US -DF3DEX_GBI_2 -D__CTX__ \
        -I"$project_root/include" -I"$upstream/include" -I"$upstream/src" \
        -I"$upstream/src/port" -I"$upstream/external/libultraship/include" \
        -c "$file" -o "$object"
    objects="$objects $object"
done
"$cc" $sanitizer_flags -std=c11 -O2 -Wall -Wextra -Werror \
    -I"$project_root/tests/mocks" -I"$project_root/include" -I"$zlib" \
    "$project_root/source/compat.c" "$project_root/source/memory.c" \
    "$project_root/source/o2r.c" "$project_root/source/texture.c" \
    "$project_root/source/runtime_resources.c" \
    "$project_root/tests/test_runtime_resources.c" $objects -o "$out/test"
"$out/test" "$out/fixtures/stored.o2r"
"$out/test" "$out/fixtures/deflate.o2r"
