#!/usr/bin/env bash
# Build and run the mathedit host tests (cases + property fuzz).
#   bash tools/mathedit/run_mathedit_tests.sh [seed [fuzz-steps]]
#   wsl.exe -d Ubuntu-22.04 -- bash tools/mathedit/run_mathedit_tests.sh
# Finds mathedit.cc next to this script, else in ../../src (tests/host layout).
# Uses $CXX (default g++) with -fsanitize=address,undefined; a toolchain without the sanitizer
# runtimes (MinGW) falls back to UBSan trap mode. Prints PASS/FAIL per case and a summary;
# exits non-zero on any failure.
set -u
here=$(cd "$(dirname "$0")" && pwd)
src=$here
[ -f "$src/mathedit.cc" ] || src=$(cd "$here/../../src" && pwd)
CXX=${CXX:-g++}
out=$(mktemp -d 2>/dev/null || echo "${TMPDIR:-/tmp}/mathedit_tests.$$")
mkdir -p "$out"
trap 'rm -rf "$out"' EXIT

base="-std=c++14 -Wall -Wextra -Werror -g -O1 -I$src"
san="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"
build() {
  $CXX $base $san "$src/mathedit.cc" "$here/test_mathedit.cc" -o "$out/test_mathedit" 2>"$out/err.txt"
}
if ! build; then
  if grep -qiE "asan|ubsan|sanitiz" "$out/err.txt"; then
    echo "note: $CXX has no ASan/UBSan runtime here; using -fsanitize=undefined (trap)"
    san="-fsanitize=undefined -fsanitize-undefined-trap-on-error -D_GLIBCXX_ASSERTIONS"
    if ! build; then cat "$out/err.txt"; echo "FAIL: build"; exit 1; fi
  else
    cat "$out/err.txt"; echo "FAIL: build"; exit 1
  fi
fi
echo "compiler: $($CXX --version | head -n 1)"
echo "flags:    $base $san"

"$out/test_mathedit" "$@"
rc=$?
if [ $rc -ne 0 ]; then echo "FAIL: test_mathedit exit code $rc"; exit 1; fi
exit 0
