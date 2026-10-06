#!/bin/bash
# Builds the host leak-check executable (giac of KhiCAS for x86-64 Linux, AddressSanitizer +
# LeakSanitizer). Usage (from Windows Git Bash; wsl.exe re-parses command lines, so no quotes):
#   MSYS_NO_PATHCONV=1 wsl.exe -d Ubuntu-22.04 -- bash <repo>/tools/hostgiac/build.sh <variant> [giac ref] [patches...]
# <variant> names the build (tree-<variant>, bin/hostgiac-<variant>). Sources: src/giac at the
# given commit (default HEAD, read with git archive), then the given patches (files of this
# folder, applied in src/giac; default: fix.patch for the variant "fixed", none otherwise), then
# host.patch. The variant "worktree" takes src/giac as it is in the working tree (uncommitted
# edits included). ustl/, src/ustl, src/tommath and the few UI headers giac includes always come
# from the working tree.
# Before/after the leak fixes: build.sh orig 5bf21e9; build.sh fixed 5bf21e9 (= 3a54bae); build.sh head
# Everything is built under $HG_BUILD (default ~/hostgiac): tree-<v>/ (patched sources, LF line
# ends), obj-<v>/, bin/hostgiac-<v>. Nothing is written in the repo (git archive is read-only).
set -e
HG=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HG/../.." && pwd)
B=${HG_BUILD:-$HOME/hostgiac}
V=${1:-head}; REF=${2:-HEAD}
case "$V" in *[!a-zA-Z0-9_-]*|"") echo "usage: build.sh <variant> [giac ref] [patches...]"; exit 2 ;; esac
if [ $# -ge 3 ]; then shift 2; PATCHES="$*"; else PATCHES=$([ "$V" = fixed ] && echo fix.patch || true); fi
S=$B/stage-$V; T=$B/tree-$V
rm -rf "$S"; mkdir -p "$S/giac" "$S/ustl" "$S/ustlsrc" "$S/src" "$T"
if [ "$V" = worktree ]; then
  cp "$REPO"/src/giac/*.cc "$REPO"/src/giac/*.h "$S/giac/"
  echo "working tree" > "$B/giac-ref-$V.txt"
else
  git -c safe.directory='*' -C "$REPO/src/giac" archive "$REF" | tar -x -C "$S/giac"
  git -c safe.directory='*' -C "$REPO/src/giac" rev-parse --short "$REF" > "$B/giac-ref-$V.txt"
fi
cp "$REPO"/ustl/* "$S/ustl/"
cp "$REPO"/src/ustl/*.cc "$S/ustlsrc/"
for h in k_csdk.h k_defs.h dbg.h graphic.h console.h calc.h menuGUI.h main.h file.h textGUI.h ui_gfx.h ui_font.h ui_fontdata.h focus.h; do cp "$REPO/src/$h" "$S/src/"; done
# the working tree mixes CRLF and LF files: everything (sources and patches) is used with LF
find "$S" -type f \( -name '*.cc' -o -name '*.h' -o -name '*.c' \) -exec sed -i 's/\r$//' {} +
find "$S/ustl" -type f -exec sed -i 's/\r$//' {} +
for p in $PATCHES; do
  sed 's/\r$//' "$HG/$p" | (cd "$S/giac" && patch -p1 --quiet --no-backup-if-mismatch --forward) || { echo "$p does not apply to $REF"; exit 1; }
done
sed 's/\r$//' "$HG/host.patch" | (cd "$S" && patch -p1 --quiet --no-backup-if-mismatch --forward) || { echo "host.patch does not apply"; exit 1; }
# copy into the tree only what changed (make rebuilds what depends on it)
(cd "$S" && find . -type f) | while read -r f; do
  if ! cmp -s "$S/$f" "$T/$f"; then mkdir -p "$(dirname "$T/$f")"; cp "$S/$f" "$T/$f"; fi
done
rm -rf "$S"
# a change of the Makefile (flags) rebuilds everything
stamp=$(md5sum "$HG/Makefile" | cut -d' ' -f1)
if [ "$(cat "$B/obj-$V/.stamp" 2>/dev/null)" != "$stamp" ]; then rm -rf "$B/obj-$V"; mkdir -p "$B/obj-$V"; echo "$stamp" > "$B/obj-$V/.stamp"; fi
make -s -f "$HG/Makefile" T="$T" OBJ="$B/obj-$V" EXE="$B/bin/hostgiac-$V" -j"${JOBS:-6}"
echo "built $B/bin/hostgiac-$V (giac $(cat "$B/giac-ref-$V.txt")${PATCHES:+ + $PATCHES})"
