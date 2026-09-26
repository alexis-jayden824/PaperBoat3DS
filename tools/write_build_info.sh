#!/bin/sh
set -eu

output=${1:-build/build-info.txt}
mkdir -p "$(dirname "$output")"

cat > "$output" <<EOF
project=PaperBoat3DS Refolded
version=${PB3DS_VERSION:-unknown}
git_sha=${PB3DS_BUILD_SHA:-unknown}
build_utc=${PB3DS_BUILD_UTC:-unknown}
docker_image=${PB3DS_DOCKER_IMAGE:-unset}
docker_digest=${PB3DS_DOCKER_DIGEST:-unset}
makerom_commit=${PB3DS_MAKEROM_COMMIT:-unset}
host_cc=${HOST_CC:-cc}
devkitarm=${DEVKITARM:-unset}
EOF

if [ -n "${DEVKITARM:-}" ] && [ -x "$DEVKITARM/bin/arm-none-eabi-gcc" ]; then
    "$DEVKITARM/bin/arm-none-eabi-gcc" --version | awk 'NR==1 { print "gcc="$0 }' >> "$output"
fi

test -s "$output"
printf 'wrote %s\n' "$output"
