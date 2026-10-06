#!/bin/bash
# Build a commit for the user's real calculator in a separate tree (does not disturb the main one),
# then bundle it: Downloads/khicas84-<NAME>/KhiCAS-<NAME>.b84 (+ files/).
# usage (WSL): user_bundle.sh <super-commit> <giac-commit> <NAME>
set -e
KB2=/home/bell/khicas-build-user
MAIN=/home/bell/khicas-review-build
rm -rf "$KB2"
mkdir -p "$KB2/out" "$KB2/logs"
cp -a "$MAIN/src" "$KB2/src"
ln -sfn "$MAIN/CEdev" "$KB2/CEdev"
echo "copied tree"
export KB="$KB2"
bash /mnt/c/Users/tjb43/Downloads/Projects/khicas/KhiCAS-v2/tools/dev/wsl_build.sh /mnt/c/Users/tjb43/Downloads/Projects/khicas/KhiCAS-v2 en "$1" "$2"
set -- "$3"
NAME=${1:-FOCUS1}
OUT=/mnt/c/Users/tjb43/Downloads/khicas84-$NAME
NEW2=/mnt/c/Users/tjb43/Downloads/khicas84-NEW2/files
rm -rf "$OUT"
mkdir -p "$OUT/files"
cp "$KB2"/out/en/AppIns*.8xv "$OUT/files/"
cp "$NEW2/artific2.8xp" "$OUT/files/"
cp "$KB2/src/app_tools/INST.8xp" "$OUT/files/"
cmp "$OUT/files/INST.8xp" "$NEW2/INST.8xp" && echo "INST identical to NEW2's"
args=""
for f in $(ls "$OUT"/files/AppIns*.8xv | sort); do args="$args -i $f"; done
"$KB2/CEdev/bin/convbin" -j 8x -k b84 $args -i "$OUT/files/artific2.8xp" -i "$OUT/files/INST.8xp" -o "$OUT/KhiCAS-$NAME.b84" >/dev/null
ls -la "$OUT/KhiCAS-$NAME.b84" "$NEW2/../KhiCAS-NEW2.b84"
ls "$OUT/files" | wc -l
