#!/bin/bash
# Development helper: regenerates host.patch from the edited work copy (see setup_work.sh)
HG=$(cd "$(dirname "$0")" && pwd)
B=${HG_BUILD:-$HOME/hostgiac}
cd $B && diff -ruN pristine work > $HG/host.patch
grep -c "^+++ " $HG/host.patch | sed 's/^/files changed: /'
