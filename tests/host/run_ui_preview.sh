#!/usr/bin/env bash
# Render a Focus UI scene with the calculator's drawing code on the PC.
#   wsl.exe -d Ubuntu-22.04 -- bash tests/host/run_ui_preview.sh out.png [hero|typing|sizes] [scale]
set -eu
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
out=${1:-/tmp/ui_preview.png}
scene=${2:-hero}
scale=${3:-2}
bin=/tmp/ui_preview_bin
for f in ui_gfx ui_font ui_fontdata; do
  gcc -c -O1 -g -Wall -I"$root/src" -o "/tmp/$f.o" "$root/src/$f.c"
done
g++ -std=c++14 -O1 -g -Wall -I"$root/src" -o "$bin" \
  "$here/ui_preview.cc" "$root/src/mathinput.cc" "$root/src/ui_math.cc" /tmp/ui_gfx.o /tmp/ui_font.o /tmp/ui_fontdata.o
"$bin" /tmp/ui_preview.ppm "$scene"
python3 -c "
from PIL import Image
im=Image.open('/tmp/ui_preview.ppm'); s=$scale
im.resize((im.width*s, im.height*s), Image.NEAREST).save('$out')
print('wrote $out')"
