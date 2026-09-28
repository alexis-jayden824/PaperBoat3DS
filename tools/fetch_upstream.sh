#!/bin/sh

set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
lock_file="$repo_root/upstream/PAPERBOAT.lock"
upstream_root=${1:-"$repo_root/.cache/upstream"}

if [ ! -f "$lock_file" ]; then
    echo "Missing upstream lock file: $lock_file" >&2
    exit 1
fi

# The lock file is maintained in this repository and contains only shell-safe
# KEY=value assignments. Sourcing it keeps local and CI fetches on one contract.
. "$lock_file"

fetch_locked_repository() {
    name=$1
    repository=$2
    commit=$3
    destination=$4

    mkdir -p "$destination"
    if [ ! -d "$destination/.git" ]; then
        git -C "$destination" init -q
        git -C "$destination" remote add origin "$repository"
    else
        git -C "$destination" remote set-url origin "$repository"
    fi

    current_commit=$(git -C "$destination" rev-parse HEAD 2>/dev/null || true)
    if [ "$current_commit" != "$commit" ]; then
        echo "Fetching $name at $commit"
        git -C "$destination" fetch --quiet --depth 1 origin "$commit"
        git -C "$destination" checkout --quiet --detach --force FETCH_HEAD
    fi

    resolved_commit=$(git -C "$destination" rev-parse HEAD)
    if [ "$resolved_commit" != "$commit" ]; then
        echo "$name resolved to $resolved_commit instead of $commit" >&2
        exit 1
    fi

    echo "$name: $resolved_commit"
}

paperboat_dir="$upstream_root/PaperBoat"
fetch_locked_repository "PaperBoat" "$PAPERBOAT_REPOSITORY" \
    "$PAPERBOAT_COMMIT" "$paperboat_dir"
fetch_locked_repository "libultraship" "$LIBULTRASHIP_REPOSITORY" \
    "$LIBULTRASHIP_COMMIT" "$paperboat_dir/external/libultraship"
fetch_locked_repository "Torch" "$TORCH_REPOSITORY" \
    "$TORCH_COMMIT" "$paperboat_dir/external/torch"

echo "Pinned upstream sources are ready in $upstream_root"
