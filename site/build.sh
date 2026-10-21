#!/bin/bash
# Assembles the FlowCE web page in _site/: the page (site/), the README's media (docs/) and the
# release bundles for the web installer: FlowCE.b84, and the Python build if given.
#   site/build.sh <FlowCE.b84> [version] [<FlowCE-python.b84> <its version>]
#   e.g. site/build.sh builds/FlowCE.b84 v2.0 builds/FlowCE-python.b84 v1.9.1
# The GitHub Pages workflow (.github/workflows/pages.yml) runs it with the latest release and the
# newest release that has a FlowCE-python.b84. Without one, the page's Python choice is off.
set -euo pipefail
cd "$(dirname "$0")/.."
B84=${1:?usage: site/build.sh <FlowCE.b84> [version] [<FlowCE-python.b84> <its version>]}
VER=${2:-}
PY_B84=${3:-}
PY_VER=${4:-}
OUT=_site
rm -rf "$OUT"
mkdir -p "$OUT/media/screenshots" "$OUT/media/install" "$OUT/files"
cp site/index.html site/style.css site/installer.js site/dusb.js "$OUT/"
cp -R site/artifice-v3 "$OUT/"
python3 site/fetch_inequalz.py "$OUT/artifice-v3" ${FLOWCE_INEQUALZ_FILE:+"$FLOWCE_INEQUALZ_FILE"}
cp site/icon.svg "$OUT/media/icon.svg"
cp docs/flowce_light.gif docs/flowce_dark.gif docs/flowce_themes.gif docs/flowce_apps.gif docs/flowce_graph.gif docs/themes_nine.png "$OUT/media/"
cp docs/screenshots/*.png "$OUT/media/screenshots/"
cp docs/install/*.gif "$OUT/media/install/"
cp "$B84" "$OUT/files/FlowCE.b84"
printf '%s\n' "$VER" > "$OUT/files/version.txt"
if [ -n "$PY_B84" ]; then
  cp "$PY_B84" "$OUT/files/FlowCE-python.b84"
  printf '%s\n' "${PY_VER:-v1.9}" > "$OUT/files/version-python.txt"
fi
echo "built $OUT ($(du -sh "$OUT" | cut -f1)), FlowCE ${VER:-(no version)}${PY_B84:+, Python ${PY_VER:-v1.9}}"
