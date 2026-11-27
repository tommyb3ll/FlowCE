#!/bin/bash
# Behaviour lock for optimization work: the exact output of the host giac binary over a broad input
# set, compared with the files in tests/host/opt/golden (written from the code before any
# optimization). Any difference is a behaviour change.
# Usage (MSYS2 or Linux): bash tests/host/opt/run_golden.sh <binary> [--regen]
#   <binary>  tools/hostgiac/build.sh worktree: $HG_BUILD/bin/hostgiac-worktree
#   --regen   rewrite the goldens (only on purpose, from unoptimized code)
# Sets (each one file in golden/):
#   answers_<corpus>   -k -p : the console's answer (src/answer.cc around giac), one process per corpus
#   forms              -k -p -F : the answer and its F4 forms
#   analysis           -A : analyze(f) reports (src/analyze.cc), one process per function
#   analysis2          -A : the same, a second larger set (inputs/analysis2.txt)
#   steps_calc         -S : step-by-step solutions (src/steps.cc) of the calculus corpora, one process
#                      per problem, times stripped
#   steps_misc         -S : equations, inequalities, systems, divisions, partial fractions, completing
#                      the square (inputs: inputs/steps_misc.txt)
#   render             -V : the steps screen (src/focus_steps.cc, ui_math.cc, mathinput.cc, ui_gfx.c,
#                      ui_font.c) drawn off screen: the SHA-1 of every image (inputs/render.txt)
# The factoring steps have their own goldens and checker: tests/host/run_factor_tests.sh.
set -u
here=$(cd "$(dirname "$0")" && pwd); root=$(cd "$here/../../.." && pwd)
bin=; regen=0
for a in "$@"; do case $a in --regen) regen=1 ;; *) bin=$a ;; esac; done
[ -n "$bin" ] && [ -x "$bin" ] || { sed -n '2,6p' "$0"; exit 2; }
bin=$(cd "$(dirname "$bin")" && pwd)/$(basename "$bin")
export B=$bin ASAN_OPTIONS=${ASAN_OPTIONS:-detect_leaks=0}
T=$(mktemp -d) || exit 2; trap 'rm -rf "$T"' EXIT; export T
hg=$root/tools/hostgiac; G=$here/golden; I=$here/inputs
mkdir -p "$G" "$T/out"
strip(){ tr -d '\r' | sed -E 's/, [0-9.]+ ms\]/]/; s/  @@ *[0-9.]+$//; s/\[ok, [0-9.]+ ms\]/[ok]/; s/ @[0-9.]+ \|/ |/g; s/ @[0-9.]+$//'; } # (times out)
clean(){ grep -v -E '^(#|[[:space:]]*$)' "$1" | tr -d '\r'; }
# one process per line of a list: <option> <list> <out>
each(){
  rm -rf "$T/run"; mkdir -p "$T/run"
  clean "$2" | awk '{printf "%05d\t%s\n",NR,$0}' > "$T/list"
  export OPT=$1
  one(){ n=${1%%$'\t'*}; e=${1#*$'\t'}; ( cd "$T" && timeout 120 "$B" $OPT -e "$e" > "$T/run/$n" 2>&1 ) || echo "== $e   [TIMEOUT or crash]" >> "$T/run/$n"; }
  export -f one
  tr '\n' '\0' < "$T/list" | xargs -0 -P "${JOBS:-6}" -I{} bash -c 'one "$1"' _ {}
  cat "$T"/run/[0-9]* | strip > "$3"
}
# one process for a whole file: <options> <list> <out>
whole(){ clean "$2" > "$T/in.txt"; ( cd "$T" && timeout 900 "$B" $1 -f "$T/in.txt" 2>&1 ) | strip > "$3"; }

# the corpora written as plain inputs (numcheck's "kind ; expr ; ..." lines are skipped: raw inputs only)
plain(){ clean "$1" | grep -v ' ; ' ; }
for c in exprs_cases exprs_more exprs_default; do
  plain "$hg/$c.txt" > "$T/p_$c.txt"; whole "-k -p" "$T/p_$c.txt" "$T/out/answers_$c.txt"
done
cat "$hg/corpus_gen_diff_s1.txt" "$hg/corpus_gen_int_s1.txt" "$hg/corpus_holdout.txt" > "$T/calc.txt"
plain "$T/calc.txt" > "$T/p_calc.txt"
split -l 150 -d "$T/p_calc.txt" "$T/pc_"   # (several processes: a long session changes giac's state)
: > "$T/out/answers_calc.txt"
for f in "$T"/pc_*; do whole "-k -p" "$f" "$T/x"; cat "$T/x" >> "$T/out/answers_calc.txt"; done
cat "$hg"/corpus_factor_*.txt | sed 's/   # expect:.*//' > "$T/fac.txt"
whole "-k -p" "$T/fac.txt" "$T/out/answers_factor.txt"
whole "-k -p -F" "$I/forms.txt" "$T/out/forms.txt"
each "-A" "$I/analysis.txt" "$T/out/analysis.txt"
each "-A" "$I/analysis2.txt" "$T/out/analysis2.txt"
each "-S" "$T/calc.txt" "$T/out/steps_calc.txt"
each "-S" "$I/steps_misc.txt" "$T/out/steps_misc.txt"
each "-S" "$I/steps_apps.txt" "$T/out/steps_apps.txt"
# the steps screen: every stop of every problem, hashed
: > "$T/out/render.txt"; n=0
while IFS= read -r e; do
  n=$((n+1)); d="$T/v$n"; mkdir -p "$d"
  ( cd "$T" && timeout 120 "$B" -V "$d" -e "$e" 2>&1 ) | sed -E 's/^[0-9]+: //' | tr -d '\r' >> "$T/out/render.txt"
  for p in "$d"/*.ppm; do [ -e "$p" ] && echo "  $(basename "$p") $(sha1sum < "$p" | cut -c1-40)" >> "$T/out/render.txt"; done
done < <(clean "$I/render.txt")

if [ $regen = 1 ]; then cp "$T"/out/*.txt "$G"/; wc -l "$G"/*.txt | sed 's/^/regen /'; exit 0; fi
pass=0; fail=0
for g in "$G"/*.txt; do
  name=$(basename "$g")
  if cmp -s <(tr -d '\r' < "$g") "$T/out/$name"; then pass=$((pass+1)); echo "PASS  $name ($(wc -l < "$g") lines identical)"
  else fail=$((fail+1)); echo "FAIL  $name"; diff <(tr -d '\r' < "$g") "$T/out/$name" | head -12 | sed 's/^/        /'; fi
done
echo "opt goldens: $pass passed, $fail failed"
[ $fail = 0 ]
