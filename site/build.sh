#!/bin/bash
# Assembles the FlowCE web page in _site/: the page (site/), the README's media (docs/) and the
# release bundles for the web installer: FlowCE.b84, and KhiCAS + Flow's if given.
#   site/build.sh <FlowCE.b84> [version] [<KhiCAS-Flow.b84> <its version>]
#   e.g. site/build.sh builds/FlowCE.b84 v2.2.4 builds/KhiCAS-Flow.b84 v1.0
# The GitHub Pages workflow (.github/workflows/pages.yml) runs it with the latest release and the
# newest release that has a KhiCAS-Flow.b84. Without one, the page's KhiCAS + Flow choice is off.
set -euo pipefail
cd "$(dirname "$0")/.."
B84=${1:?usage: site/build.sh <FlowCE.b84> [version] [<KhiCAS-Flow.b84> <its version>]}
VER=${2:-}
KH_B84=${3:-}
KH_VER=${4:-}
OUT=_site
rm -rf "$OUT"
mkdir -p "$OUT/media/screenshots" "$OUT/media/install" "$OUT/files"
cp site/index.html site/style.css site/installer.js site/dusb.js site/chat.js "$OUT/"
cp -R site/artifice-v3 "$OUT/"
python3 site/fetch_inequalz.py "$OUT/artifice-v3" ${FLOWCE_INEQUALZ_FILE:+"$FLOWCE_INEQUALZ_FILE"}
cp site/icon.svg "$OUT/media/icon.svg"
cp docs/flowce_light.gif docs/flowce_dark.gif docs/flowce_themes.gif docs/flowce_apps.gif docs/flowce_graph.gif docs/khicasflow.gif "$OUT/media/"
cp docs/screenshots/*.png "$OUT/media/screenshots/"
cp docs/install/*.gif "$OUT/media/install/"
# Update the app installer even when the release predates the boot-code compatibility fix.
# All AppIns files and the launch program retain their original bytes.
python3 tools/dev/repack_installer.py "$B84" "$OUT/files/FlowCE.b84"
printf '%s\n' "$VER" > "$OUT/files/version.txt"
if [ -n "$KH_B84" ]; then
  python3 tools/dev/repack_installer.py "$KH_B84" "$OUT/files/KhiCAS-Flow.b84"
  printf '%s\n' "${KH_VER:-v1.0}" > "$OUT/files/version-khicas.txt"
fi
echo "built $OUT ($(du -sh "$OUT" | cut -f1)), FlowCE ${VER:-(no version)}${KH_B84:+, KhiCAS + Flow ${KH_VER:-v1.0}}"
