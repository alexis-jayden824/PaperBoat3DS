#!/bin/sh
# list_paperboat_sources.sh — emit the PaperBoat 1.0.1 game C list using the
# same FILTER rules as CMakeLists.txt / upstream/PAPERBOAT.exclusions. This
# does not compile the units; M13 links them only when the owner enables
# the full game objects.
set -eu

project_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
root=${PB3DS_PAPERBOAT_ROOT:-"$project_root/.cache/upstream/PaperBoat"}
out=${1:-"$project_root/build/paperboat_game_sources.txt"}

if [ ! -d "$root/src" ]; then
    echo "PaperBoat tree missing; run make fetch" >&2
    exit 1
fi

mkdir -p "$(dirname "$out")"
# shellcheck disable=SC2016
find "$root/src" -name '*.c' ! -name '*.inc.c' | awk '
    /\/src\/port\// { next }
    /\/src\/os\// { next }
    /\/src\/boot\// { next }
    /\/src\/effects\/gfx\// { next }
    /unused_gfx\.c$/ { next }
    /\/[^/]*_(jp|fr|es|de|en|en_de|pal|ique)\.c$/ { next }
    /filemenu_selectlanguage\.c$/ { next }
    /(npc_composer|npc_hint_dryite_companion|npc_hint_dryite|npc_merlee|npc_moustafa|npc_shop_owner)\.c$/ { next }
    /kzn_19_anim[0-9]+\.c$/ { next }
    /hos_10\/narrator\.c$/ { next }
    /battle\/area\/sam2\/(dlist|vtx)\.c$/ { next }
    /pra_02\/entity\.c$/ { next }
    { print }
' | sort >"$out"

count=$(wc -l <"$out")
echo "listed $count PaperBoat game C files into $out"
test "$count" -gt 100
