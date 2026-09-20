#!/bin/sh
set -eu

if [ "$#" -lt 5 ]; then
    echo "usage: $0 PAPERBOAT_ROOT BUILD_DIR CC AR CFLAGS..." >&2
    exit 2
fi

paperboat_root=$1
build_dir=$2
compiler=$3
archiver=$4
shift 4

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
source_list="$project_root/upstream/M13_RUNTIME_SOURCES.txt"
generated_dir="$build_dir/generated"
object_dir="$build_dir/objects"
archive="$build_dir/libpaperboat-m13.a"

python3 "$project_root/tools/generate_m13_runtime.py" \
    "$paperboat_root" "$generated_dir"
mkdir -p "$object_dir"
rm -f "$archive"

objects=
while IFS= read -r source; do
    [ -n "$source" ] || continue
    object_name=$(printf '%s' "$source" | sed 's|/|__|g; s|\.|_|g')
    object="$object_dir/$object_name.o"
    echo "  M13 $source"
    "$compiler" "$@" -c "$paperboat_root/$source" -o "$object"
    objects="$objects $object"
done < "$source_list"

for generated in runtime_world_mac runtime_game_modes runtime_nusys_overrides; do
    object="$object_dir/$generated.o"
    echo "  M13 generated/$generated.c"
    "$compiler" "$@" -c "$generated_dir/$generated.c" -o "$object"
    objects="$objects $object"
done

# Deliberate word splitting: objects contains paths produced by this script and
# the workspace contract rejects whitespace in repository paths.
# shellcheck disable=SC2086
"$archiver" rcs "$archive" $objects
test -s "$archive"
