#!/bin/sh
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
lock="$project_root/toolchain/TOOLCHAINS.lock"
workflow="$project_root/.github/workflows/3ds-build.yml"

fail() {
    printf '%s\n' "$*" >&2
    exit 1
}

[ -f "$lock" ] || fail "missing toolchain lock: $lock"
[ -f "$workflow" ] || fail "missing workflow: $workflow"

value() {
    awk -F= -v key="$1" '$1 == key { print $2; found=1 } END { exit found ? 0 : 1 }' "$lock"
}

digest=$(value docker_digest)
image=$(value docker_image)
makerom=$(value makerom_commit)
packages=$(value host_packages)
arch=$(value cpu_arch)

[ -n "$digest" ] && [ -n "$image" ] && [ -n "$makerom" ] || fail "lock is incomplete"
printf '%s' "$digest" | grep -Eq '^sha256:[0-9a-f]{64}$' || fail "docker_digest is not a sha256"
printf '%s' "$makerom" | grep -Eq '^[0-9a-f]{40}$' || fail "makerom_commit is not a 40-char SHA"

grep -Fq "$image@$digest" "$workflow" || fail "workflow does not pin $image@$digest"
grep -Fq "$makerom" "$workflow" || fail "workflow does not pin makerom $makerom"
grep -Fq "host_packages=$packages" "$lock" || fail "host_packages missing"
grep -Fq "cpu_arch=$arch" "$lock" || fail "cpu_arch missing"

printf 'M1 toolchain lock: %s@%s makerom=%s\n' "$image" "$digest" "$makerom"
