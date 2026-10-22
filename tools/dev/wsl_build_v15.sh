#!/bin/bash
# WSL side of tools/dev/build.sh for the CE toolchain v15 variants (en-v15, en-steps-v15, en-units-v15), called by
# wsl_build.sh. Args: <windows-repo-path-in-wsl> <variant> <repo-commit> <giac-commit>
# Toolchain: $CEDEV15 (default /home/bell/cedev15/x/CEdev); own source tree $KB/src15, so the v14.2
# objects are not mixed in. The app is linked twice with GNU ld (the toolchain's application linker
# script and crt0), the second time at its final flash address, then app_tools/pack_app.py adds
# the headers INST expects and make_segments.py cuts the AppIns, as for v14.2.
set -uo pipefail
WIN="$1"; VARIANT="$2"; SUPER="$3"; GIAC="$4"
KB="${KB:-/home/bell/khicas-review-build}"
CEDEV15="${CEDEV15:-/home/bell/cedev15/x/CEdev}"
SRC="$KB/src15"; [ "${VARIANT#*tilook}" != "$VARIANT" ] && SRC="$KB/src15ti" # (objects of its own)
[ "${VARIANT#*khicas}" != "$VARIANT" ] && SRC="$KB/src15kh"
OUT="$KB/out/$VARIANT"; LOG="$KB/logs"
if [ -n "${NIGHTLY:-}" ]; then # (variant-nightly: the nightly toolchain, its own objects and outputs)
  CEDEV15="${CEDEVN:-/home/bell/cedev-nightly-20261014/CEdev}"; SRC="${SRC}n"; OUT="$OUT-nightly"
fi
BU="$CEDEV15/binutils/bin"
export PATH="$CEDEV15/bin:$PATH"
mkdir -p "$OUT" "$LOG"
G() { git -c safe.directory='*' "$@"; }

if [ ! -d "$SRC/.git" ]; then G clone -q "$WIN" "$SRC"; fi
G -C "$SRC" fetch -q --no-recurse-submodules "$WIN" +refs/devbuild/snapshot:refs/devbuild/snapshot || exit 1
G -C "$SRC" checkout -qf "$SUPER" || exit 1
if [ ! -e "$SRC/src/giac/.git" ]; then G -C "$SRC" -c protocol.file.allow=always submodule update -q --init; fi
G -C "$SRC/src/giac" fetch -q "$WIN/.git/modules/src/giac" +refs/devbuild/snapshot:refs/devbuild/snapshot || exit 1
G -C "$SRC/src/giac" checkout -qf "$GIAC" || exit 1

cd "$SRC"
case "$VARIANT" in
  en-v15) ARGS="APPLANG=en" ;;
  en-steps-v15) ARGS="APPLANG=en STEPS=1" ;;
  en-units-v15) ARGS="APPLANG=en UNITS=1" ;;
  en-steps-tilook-v15) ARGS="APPLANG=en STEPS=1 TILOOK=1" ;;
  en-khicas-v15) ARGS="APPLANG=en KHICAS=1"; export APP_NAME=KhiFlow APP_INFO="KhiCAS + Flow - CAS - GNU GPL" ;; # (its own name in APPS)
  *) echo "unknown variant $VARIANT"; exit 2 ;;
esac
if [ "$(cat obj/.variant 2>/dev/null)" != "$VARIANT" ]; then rm -rf obj bin; mkdir -p obj; echo "$VARIANT" > obj/.variant; fi

