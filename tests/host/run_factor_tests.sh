#!/bin/bash
# Host tests of the factoring steps (src/steps.cc through the host giac binary).
# Usage: bash tests/host/run_factor_tests.sh <binary> [--regen]
#   <binary>  the host giac driver (tools/hostgiac/build.sh worktree: $HG_BUILD/bin/hostgiac-worktree)
#   --regen   rewrite tests/host/factor/golden/*.txt from the binary's current output (their
#             problems come from golden/NAME.in). Only on purpose: the goldens lock behaviour.
# What it runs, each PASS/FAIL per case, a summary last, exit status 1 on any failure:
#   1. every golden (golden/*.txt except todo_multivar) against the binary's `-S` output (times
#      stripped: "[ok, 5 steps, 12 ms]" is "[ok, 5 steps]"), one process per problem, JOBS at a time
#   2. tools/hostgiac/factorcheck.py on the goldens' factor(...) problems, on factor/corpus_*.txt and
#      on tools/hostgiac/corpus_factor_*.txt (when present): every displayed step proved valid
#   3. the checker's self-test (factor/selftest: wrong outputs it must flag, right ones it must pass)
#   (known_wrong.txt: failing problems written up as engine defects show as KNOWN-WRONG, counted in the last line)
#   4. golden/todo_multivar: problems with no steps today; reports NOSTEPS / NOW HAS STEPS, never fails
here=$(cd "$(dirname "$0")" && pwd)
root=$(cd "$here/../.." && pwd)
fdir=$here/factor
bin=; regen=0
for a in "$@"; do
  case $a in
    --regen) regen=1 ;;
    *) bin=$a ;;
  esac
done
[ -n "$bin" ] || { sed -n '2,3p' "$0"; exit 2; }
[ -x "$bin" ] || { echo "no such binary: $bin"; exit 2; }
bin=$(cd "$(dirname "$bin")" && pwd)/$(basename "$bin")
# the checker is Python 3 (on MSYS2 the Windows one: PYTHON, or python3/python on PATH)
py=${PYTHON:-}
if [ -z "$py" ]; then
  for c in python3 python; do
    if command -v "$c" >/dev/null 2>&1 && "$c" -c 'import sys; sys.exit(sys.version_info<(3,6))' >/dev/null 2>&1; then py=$c; break; fi
  done
fi
[ -n "$py" ] || { echo "no Python 3 found (set PYTHON)"; exit 2; }
W(){ if command -v cygpath >/dev/null 2>&1; then cygpath -m "$1"; else printf '%s\n' "$1"; fi; }
check="$py -I $(W "$root/tools/hostgiac/factorcheck.py") $(W "$bin")"
export B=$bin ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0}
T=$(mktemp -d) || exit 2
trap 'rm -rf "$T"' EXIT
trap 'exit 130' INT TERM
export T
pass=0; fail=0; kwrong=0
ok(){ pass=$((pass+1)); echo "PASS  $1"; }
bad(){ fail=$((fail+1)); echo "FAIL  $1"; }

