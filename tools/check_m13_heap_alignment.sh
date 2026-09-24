#!/bin/sh
set -eu

if [ "$#" -ne 1 ]; then
    echo "usage: $0 ELF" >&2
    exit 2
fi

elf=$1
nm=${NM:-arm-none-eabi-nm}

check_heap() {
    symbol=$1
    expected_size=$2
    record=$($nm -S --defined-only "$elf" | awk -v wanted="$symbol" \
        '$4 == wanted { print $1 " " $2 }')
    if [ -z "$record" ]; then
        echo "missing M13 heap storage symbol: $symbol" >&2
        return 1
    fi

    set -- $record
    address=$((0x$1))
    size=$((0x$2))
    if [ $((address % 16)) -ne 0 ]; then
        printf '%s is not 16-byte aligned (address=0x%s)\n' \
            "$symbol" "$1" >&2
        return 1
    fi
    if [ "$size" -ne "$expected_size" ]; then
        printf '%s has unexpected size 0x%s (expected 0x%x)\n' \
            "$symbol" "$2" "$expected_size" >&2
        return 1
    fi
}

check_heap heap_generalHead 344064
check_heap heap_spriteHead 1048576
check_heap heap_collisionHead 262144
check_heap heap_battleHead 153600

echo "M13 upstream heap storage: aligned"
