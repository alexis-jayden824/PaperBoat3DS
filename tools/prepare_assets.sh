#!/bin/sh
# Host-only legal asset wrapper. Never run on ARM11. Never commit outputs.
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
lock="$project_root/upstream/PAPERBOAT.lock"
assets="$project_root/upstream/ASSETS.lock"
out_dir="$project_root/.cache/assets"
dry_run=0
rom=""

fail() {
    printf '%s\n' "$*" >&2
    exit 1
}

value() {
    awk -F= -v key="$2" '$1 == key { print $2; found=1 } END { exit found ? 0 : 1 }' "$1"
}

rom_sha1() {
    path=$1
    if command -v sha1sum >/dev/null; then
        sha1sum "$path" | awk '{ print $1 }'
    elif command -v shasum >/dev/null; then
        shasum -a 1 "$path" | awk '{ print $1 }'
    else
        openssl dgst -sha1 "$path" | awk '{ print $NF }'
    fi
}

usage() {
    cat <<EOF
Usage: sh tools/prepare_assets.sh --dry-run
       sh tools/prepare_assets.sh /path/to/baserom.us.z64

Supply a legally obtained US Paper Mario dump. This script never downloads a
ROM. Torch extraction stays on the host; outputs go to .cache/assets/ (gitignored).
EOF
}

expected=$(value "$assets" PAPERMARIO_US_SHA1)
baserom_name=$(value "$assets" BASEROM_FILENAME)
pm64_o2r=$(value "$assets" PM64_O2R)
paperboat_o2r=$(value "$assets" PAPERBOAT_O2R)
[ -n "$expected" ] || fail "ASSETS.lock missing PAPERMARIO_US_SHA1"

while [ $# -gt 0 ]; do
    case "$1" in
        --dry-run)
            dry_run=1
            shift
            ;;
        -h | --help)
            usage
            exit 0
            ;;
        --)
            shift
            break
            ;;
        -*)
            fail "unknown option $1"
            ;;
        *)
            rom=$1
            shift
            ;;
    esac
done

sh "$project_root/tools/fetch_torch.sh"

if [ "$dry_run" -eq 1 ]; then
    printf 'M7 dry-run: expect %s SHA-1 %s\n' "$baserom_name" "$expected"
    printf 'outputs: %s and %s under %s\n' "$pm64_o2r" "$paperboat_o2r" "$out_dir"
    exit 0
fi

[ -n "$rom" ] || fail "missing ROM path (see --help)"
[ -f "$rom" ] || fail "ROM not found: $rom"

got=$(rom_sha1 "$rom")
[ "$got" = "$expected" ] || fail "ROM SHA-1 $got is not the PaperBoat US image $expected"

mkdir -p "$out_dir"
{
    echo "baserom_sha1=$got"
    echo "torch_commit=$(value "$lock" TORCH_COMMIT)"
    echo "pm64_o2r=$pm64_o2r"
    echo "paperboat_o2r=$paperboat_o2r"
} >"$out_dir/MANIFEST.txt"

torch_bin=${TORCH_BIN:-}
if [ -z "$torch_bin" ]; then
    for candidate in \
        "$project_root/.cache/upstream/Torch-LH/build-cmake/torch" \
        "$project_root/.cache/upstream/Torch-LH/build/torch"; do
        if [ -x "$candidate" ]; then
            torch_bin=$candidate
            break
        fi
    done
fi

if [ -n "$torch_bin" ] && [ -x "$torch_bin" ]; then
    printf 'running %s otr %s\n' "$torch_bin" "$rom"
    (cd "$out_dir" && "$torch_bin" otr "$rom")
    if [ -f "$out_dir/$pm64_o2r" ]; then
        echo "pm64_sha1=$(rom_sha1 "$out_dir/$pm64_o2r")" >>"$out_dir/MANIFEST.txt"
    fi
else
    echo "torch_binary=not-built" >>"$out_dir/MANIFEST.txt"
    printf 'ROM verified. Build Torch-LH at the pinned commit, then rerun with TORCH_BIN=\n'
fi

printf 'wrote %s\n' "$out_dir/MANIFEST.txt"
