#!/bin/bash
# Build KhiCAS with CEdev v14.2 inside WSL from the CURRENT working tree:
# uncommitted and untracked (non-ignored) files of this repo and of src/giac are included.
#
# How: each repo's working tree is snapshotted into a commit object (temporary index,
# LF-normalized by git, real index/worktree untouched) stored at refs/devbuild/snapshot.
# WSL fetches exactly that snapshot and builds it (see wsl_build.sh).
#
# Usage (Windows Git Bash):  tools/dev/build.sh [en|en-units|fr|l2]
set -euo pipefail
REPO="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
VARIANT="${1:-en}"

snapshot() {
  local dir="$1" idx tree commit
  idx="$(mktemp -u)"   # must not exist yet: git rejects an empty index file
  tree=$(cd "$dir" && GIT_INDEX_FILE="$idx" git read-tree HEAD && GIT_INDEX_FILE="$idx" git add -A . && GIT_INDEX_FILE="$idx" git write-tree)
  rm -f "$idx"
  commit=$(cd "$dir" && git commit-tree "$tree" -p HEAD -m "devbuild snapshot")
  git -C "$dir" update-ref refs/devbuild/snapshot "$commit"
  echo "$commit"
}

giac=$(snapshot "$REPO/src/giac")
super=$(snapshot "$REPO")
echo "[build] snapshot repo=${super:0:10} giac=${giac:0:10} variant=$VARIANT"

WSLREPO="/mnt/$(echo "${REPO:1:1}" | tr 'A-Z' 'a-z')${REPO:2}"
MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu-22.04 -- bash "$WSLREPO/tools/dev/wsl_build.sh" "$WSLREPO" "$VARIANT" "$super" "$giac" 2>&1 | tr -d '\0'
exit "${PIPESTATUS[0]}"
