#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_directory=${1:-"$project_root/build/m7-tests"}
host_cc=${HOST_CC:-cc}
test_binary="$build_directory/test_m7"
assets="$project_root/upstream/ASSETS.lock"
header="$project_root/include/pb3ds/assets.h"

fail() {
    printf '%s\n' "$*" >&2
    exit 1
}

sha1=$(awk -F= '/^PAPERMARIO_US_SHA1=/ { print $2 }' "$assets")
[ -n "$sha1" ] || fail "ASSETS.lock missing SHA-1"
grep -Fq "$sha1" "$header" || fail "assets.h SHA-1 must match ASSETS.lock"
grep -Fq "$sha1" "$project_root/docs/M7.md" || fail "docs/M7.md missing SHA-1"

grep -q '\*\.z64' "$project_root/.gitignore" || fail ".gitignore must ignore z64"
grep -q '\*\.o2r' "$project_root/.gitignore" || fail ".gitignore must ignore o2r"

if git -C "$project_root" ls-files '*.z64' '*.o2r' '*.n64' | grep .; then
    fail "ROMs or o2r archives must not be tracked"
fi

if grep -nE 'pm64\.o2r|paperboat\.o2r' "$project_root/.github/workflows/3ds-build.yml"; then
    fail "CI must not upload o2r archives"
fi

if grep -nE 'Torch-LH|torch otr' "$project_root/source/"*.c "$project_root/Makefile"; then
    fail "ARM11 sources must not invoke Torch"
fi
if grep -n '#include[[:space:]]*<3ds.h>' "$project_root/source/assets.c"; then
    fail "assets.c must not include 3ds.h"
fi

mkdir -p "$build_directory"
sh "$project_root/tools/prepare_assets.sh" --dry-run

fake="$build_directory/fake.z64"
printf 'not-a-rom' >"$fake"
if sh "$project_root/tools/prepare_assets.sh" "$fake"; then
    fail "prepare_assets.sh must reject a ROM with the wrong SHA-1"
fi

if sh "$project_root/tools/prepare_assets.sh"; then
    fail "prepare_assets.sh must require a ROM path"
fi

"$host_cc" \
    -std=c11 -O2 -Wall -Wextra -Werror \
    -I"$project_root/include" \
    "$project_root/source/assets.c" \
    "$project_root/tests/test_m7.c" \
    -o "$test_binary"

"$test_binary"
echo "M7 legal asset pipeline contract passed"
