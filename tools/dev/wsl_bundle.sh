#!/bin/bash
# WSL side of tools/dev/packs_build.sh: bundles a build's AppIns, arTIfiCE's launcher and INST into
# one .b84, as the releases are (convbin from the CE toolchain v15); <out>.b83: the TI-83 Premium CE's
# bundle (its own target device in METADATA, its own checksum).
# usage (WSL): wsl_bundle.sh <the build's out dir> <artific2.8xp> <INST.8xp> <out.b84 or .b83>
set -euo pipefail
DIR="$1"; ART="$2"; INST="$3"; B84="$4"
CEDEV15="${CEDEV15:-/home/bell/cedev15/x/CEdev}"
args=()
for f in $(ls "$DIR"/AppIns*.8xv | sort); do args+=(-i "$f"); done
[ "${#args[@]}" -ge 4 ] || { echo "[bundle] no AppIns in $DIR"; exit 1; }
rm -f "$B84"
KIND=${B84##*.}; [ "$KIND" = b83 ] || KIND=b84
"$CEDEV15/bin/convbin" -j 8x -k "$KIND" "${args[@]}" -i "$ART" -i "$INST" -o "$B84" > /dev/null
echo "[bundle] $(basename "$B84"): $(( ${#args[@]} / 2 )) AppIns, $(stat -c %s "$B84") B"
