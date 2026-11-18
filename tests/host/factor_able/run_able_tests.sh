#!/bin/sh
# Host test of the steps screen's gate (focus_steps_able, src/focus_steps.cc), table cases.txt.
# Usage (through .orch/mb, after tools/hostgiac/build.sh worktree built the objects):
#   sh tests/host/factor_able/run_able_tests.sh [table]   (HG_BUILD: the host build dir; -p: see test_able.cc)
# Links test_able.cc with the host objects (all but the driver's, which has the main).
# Exit status: 0 all passed, 1 test failure, 2 build error.
here=$(cd "$(dirname "$0")" && pwd) #
root=$(cd "$here/../../.." && pwd) #
o=${HG_BUILD:-$HOME/hostgiac}/obj-worktree #
tmp=$(mktemp -d 2>/dev/null) || tmp="${TMPDIR:-/tmp}/able_tests.$$" #
mkdir -p "$tmp" || exit 2 #
trap 'rm -rf "$tmp"' EXIT #
exe="$tmp/test_able.exe" #
objs=$(find "$o" -name '*.o' ! -name driver.o) #
if ! g++ -std=c++14 -Wall -Wextra -Werror -o "$exe" "$here/test_able.cc" $objs -lm; then #
  echo "BUILD FAILED" #
  exit 2 #
fi #
t=${1:-$here/cases.txt} #
[ $# -gt 0 ] && shift #
"$exe" "$t" "$@" #
