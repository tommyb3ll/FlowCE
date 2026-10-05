#!/bin/bash
# Windows Git Bash wrapper: install the last build into the emulator and save a console state.
#   tools/emu/install.sh [variant=en] [state=base]
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
W="/mnt/$(echo "${REPO:1:1}" | tr 'A-Z' 'a-z')${REPO:2}"
MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu-22.04 -- python3 "$W/tools/emu/install.py" "${1:-en}" "${2:-base}" "$(dirname "$W")/emu-shots" 2>&1 \
  | tr -d '\0' | grep -v 'screen size is bogus' | grep -v -E '^(write|erase) ' | grep -v '^--- calculator'
