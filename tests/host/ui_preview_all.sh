#!/usr/bin/env bash
# Render every Focus UI preview scene side by side (2x) into one PNG.
#   wsl.exe -d Ubuntu-22.04 -- bash tests/host/ui_preview_all.sh out.png [scene ...]
set -eu
here=$(cd "$(dirname "$0")" && pwd)
out=${1:-/tmp/ui_all.png}
shift || true
scenes=${*:-hero typing sizes}
bash "$here/run_ui_preview.sh" /tmp/ui_first.png hero 1 >/dev/null
files=""
for s in $scenes; do
  /tmp/ui_preview_bin "/tmp/ui_$s.ppm" "$s"
  files="$files /tmp/ui_$s.ppm"
done
python3 - "$out" $files <<'EOF'
import sys
from PIL import Image
ims = [Image.open(p) for p in sys.argv[2:]]
ims = [i.resize((i.width * 2, i.height * 2), Image.NEAREST) for i in ims]
W = sum(i.width for i in ims) + 10 * (len(ims) - 1); H = max(i.height for i in ims)
out = Image.new('RGB', (W, H), (60, 60, 60)); x = 0
for i in ims:
    out.paste(i, (x, 0)); x += i.width + 10
out.save(sys.argv[1]); print('wrote', sys.argv[1])
EOF
