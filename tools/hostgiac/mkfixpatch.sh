#!/bin/bash
# Development helper: a patch from $HG_BUILD/<dir> (a = repo, b = edited), paths a/<file> b/<file>
# relative to src/giac (apply there with: patch -p1 < file, or git apply).
# Usage: mkfixpatch.sh [dir] [output] (default: fixwork fix.patch)
HG=$(cd "$(dirname "$0")" && pwd)
B=${HG_BUILD:-$HOME/hostgiac}
D=${1:-fixwork}; OUT=${2:-fix.patch}
HDR=$HG/$OUT.header
cd $B/$D && { [ -f $HDR ] && cat $HDR; LC_ALL=C diff -ruN a b | sed -E 's#^diff -ruN a/(.*) b/(.*)$#diff --git a/\1 b/\2#; s#^(---|\+\+\+) ((a|b)/[^\t]*)\t.*#\1 \2#'; } > $HG/$OUT
grep "^+++ " $HG/$OUT
