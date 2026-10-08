#!/bin/bash
# WSL side of tools/dev/build.sh. Args: <windows-repo-path-in-wsl> <en|en-units|fr|l2> <repo-commit> <giac-commit>
# Build tree, toolchain and outputs live outside the repo, in $KB (default /home/bell/khicas-review-build).
set -uo pipefail
WIN="$1"; VARIANT="$2"; SUPER="$3"; GIAC="$4"
KB="${KB:-/home/bell/khicas-review-build}"
SRC="$KB/src"; OUT="$KB/out/$VARIANT"; LOG="$KB/logs"
export PATH="$KB/CEdev/bin:$PATH"
mkdir -p "$OUT" "$LOG"
G() { git -c safe.directory='*' "$@"; }

if [ ! -d "$SRC/.git" ]; then G clone -q "$WIN" "$SRC"; fi
G -C "$SRC" fetch -q "$WIN" +refs/devbuild/snapshot:refs/devbuild/snapshot || exit 1
G -C "$SRC" checkout -qf "$SUPER" || exit 1
if [ ! -e "$SRC/src/giac/.git" ]; then G -C "$SRC" -c protocol.file.allow=always submodule update -q --init; fi
G -C "$SRC/src/giac" fetch -q "$WIN/.git/modules/src/giac" +refs/devbuild/snapshot:refs/devbuild/snapshot || exit 1
G -C "$SRC/src/giac" checkout -qf "$GIAC" || exit 1

cd "$SRC"
case "$VARIANT" in
  en) ln -sf app.src1 app_tools/app.src; ARGS="APPLANG=en" ;;
  en-units) ln -sf app.src1 app_tools/app.src; ARGS="APPLANG=en UNITS=1" ;;
  fr) ln -sf app.src2 app_tools/app.src; ARGS="APPLANG=fr" ;;
  l2) ln -sf app.src2 app_tools/app.src; ARGS="APPLANG=fr VARIANT=l2" ;;
  *) echo "unknown variant $VARIANT"; exit 2 ;;
esac
# Variants share obj/ but use different defines: force a clean build when the variant changes.
if [ "$(cat obj/.variant 2>/dev/null)" != "$VARIANT" ]; then make clean >/dev/null 2>&1; mkdir -p obj; echo "$VARIANT" > obj/.variant; fi

t0=$(date +%s)
make -j"$(nproc)" $ARGS OUTPUT_MAP=YES > "$LOG/build_$VARIANT.log" 2>&1
rc=$?
t1=$(date +%s)
if [ $rc -ne 0 ] || [ ! -f bin/DEMO.bin ]; then
  echo "[build] FAILED (rc=$rc) after $((t1-t0))s - last errors:"; grep -E "error|Error" "$LOG/build_$VARIANT.log" | head -20; exit 1
fi
g++ -O2 reloc.cc -o "$KB/reloc_bin" && (cd bin && "$KB/reloc_bin" DEMO.bin >/dev/null) || { echo "[build] reloc failed"; exit 1; }
rm -f bin/AppIns*.8xv bin/AppIns*.bin
python3 app_tools/make_segments.py bin/_DEMO.bin bin >/dev/null || { echo "[build] make_segments failed"; exit 1; }
rm -f "$OUT"/AppIns*.8xv
cp bin/AppIns*.8xv bin/_DEMO.bin bin/DEMO.map "$OUT"/
echo "$SUPER $GIAC" > "$OUT/SNAPSHOT"
size=$(stat -c %s bin/_DEMO.bin)
echo "[build] OK in $((t1-t0))s: _DEMO.bin=$size B ($(( (size+65535)/65536 )) flash pages), $(ls "$OUT"/AppIns*.8xv | wc -l) AppIns, warnings=$(grep -c 'warning:' "$LOG/build_$VARIANT.log") -> $OUT"
# The app must fit in 43 flash pages: INST puts it right below 0x3AFFFD, so at most 0x2AFFFD B
# (B. Parisse: another AppIns is fine within the pages). A 44th AppIns (65,232 B each: the binary
# + a 3-byte trailer) installs byte for byte when it holds 1 KB or more (emulator, 2026-10-07:
# 1,024 B, 7,305 B, 13,027 B). A few bytes stop INST ("AppIns43: AppVar is missing or was
# transfered unordered", 30 B): it seems to share the previous AppIns's flash sector.
max=$((0x2AFFFD))
segs=$(( (size + 3 + 65231) / 65232 )); last=$(( size + 3 - (segs - 1) * 65232 ))
if [ "$size" -gt "$max" ]; then echo "[build] TOO BIG: $size B > $max B (43 flash pages): INST will fail"; exit 3; fi
if [ "$segs" -gt 43 ] && [ "$last" -lt 1024 ]; then echo "[build] the last AppIns would hold $last B (< 1 KB): INST stops on it; grow or shrink the app by 1 KB"; exit 3; fi
echo "[build] room left: $((max - size)) B"
