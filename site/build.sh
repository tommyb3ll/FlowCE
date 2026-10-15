#!/bin/bash
# Assembles the FlowCE web page in _site/: the page (site/), the README's media (docs/) and the
# release bundles for the web installer: FlowCE.b84, and the steps build if given.
#   site/build.sh <FlowCE.b84> [version] [<FlowCE-steps.b84> <steps version>]
#   e.g. site/build.sh builds/FlowCE.b84 v1.9 builds/FlowCE-steps.b84 v2.0-beta1
# The GitHub Pages workflow (.github/workflows/pages.yml) runs it with the latest release and the
# newest release that has a FlowCE-steps.b84. Without one, the page's Steps choice is off.
set -euo pipefail
cd "$(dirname "$0")/.."
B84=${1:?usage: site/build.sh <FlowCE.b84> [version] [<FlowCE-steps.b84> <steps version>]}
VER=${2:-}
STEPS_B84=${3:-}
STEPS_VER=${4:-}
OUT=_site
rm -rf "$OUT"
mkdir -p "$OUT/media/screenshots" "$OUT/media/install" "$OUT/files"
cp site/index.html site/style.css site/installer.js site/dusb.js "$OUT/"
cp site/icon.svg "$OUT/media/icon.svg"
cp docs/flowce_light.gif docs/flowce_dark.gif "$OUT/media/"
cp docs/screenshots/*.png "$OUT/media/screenshots/"
cp docs/install/*.gif "$OUT/media/install/"
cp "$B84" "$OUT/files/FlowCE.b84"
printf '%s\n' "$VER" > "$OUT/files/version.txt"
if [ -n "$STEPS_B84" ]; then
  cp "$STEPS_B84" "$OUT/files/FlowCE-steps.b84"
  printf '%s\n' "${STEPS_VER:-beta}" > "$OUT/files/version-steps.txt"
fi
echo "built $OUT ($(du -sh "$OUT" | cut -f1)), FlowCE ${VER:-(no version)}${STEPS_B84:+, steps ${STEPS_VER:-beta}}"
