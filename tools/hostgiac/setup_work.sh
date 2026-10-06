#!/bin/bash
# Development helper: fresh copies of the repo's sources in $HG_BUILD/pristine and $HG_BUILD/work
# (work is then edited for the host and diffed into host.patch). build.sh is the reproducible path.
set -e
REPO=$(cd "$(dirname "$0")/../.." && pwd)
B=${HG_BUILD:-$HOME/hostgiac}
for d in pristine work; do
  rm -rf $B/$d
  mkdir -p $B/$d/giac $B/$d/ustl $B/$d/ustlsrc $B/$d/src
  cp $REPO/src/giac/*.cc $REPO/src/giac/*.h $B/$d/giac/
  cp $REPO/ustl/* $B/$d/ustl/
  cp $REPO/src/ustl/*.cc $B/$d/ustlsrc/
  for h in k_csdk.h k_defs.h dbg.h graphic.h console.h calc.h menuGUI.h main.h file.h textGUI.h; do cp $REPO/src/$h $B/$d/src/; done
done
echo "work copy in $B/work"
