#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
build_directory=${1:-"$project_root/build/m1-tests"}
mkdir -p "$build_directory"

sh "$project_root/tools/check_toolchain_lock.sh"

version=$(awk '/^#define PB3DS_VERSION / { gsub(/"/, "", $3); print $3 }' \
    "$project_root/include/pb3ds/version.h")
stage=$(awk '/^#define PB3DS_ROADMAP_STAGE / {
    for (i = 3; i <= NF; i++) printf "%s%s", $i, (i < NF ? " " : "")
    print ""
}' "$project_root/include/pb3ds/version.h" | tr -d '"')

[ "$version" = "0.1.0-m1" ] || {
    echo "expected PB3DS_VERSION 0.1.0-m1, got $version" >&2
    exit 1
}
printf '%s' "$stage" | grep -q "M1" || {
    echo "expected M1 roadmap stage, got $stage" >&2
    exit 1
}

PB3DS_VERSION="$version" \
    PB3DS_BUILD_SHA="testdeadbeef" \
    PB3DS_BUILD_UTC="2026-09-26T00:00:00Z" \
    PB3DS_DOCKER_IMAGE="devkitpro/devkitarm" \
    PB3DS_DOCKER_DIGEST="sha256:test" \
    PB3DS_MAKEROM_COMMIT="e8f5f529c54ff9b22a2491a480ffa69206bf7b19" \
    sh "$project_root/tools/write_build_info.sh" "$build_directory/build-info.txt"

grep -q 'version=0.1.0-m1' "$build_directory/build-info.txt"
grep -q 'git_sha=testdeadbeef' "$build_directory/build-info.txt"
grep -q 'build_utc=2026-09-26T00:00:00Z' "$build_directory/build-info.txt"

echo "M1 metadata contract: lock, version, and build-info checks passed"
