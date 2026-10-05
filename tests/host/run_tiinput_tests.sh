#!/bin/sh
# Host unit tests for src/tiinput.cc (TI-style implicit multiplication).
# Usage: sh tests/host/run_tiinput_tests.sh       (set CXX to use another g++)
# Builds with the g++ on PATH (Linux/WSL, or Windows MinGW/Strawberry from Git
# Bash) into a temporary directory, runs the test, and prints PASS/FAIL per
# case and a summary. Exit status: 0 all passed, 1 test failure, 2 build error.
# Every command line ends in '#' and there are no blank lines, so the script
# also runs from a Windows checkout with CRLF line endings (core.autocrlf).
here=$(cd "$(dirname "$0")" && pwd) #
root=$(cd "$here/../.." && pwd) #
cxx=${CXX:-g++} #
tmp=$(mktemp -d 2>/dev/null) || tmp="${TMPDIR:-/tmp}/tiinput_tests.$$" #
mkdir -p "$tmp" || exit 2 #
trap 'rm -rf "$tmp"' EXIT #
trap 'exit 130' INT TERM #
exe="$tmp/test_tiinput.exe" #
echo "building: $cxx -std=c++14 -Wall -Wextra -Werror" #
if ! "$cxx" -std=c++14 -Wall -Wextra -Werror -I"$root/src" -o "$exe" "$here/test_tiinput.cc" "$root/src/tiinput.cc"; then #
  echo "BUILD FAILED" #
  exit 2 #
fi #
"$exe" #
status=$? #
exit $status #
