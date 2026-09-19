#!/bin/sh
set -eu

if [ "$#" -lt 1 ] || [ "$#" -gt 2 ]; then
    echo "usage: $0 <elf> [report]" >&2
    exit 2
fi

elf=$1
report=${2:-build/memory-budget.txt}
size_tool=${SIZE:-arm-none-eabi-size}
max_image=${PB3DS_MAX_IMAGE_BYTES:-16777216}
max_static_ram=${PB3DS_MAX_STATIC_RAM_BYTES:-8388608}

if [ ! -s "$elf" ]; then
    echo "memory budget: missing ELF: $elf" >&2
    exit 1
fi

set -- $($size_tool "$elf" | sed -n '2p')
text_bytes=$1
data_bytes=$2
bss_bytes=$3
image_bytes=$((text_bytes + data_bytes))
static_ram_bytes=$((data_bytes + bss_bytes))

mkdir -p "$(dirname "$report")"
{
    echo "PaperBoat3DS M6 static memory report"
    echo "elf=$elf"
    echo "text_bytes=$text_bytes"
    echo "data_bytes=$data_bytes"
    echo "bss_bytes=$bss_bytes"
    echo "image_bytes=$image_bytes"
    echo "static_ram_bytes=$static_ram_bytes"
    echo "max_image_bytes=$max_image"
    echo "max_static_ram_bytes=$max_static_ram"
} > "$report"

cat "$report"

if [ "$image_bytes" -gt "$max_image" ]; then
    echo "memory budget: loadable image exceeds $max_image bytes" >&2
    exit 1
fi
if [ "$static_ram_bytes" -gt "$max_static_ram" ]; then
    echo "memory budget: data+bss exceeds $max_static_ram bytes" >&2
    exit 1
fi
