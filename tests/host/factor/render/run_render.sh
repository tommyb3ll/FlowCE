#!/bin/bash
# Renders every stop of the steps screen (src/focus_steps.cc, drawn off screen by the host binary's -V)
# for each problem of problems.txt, as PNG when ffmpeg is on the PATH (else only the .ppm).
# Usage: bash tests/host/factor/render/run_render.sh <binary> <outdir>
# Output: <outdir>/NN/0_<stop>.ppm|png, NN the problem's number in problems.txt (comments skipped).
# Fails (exit 1) if a problem reports "no steps" or produces no image. The images are not committed.
here=$(cd "$(dirname "$0")" && pwd)
bin=$1; out=$2
[ -n "$bin" ] && [ -n "$out" ] || { sed -n '2,6p' "$0"; exit 2; }
[ -x "$bin" ] || { echo "no such binary: $bin"; exit 2; }
mkdir -p "$out" || exit 2
export ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0}
have_ffmpeg=0; command -v ffmpeg >/dev/null 2>&1 && have_ffmpeg=1
n=0; fail=0; images=0
while IFS= read -r p || [ -n "$p" ]; do
  p=${p%$'\r'}
  case $p in ''|'#'*) continue ;; esac
  n=$((n+1)); d=$(printf '%s/%02d' "$out" "$n")
  rm -rf "$d"; mkdir -p "$d"
  log=$(timeout 60 "$bin" -V "$d" -e "$p" 2>&1)
  k=$(ls "$d"/*.ppm 2>/dev/null | wc -l)
  case $log in *"no steps"*) echo "FAIL  $p  (no steps)"; fail=$((fail+1)); continue ;; esac
  if [ "$k" = 0 ]; then echo "FAIL  $p  (no image)"; fail=$((fail+1)); continue; fi
  if [ $have_ffmpeg = 1 ]; then
    for f in "$d"/*.ppm; do ffmpeg -loglevel error -y -i "$f" "${f%.ppm}.png" || { echo "FAIL  $p  (ffmpeg on $f)"; fail=$((fail+1)); }; done
  fi
  images=$((images+k))
  echo "PASS  $p  ($k stops)  $d"
done < "$here/problems.txt"
[ $n -gt 0 ] || { echo "FAIL  no problems"; exit 1; }
echo "render: $n problems, $images images, $fail failed"
[ $fail = 0 ]
