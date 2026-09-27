#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_directory=${1:-"$project_root/build/m5-tests"}
host_cc=${HOST_CC:-cc}
test_binary="$build_directory/test_m5"
lock="$project_root/upstream/PAPERBOAT.lock"
cmake_lists=${PB3DS_PAPERBOAT_ROOT:-"$project_root/.cache/upstream/PaperBoat"}/CMakeLists.txt
exclusions="$project_root/upstream/PAPERBOAT.exclusions"

fail() {
    printf '%s\n' "$*" >&2
    exit 1
}

value() {
    awk -F= -v key="$1" '$1 == key { print $2; found=1 } END { exit found ? 0 : 1 }' "$lock"
}

commit=$(value PAPERBOAT_COMMIT)

sh "$project_root/tools/fetch_paperboat.sh"
sh "$project_root/tools/write_paperboat_config.sh" "$build_directory/paperboat_config.h"

[ -f "$cmake_lists" ] || fail "PaperBoat CMakeLists.txt missing after fetch"
while IFS= read -r pattern || [ -n "$pattern" ]; do
    case "$pattern" in
        '' | \#*) continue ;;
    esac
    grep -F "$pattern" "$cmake_lists" >/dev/null ||
        fail "CMakeLists.txt missing exclusion $pattern"
done <"$exclusions"

# Desktop isolation: fetch and ARM wrappers must not pull desktop engines.
if grep -nE 'Game\.cpp|Engine\.cpp|SDL2|Torch-LH|libultraship' \
    "$project_root/source/pb_upstream_libc_compat.c" \
    "$project_root/source/pb_upstream_yay0.c" \
    "$project_root/source/paperboat.c" \
    "$project_root/tools/fetch_paperboat.sh"; then
    fail "M5 slice must not compile desktop PaperBoat/SDL/Torch/LUS"
fi
if grep -n '#include[[:space:]]*<3ds.h>' "$project_root/source/paperboat.c" \
    "$project_root/source/pb_upstream_yay0.c" \
    "$project_root/source/pb_upstream_libc_compat.c"; then
    fail "PaperBoat slice wrappers must not include 3ds.h"
fi

grep -q 'fetch_paperboat' "$project_root/tools/fetch_paperboat.sh" || true
grep -q 'PAPERBOAT_REPOSITORY' "$project_root/tools/fetch_paperboat.sh"
if grep -q 'TORCH_REPOSITORY' "$project_root/tools/fetch_paperboat.sh"; then
    fail "fetch_paperboat.sh must not clone Torch"
fi
if grep -q 'LIBULTRASHIP_REPOSITORY' "$project_root/tools/fetch_paperboat.sh"; then
    fail "fetch_paperboat.sh must not clone libultraship (M6)"
fi

mkdir -p "$build_directory"
"$host_cc" \
    -std=c11 -O2 -Wall -Wextra -Werror \
    -I"$project_root/include" \
    -I"$project_root/include/pb3ds/n64shim" \
    -I"$build_directory" \
    -DPB3DS_PAPERBOAT_COMMIT=\"$commit\" \
    -DPB3DS_PAPERBOAT_RELEASE=\"1.0.1\" \
    "$project_root/source/pb_upstream_libc_compat.c" \
    "$project_root/source/pb_upstream_yay0.c" \
    "$project_root/source/paperboat.c" \
    "$project_root/tests/test_m5.c" \
    -o "$test_binary"

"$test_binary"
echo "M5 PaperBoat slice contract passed"
