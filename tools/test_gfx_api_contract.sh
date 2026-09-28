#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_directory=${1:-"$project_root/build/m10-api-tests"}
host_cc=${HOST_CC:-cc}
host_cxx=${HOST_CXX:-c++}
upstream_include="$project_root/.cache/upstream/PaperBoat/external/libultraship/include"
sanitizer_flags=""
if [ "${SANITIZE:-0}" = "1" ]; then
    sanitizer_flags="-g -fno-omit-frame-pointer -fsanitize=address,undefined"
fi

if [ ! -f "$upstream_include/fast/backends/gfx_rendering_api.h" ]; then
    echo "M10 API contract: fetch pinned upstream sources first" >&2
    exit 1
fi

mkdir -p "$build_directory"
"$host_cc" $sanitizer_flags -std=c11 -O2 -Wall -Wextra -Werror \
    -I"$project_root/include" \
    -c "$project_root/source/renderer.c" \
    -o "$build_directory/renderer.o"
"$host_cc" $sanitizer_flags -std=c11 -O2 -Wall -Wextra -Werror \
    -I"$project_root/include" \
    -c "$project_root/source/gfx_bridge.c" \
    -o "$build_directory/gfx_bridge.o"
"$host_cc" $sanitizer_flags -std=c11 -O2 -Wall -Wextra -Werror \
    -I"$project_root/include" \
    -c "$project_root/source/fast3d_semantics.c" \
    -o "$build_directory/fast3d_semantics.o"
"$host_cc" $sanitizer_flags -std=c11 -O2 -Wall -Wextra -Werror \
    -I"$project_root/include" \
    -c "$project_root/source/title_layout.c" \
    -o "$build_directory/title_layout.o"
"$host_cxx" $sanitizer_flags -std=gnu++17 -O2 -Wall -Wextra -Werror \
    -Wno-unused-parameter \
    -fno-exceptions -fno-rtti \
    -I"$project_root/tests/mocks" -I"$project_root/include" \
    -I"$upstream_include" \
    "$project_root/source/gfx_rendering_api_3ds.cpp" \
    "$project_root/source/runtime_gfx.cpp" \
    "$project_root/tests/test_gfx_api_contract.cpp" \
    "$build_directory/renderer.o" "$build_directory/gfx_bridge.o" \
    "$build_directory/fast3d_semantics.o" \
    "$build_directory/title_layout.o" \
    -o "$build_directory/test_gfx_api_contract"

if [ "${SANITIZE:-0}" = "1" ]; then
    ASAN_OPTIONS=detect_leaks=0 UBSAN_OPTIONS=halt_on_error=1 \
        "$build_directory/test_gfx_api_contract"
else
    "$build_directory/test_gfx_api_contract"
fi
