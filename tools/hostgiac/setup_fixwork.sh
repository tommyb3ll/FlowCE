#!/bin/bash
# Development helper: $HG_BUILD/<dir>/a = src/giac files as in the working tree (line ends as
# there), b = the same to be edited; mkfixpatch.sh turns the difference into a patch (paths
# relative to src/giac). Usage: setup_fixwork.sh [dir] [files...] (default: fixwork, all files)
set -e
REPO=$(cd "$(dirname "$0")/../.." && pwd)
B=${HG_BUILD:-$HOME/hostgiac}
D=${1:-fixwork}; shift || true
rm -rf $B/$D; mkdir -p $B/$D/a $B/$D/b
if [ $# -eq 0 ]; then set -- $(cd $REPO/src/giac && ls *.cc *.h); fi
for f in "$@"; do cp $REPO/src/giac/$f $B/$D/a/; cp $REPO/src/giac/$f $B/$D/b/; done
# the working-tree copies must be HEAD's (no uncommitted edits in them)
mkdir -p $B/$D/head && git -c safe.directory='*' -C $REPO/src/giac archive HEAD "$@" | tar -x -C $B/$D/head
for f in "$@"; do cmp -s <(tr -d '\r' < $B/$D/a/$f) <(tr -d '\r' < $B/$D/head/$f) || echo "WARNING: $f differs from HEAD"; done
rm -rf $B/$D/head
echo "$B/$D ready ($# files)"
