#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
# shellcheck disable=SC2034
build_directory=${1:-"$project_root/build/m2-tests"}
mkdir -p "$build_directory"
sh "$project_root/tools/check_upstream_lock.sh"
echo "M2 dependency audit contract passed"