LV="$VARIANT${NIGHTLY:+-nightly}" # (the log's name)
# the fasmg link's settings are replaced: the toolchain's app linker script (v15 forces it for an
# application, the nightly takes it only if none is set: app_tools/makefile's is fasmg's), the heap bounds
# of the custom allocator under the names that script gives them
LDX="--defsym=___heapbot=___heap_low --defsym=___heaptop=___heap_high"
t0=$(date +%s)
: > "$LOG/build_$LV.log"
# the catalog's English descriptions, packed from menufr.cc's D("...") (DeltaCE's tools/catpack.py;
# the header is rewritten only when they changed)
python3 tools/catpack.py src/menufr.cc src/catalog_text.h >> "$LOG/build_$LV.log" || { echo "[build] tools/catpack.py failed"; exit 1; }
PAD=0 # 0xFF bytes after the code (a last AppIns under 1 KB: below)
link() { # <load address>
  rm -f bin/DEMO.obj
  make -j"$(nproc)" $ARGS APPLICATION=YES LINKER_SCRIPT="$CEDEV15/meta/linker_script_app.ld" EXTRA_LDFLAGS="$LDX" OUTPUT_MAP=YES STRIP_CMD=: LOAD_ADDR="$1" bin/DEMO.obj >> "$LOG/build_$LV.log" 2>&1 \
    || { echo "[build] FAILED - last errors:"; grep -E "error|undefined" "$LOG/build_$LV.log" | head -20; exit 1; }
  "$BU/z80-none-elf-objcopy" -O binary bin/DEMO.obj bin/body.bin
  [ "$PAD" = 0 ] || head -c "$PAD" /dev/zero | LC_ALL=C tr '\0' '\377' >> bin/body.bin
}
pack_all() { # the link at the app's final address (from the size linked at 0), the image, its AppIns
  LA=$(python3 app_tools/pack_app.py size bin/body.bin)
  n1=$(stat -c %s bin/body.bin)
  link "$LA"
  n2=$(stat -c %s bin/body.bin)
  [ "$n1" = "$n2" ] || { echo "[build] the app's size changed with its address: $n1 -> $n2"; exit 1; }
  ENTRY=0x$("$BU/z80-none-elf-nm" bin/DEMO.obj | awk '$3=="__start" {print $1}')
  python3 app_tools/pack_app.py pack bin/body.bin "$ENTRY" "$LA" bin/_DEMO.bin > /dev/null || { echo "[build] pack_app failed"; exit 1; }
  rm -f bin/AppIns*.8xv bin/AppIns*.bin
  python3 app_tools/make_segments.py bin/_DEMO.bin bin >/dev/null || { echo "[build] make_segments failed"; exit 1; }
  size=$(stat -c %s bin/_DEMO.bin)
  segs=$(( (size + 3 + 65231) / 65232 )); last=$(( size + 3 - (segs - 1) * 65232 ))
}
link 0
# v15's frame bias miscompiles functions with variable-length arrays: refuse such a build
python3 tools/dev/v15_biascheck.py obj/lto.s || { echo "[build] REFUSED (tools/dev/v15_biascheck.py)"; exit 1; }
# and drops the carry clear before some subtractions (a stale borrow): refuse those too
python3 tools/dev/v15_carrycheck.py obj/lto.s || { echo "[build] REFUSED (tools/dev/v15_carrycheck.py)"; exit 1; }
pack_all
if [ "$last" -lt 1024 ]; then
  # INST stops on a last AppIns under 1 KB, whatever their number (DeltaCE: a 42-piece build with 86
  # bytes in its last failed, 2026-10-17): 0xFF bytes after the code fill it to 1 KB, linked again
  PAD=$((1024 - last + 64))
  echo "[build] the last AppIns would hold $last B (< 1 KB): $PAD B of padding after the code"
  link 0; pack_all
fi
t1=$(date +%s)
rm -f "$OUT"/AppIns*.8xv
# the emulator tools read a map in the fasmg format: name = address. v15 aligns short on 2 bytes
# (v14.2: 1), so the console's struct line (console.h) takes 14 bytes, not 13: told to the tools
# (tools/emu/cemu.py line_record) as a pseudo-symbol
"$BU/z80-none-elf-nm" bin/DEMO.obj | awk 'NF==3 {printf "%s = %06X\n", $3, strtonum("0x" $1)}' > bin/DEMO.map
echo "__flowce_line_size = 00000E" >> bin/DEMO.map
cp bin/AppIns*.8xv bin/_DEMO.bin bin/DEMO.map "$OUT"/
echo "$SUPER $GIAC" > "$OUT/SNAPSHOT"
echo "[build] OK in $((t1-t0))s: _DEMO.bin=$size B ($(( (size+65535)/65536 )) flash pages), $(ls "$OUT"/AppIns*.8xv | wc -l) AppIns, warnings=$(grep -c 'warning:' "$LOG/build_$LV.log") -> $OUT"
max=$((0x2AFFFD))
if [ "$size" -gt "$max" ]; then echo "[build] TOO BIG: $size B > $max B (43 flash pages): INST will fail"; exit 3; fi
if [ "$last" -lt 1024 ]; then echo "[build] the last AppIns holds $last B (< 1 KB) even padded: INST stops on it"; exit 3; fi
echo "[build] room left: $((max - size)) B"
