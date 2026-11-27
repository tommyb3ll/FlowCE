#!/bin/bash
# The release folders, outside the repo, from builds already made (tools/dev/packs_build.sh with OUT=<packs
# dir>, and tools/dev/build.sh en-khicas-v15-nightly): <flowce dir>/release (every FlowCE*.b84 of the packs
# dir; FlowCE.b83, the TI-83 Premium CE's bundle of the default build, en-steps-p0011; notes.md) and <khicas
# dir>/release (KhiCAS-Flow.b84 and .b83 bundled from the KhiCAS + Flow build, notes.md). The builds' out
# dirs must still hold those builds. Nothing is published.
# usage (Windows Git Bash, from the repo): tools/dev/release_dirs.sh <packs dir> <flowce dir> <flowce notes.md> <khicas dir> <khicas notes.md>
set -euo pipefail
cd "$(dirname "$0")/../.."
PACKS=$1; FD=$2; FN=$3; KD=$4; KN=$5
ARTIFICE=${ARTIFICE:-../builds/khicas84-NEW2/files/artific2.8xp}
wslpath() { local p; p=$(cd "$1" && pwd); echo "/mnt/$(echo "${p:1:1}" | tr 'A-Z' 'a-z')${p:2}"; }
mkdir -p "$FD/release" "$KD/release"
cp "$PACKS"/FlowCE*.b84 "$FD/release/"
cp "$FN" "$FD/release/notes.md"
A="$(wslpath "$(dirname "$ARTIFICE")")/$(basename "$ARTIFICE")"
bundle() { # <variant> <out file in WSL form>
  MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu-22.04 -- bash "$(wslpath tools/dev)/wsl_bundle.sh" \
    "/home/bell/khicas-review-build/out/$1" "$A" "$(wslpath app_tools)/INST.8xp" "$2" 2>&1 | tr -d '\0'
}
bundle en-steps-p0011-v15-nightly "$(wslpath "$FD/release")/FlowCE.b83"
# (the .b83 from the same build as FlowCE.b84: their first piece alike)
cmp <(unzip -p "$FD/release/FlowCE.b84" AppIns00.8xv) <(unzip -p "$FD/release/FlowCE.b83" AppIns00.8xv) \
  || { echo "FlowCE.b83 is not from FlowCE.b84's build"; exit 1; }
bundle en-khicas-v15-nightly "$(wslpath "$KD/release")/KhiCAS-Flow.b84"
bundle en-khicas-v15-nightly "$(wslpath "$KD/release")/KhiCAS-Flow.b83"
cp "$KN" "$KD/release/notes.md"
ls -la "$FD/release" "$KD/release"
