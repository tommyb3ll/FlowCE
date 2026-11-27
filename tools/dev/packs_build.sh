#!/bin/bash
# The step-by-step build's pack combinations (3.0): builds each (series, vectors, statistics,
# differential equations: makefile SERIES VECTORS STATS DIFFEQ) from the current working tree and
# writes their sizes to builds/packs-sizes.txt: code, _DEMO.bin bytes, the link's inline threshold,
# fits or too-big. A combination fits when its app is at most 2,818,045 B (43 flash pages: INST's
# limit). The link inlines at -10 (makefile LTOINL); a combination too big at -10 is built again at
# -20, then -30 (less inlining: smaller, about 1 to 2% slower, the same answers; the size is not
# monotonic in it: the series pack alone is smallest at -20, 2026-11-27). Still too big: -20, then -30,
# with BARRETT=1 (makefile: tommath's Barrett reduction only, 7.6 KB less; prime tests of very large
# numbers slower), so the default and the others keep the full speed.
# With OUT=<dir>, each combination that fits is bundled there as the web page's customizer
# installs it (site/packs.py): FlowCE.b84 for statistics and differential equations (2.4's), else
# FlowCE-<packs>.b84 (FlowCE-core.b84: none), with arTIfiCE's launcher and app_tools/INST.8xp.
# usage (Windows Git Bash, from the repo): [OUT=<dir>] tools/dev/packs_build.sh [codes...]   (default: all 16)
set -u
cd "$(dirname "$0")/../.."
SIZES=../builds/packs-sizes.txt
MAX=2818045
codes=${*:-0000 0001 0010 0011 0100 0101 0110 0111 1000 1001 1010 1011 1100 1101 1110 1111}
OUT=${OUT:-}
ARTIFICE=${ARTIFICE:-../builds/khicas84-NEW2/files/artific2.8xp} # (arTIfiCE v2's launcher, program A, as in every release)
wslpath() { local p; p=$(cd "$1" && pwd); echo "/mnt/$(echo "${p:1:1}" | tr 'A-Z' 'a-z')${p:2}"; }
name() { # 0011 -> FlowCE.b84 (the default), 0110 -> FlowCE-vectors-stats.b84, 0000 -> FlowCE-core.b84
  [ "$1" = 0011 ] && { echo FlowCE.b84; return; }
  local n=""
  [ "${1:0:1}" = 1 ] && n="$n-series"
  [ "${1:1:1}" = 1 ] && n="$n-vectors"
  [ "${1:2:1}" = 1 ] && n="$n-stats"
  [ "${1:3:1}" = 1 ] && n="$n-diffeq"
  echo "FlowCE${n:--core}.b84"
}
if [ -n "$OUT" ]; then
  mkdir -p "$OUT"
  [ -f "$ARTIFICE" ] || { echo "no arTIfiCE launcher at $ARTIFICE (ARTIFICE=...)"; exit 1; }
fi
: > "$SIZES"
for c in $codes; do
  size="" inl=""
  for i in 10 20 30 20b 30b; do
    v="en-steps-p$c-v15-nightly"
    [ "$i" = 10 ] || v="en-steps-p$c-i$i-v15-nightly"
    line=$(bash tools/dev/build.sh "$v" 2>&1 | grep -E '\[build\] (OK|FAILED)' | tail -1)
    size=$(printf '%s' "$line" | grep -o '_DEMO.bin=[0-9]*' | cut -d= -f2)
    inl=$i
    [ -n "$size" ] && [ "$size" -le "$MAX" ] && break
    [ -n "$size" ] && [ "$size" -gt $((MAX + 40000)) ] && break # (far too big: less inlining saves about 20 KB)
  done
  if [ -z "$size" ]; then echo "$c FAILED" | tee -a "$SIZES"; continue; fi
  fits=$([ "$size" -le "$MAX" ] && echo fits || echo too-big)
  echo "$c $size -$inl $fits" | tee -a "$SIZES"
  if [ -n "$OUT" ] && [ "$fits" = fits ]; then
    f=$(name "$c")
    MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu-22.04 -- bash "$(wslpath tools/dev)/wsl_bundle.sh" \
      "/home/bell/khicas-review-build/out/$v" "$(wslpath "$(dirname "$ARTIFICE")")/$(basename "$ARTIFICE")" \
      "$(wslpath app_tools)/INST.8xp" "$(wslpath "$OUT")/$f" 2>&1 | tr -d '\0'
  fi
done
