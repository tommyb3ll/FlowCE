#!/usr/bin/env bash
# Build and run the mathinput host tests; also check that mi_preview builds.
#   bash tests/host/run_mathinput_tests.sh
#   wsl.exe -d Ubuntu-22.04 -- bash tests/host/run_mathinput_tests.sh
# Uses $CXX (default g++) with -fsanitize=address,undefined. Toolchains without the
# sanitizer runtimes (MinGW) fall back to UBSan trap mode plus libstdc++ assertions; the
# test then guards the input buffer with inaccessible pages instead of ASan.
# Prints PASS/FAIL per case and a summary; exits non-zero on any failure.
set -u
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
CXX=${CXX:-g++}
out=$(mktemp -d 2>/dev/null || echo "${TMPDIR:-/tmp}/mi_tests.$$")
mkdir -p "$out"
trap 'rm -rf "$out"' EXIT

base="-std=c++14 -Wall -Wextra -Werror -g -O1 -I$root/src"
san="-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer"
build() { # $1 = output, rest = flags and sources
  o=$1; shift
  $CXX $base $san "$@" -o "$o" 2>"$out/err.txt"
}

if ! build "$out/test_mathinput" -DMI_TEST "$root/src/mathinput.cc" "$here/test_mathinput.cc"; then
  if grep -qiE "asan|ubsan|sanitiz" "$out/err.txt"; then
    echo "note: $CXX has no ASan/UBSan runtime here; using -fsanitize=undefined (trap) + guard pages"
    san="-fsanitize=undefined -fsanitize-undefined-trap-on-error -D_GLIBCXX_ASSERTIONS"
    if ! build "$out/test_mathinput" -DMI_TEST "$root/src/mathinput.cc" "$here/test_mathinput.cc"; then
      cat "$out/err.txt"; echo "FAIL: build"; exit 1
    fi
  else
    cat "$out/err.txt"; echo "FAIL: build"; exit 1
  fi
fi
echo "compiler: $($CXX --version | head -n 1)"
echo "flags:    $base $san"

if ! build "$out/mi_preview" "$root/src/mathinput.cc" "$here/mi_preview.cc"; then
  cat "$out/err.txt"; echo "FAIL: mi_preview build"; exit 1
fi
echo "PASS mi_preview builds"

"$out/test_mathinput"
rc=$?
if [ $rc -ne 0 ]; then echo "FAIL: test_mathinput exit code $rc"; exit 1; fi
exit 0