# one: "-S" of one problem into $T/run/NNNNN (times stripped)
one(){
  n=${1%%$'\t'*}; e=${1#*$'\t'}
  timeout 60 "$B" -S -e "$e" > "$T/run/$n.raw" 2>&1 || echo "== $e   [TIMEOUT or crash]" >> "$T/run/$n.raw"
  sed -E 's/, [0-9.]+ ms\]$/]/' "$T/run/$n.raw" > "$T/run/$n"
  rm -f "$T/run/$n.raw"
}
export -f one
# runlist <file with one problem per line> <out>: their outputs, in order
runlist(){
  rm -rf "$T/run"; mkdir -p "$T/run"
  grep -v -E '^(#|[[:space:]]*$)' "$1" | tr -d '\r' | awk '{printf "%05d\t%s\n",NR,$0}' > "$T/list"
  tr '\n' '\0' < "$T/list" | xargs -0 -P "${JOBS:-8}" -I{} bash -c 'one "$1"' _ {}
  cat "$T"/run/[0-9]* > "$2" 2>/dev/null || : > "$2"
}
problems_of(){ sed -n 's/^== \(.*\)   \[.*$/\1/p' "$1"; }
# split <file> <dir>: one file per "== " block (comment lines dropped)
split_blocks(){ rm -rf "$2"; mkdir -p "$2"; awk -v d="$2" '/^#/ {next} /^== / {n++} n {print > sprintf("%s/%05d", d, n)}' "$1"; }

if [ $regen = 1 ]; then
  for inp in "$fdir"/golden/*.in; do
    name=$(basename "$inp" .in)
    runlist "$inp" "$T/out.txt"
    {
      echo "# golden: the exact -S output of the engine, times stripped (\"[ok, 5 steps, 12 ms]\" is \"[ok, 5 steps]\")."
      echo "# Written by: bash tests/host/run_factor_tests.sh <binary> --regen   (problems: golden/$name.in). Only on purpose."
      cat "$T/out.txt"
    } > "$fdir/golden/$name.txt"
    echo "regenerated golden/$name.txt ($(grep -c '^== ' "$T/out.txt") problems)"
  done
  exit 0
fi

# ---- 1. goldens
echo "== goldens"
for g in "$fdir"/golden/*.txt; do
  name=$(basename "$g" .txt)
  [ "$name" = todo_multivar ] && continue
  problems_of "$g" > "$T/probs"
  runlist "$T/probs" "$T/actual.txt"
  split_blocks "$g" "$T/exp"; split_blocks "$T/actual.txt" "$T/act"
  nf=0; nc=0
  for e in "$T"/exp/*; do
    [ -e "$e" ] || continue
    nc=$((nc+1)); b=$(basename "$e")
    if ! cmp -s "$e" "$T/act/$b"; then
      nf=$((nf+1)); bad "golden/$name: $(head -1 "$e" | sed 's/   \[.*//; s/^== //')"
      diff "$e" "$T/act/$b" | head -8 | sed 's/^/        /'
    fi
  done
  if [ "$(ls "$T/act" | wc -l)" != "$nc" ]; then bad "golden/$name: $nc cases expected, $(ls "$T/act" | wc -l) produced"; nf=$((nf+1)); fi
  [ $nf = 0 ] && ok "golden/$name ($nc cases identical)"
done

# ---- 2. the checker on the goldens' factor(...) problems, on the corpora
echo "== factorcheck"
gl=
for g in "$fdir"/golden/*.txt; do
  case $(basename "$g" .txt) in callers) ;; *) gl="$gl $(W "$g")" ;; esac
done
corp=
for c in "$fdir"/corpus_*.txt "$root"/tools/hostgiac/corpus_factor_*.txt; do
  [ -e "$c" ] && corp="$corp $(W "$c")"
done
wrongf=$(W "$fdir/known_wrong.txt")
report(){ # <label> <log>: the checker's verdict as one PASS/FAIL
  kwrong=$((kwrong + $(awk '/^known-wrong:/ {s+=$2} END {print s+0}' "$2")))
  grep -E '^KNOWN-WRONG' "$2" | sed 's/^/        /'
  grep -E '^(FAIL|NOW-HAS-STEPS|[0-9]+ problems|steps-tagged|verified)' "$2" | sed 's/^/        /'
  if grep -q -E '^FAIL' "$2" || ! grep -q -E '^[0-9]+ problems: .* 0 fail' "$2"; then bad "$1"; else ok "$1"; fi
}
$check --known-wrong "$wrongf" --golden $gl > "$T/c_golden.log" 2>&1; report "factorcheck on the goldens (every displayed step)" "$T/c_golden.log"
known=$(W "$fdir/known_nosteps.txt")
for c in $corp; do
  $check --known "$known" --known-wrong "$wrongf" "$c" > "$T/c.log" 2>&1; report "factorcheck on $(basename "$c")" "$T/c.log"
  # never green on nothing: steps-tagged lines but no step line verified
  tg=$(awk '/^steps-tagged:/ {print $6}' "$T/c.log")
  vf=$(awk '/^verified:/ {print $2}' "$T/c.log")
  if [ "${tg:-0}" -gt 0 ] && [ "${vf:-0}" = 0 ]; then bad "$(basename "$c"): $tg steps-tagged problems and nothing verified"; fi
  grep -q '^NOW-HAS-STEPS' "$T/c.log" && echo "        NOW HAS STEPS: delete these from factor/known_nosteps.txt"
done

# ---- 3. the checker's self-test
echo "== selftest"
$py -I "$(W "$root/tools/hostgiac/factorcheck.py")" --selftest "$(W "$bin")" "$(W "$fdir/selftest")" > "$T/self.log" 2>&1
tail -1 "$T/self.log" | sed 's/^/        /'
if grep -q ', 0 mistakes$' "$T/self.log"; then ok "checker self-test"; else grep '^FAIL' "$T/self.log"; bad "checker self-test"; fi

# ---- 4. todo_multivar: documents the gap, never fails
echo "== todo_multivar (no steps today)"
td=$fdir/golden/todo_multivar.txt
if [ -e "$td" ]; then
  problems_of "$td" > "$T/probs"; runlist "$T/probs" "$T/todo.txt"
  nos=$(grep -c '^== .*\[NO STEPS' "$T/todo.txt"); tot=$(grep -c '^== ' "$T/todo.txt")
  echo "todo_multivar: $nos of $tot still have no steps"
  grep '^== ' "$T/todo.txt" | grep -v '\[NO STEPS' | sed 's/   \[.*//; s/^== /NOW HAS STEPS  /'
  $check --golden "$(W "$td")" > "$T/c_todo.log" 2>&1
  grep -E '^(FAIL|[0-9]+ problems)' "$T/c_todo.log" | sed 's/^/        /'
  grep -q -E '^FAIL' "$T/c_todo.log" && bad "todo_multivar: steps that now exist fail the checker"
fi

echo "factor tests: $pass passed, $fail failed, $kwrong known-wrong"
[ $fail = 0 ]
