#!/bin/bash
# Windows Git Bash wrapper: run tools/emu/cemu.py inside WSL.
#   tools/emu/emu.sh <in: state-name|rom> <out: state-name|-> step [step ...]
# State names are files in $KB/emu/states/<name>.ce ; "rom" means boot the user's ROM.
# Screenshots land in <session folder>/emu-shots/ (outside the repo).
# Steps go through a file: wsl.exe re-parses its command line with a shell, which breaks
# on characters like ( ) in `type:` steps.
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
W="/mnt/$(echo "${REPO:1:1}" | tr 'A-Z' 'a-z')${REPO:2}"
ST=/home/bell/khicas-review-build/emu/states
in="$1"; out="$2"; shift 2
[ "$in" = rom ] && in=/mnt/c/Users/tjb43/Downloads/Projects/khicas/rom.rom || in="$ST/$in.ce"
[ "$out" = - ] || out="$ST/$out.ce"
SHOTS="$(dirname "$REPO")/emu-shots"; mkdir -p "$SHOTS"
stepfile=".steps_$$.txt"
printf '%s\n' "$@" > "$SHOTS/$stepfile"
MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu-22.04 -- python3 "$W/tools/emu/cemu.py" "$in" "$out" "$(dirname "$W")/emu-shots" "@$(dirname "$W")/emu-shots/$stepfile" 2>&1 \
  | tr -d '\0' | grep -v 'screen size is bogus'
rc=${PIPESTATUS[0]}
rm -f "$SHOTS/$stepfile"
exit "$rc"
