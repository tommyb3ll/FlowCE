#!/bin/bash
# Runs every expression of a file in its own process under ASan+LSan (N rounds each) and
# classifies the outcome: ok, LEAK (LeakSanitizer bytes), ASAN (memory error), TIMEOUT, CRASH.
# Usage: corpus.sh [orig|fixed] [expression file] [N] [extra driver options, e.g. -i -a]
HG=$(cd "$(dirname "$0")" && pwd)
B=${HG_BUILD:-$HOME/hostgiac}
V=${1:-fixed}; F=${2:-$HG/exprs_cases.txt}; N=${3:-2}; shift $(( $# < 3 ? $# : 3 ))
OPTS="$*"
D=$B/corpus/$V-$(basename "$F" .txt)$(echo "$OPTS" | tr -d " "); rm -rf $D; mkdir -p $D
export ASAN_OPTIONS=detect_leaks=1:malloc_context_size=40
export EXE=$B/bin/hostgiac-$V D N OPTS TMO=${TMO:-120}
grep -v '^#' "$F" | grep -v '^\s*$' | nl -nrz -w3 -s'	' > $D/list.txt
run_one(){
  local id=${1%%	*} e=${1#*	}
  printf '%s\n' "$e" > $D/$id.expr
  timeout $TMO $EXE -n $N -p $OPTS -f $D/$id.expr > $D/$id.log 2>&1; local rc=$?
  local st=ok
  if [ $rc -eq 124 ]; then st=TIMEOUT
  elif grep -q "ERROR: AddressSanitizer" $D/$id.log; then st="ASAN:$(grep -m1 -oE 'AddressSanitizer: [a-z-]+' $D/$id.log | cut -d' ' -f2)"
  elif grep -q "ERROR: LeakSanitizer" $D/$id.log; then st="LEAK:$(grep -m1 -oE 'SUMMARY: AddressSanitizer: [0-9]+' $D/$id.log | grep -oE '[0-9]+$')B"
  elif [ $rc -ne 0 ]; then st="CRASH:rc=$rc"; fi
  printf '%s\t%s\t%s\n' "$id" "$st" "$e" > $D/$id.status
}
export -f run_one
tr '\n' '\0' < $D/list.txt | xargs -0 -P ${JOBS:-4} -I{} bash -c 'run_one "$@"' _ {}
cat $D/*.status | sort > $D/summary.txt
echo "== $V: $(wc -l < $D/summary.txt) expressions, N=$N, options: $OPTS"
cut -f2 $D/summary.txt | sed 's/:.*//' | sort | uniq -c
grep -v "	ok	" $D/summary.txt | head -50
echo "details: $D/<id>.log"
# results (first round) for comparisons between builds
for f in $D/*.log; do grep -m1 " -> " "$f" || echo "(no result: $(basename $f))"; done > $D/results.txt
