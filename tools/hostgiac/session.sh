#!/bin/bash
# One long session: every expression of the given files in one process (as a calculator session),
# the whole list N times; prints the allocator's net growth per round (round 1 includes the
# one-time initialisations). Expressions that stop the process under ASan (pre-existing
# use-after-free reads, fixed by extra.patch) are left out. LeakSanitizer under-reports here (the
# -m bookkeeping table keeps pointers): use leakcheck.sh for leaks, this for growth.
# Usage: session.sh [variant] [N] [driver options] (default: fixed 3 -k)
HG=$(cd "$(dirname "$0")" && pwd)
B=${HG_BUILD:-$HOME/hostgiac}
V=${1:-fixed}; N=${2:-3}; shift $(( $# < 2 ? $# : 2 )); OPTS=${*:--k}
mkdir -p $B/logs
cat $HG/exprs_default.txt $HG/exprs_more.txt $HG/exprs_cases.txt $HG/exprs_catalog.txt | grep -v '^#' | grep -v '^\s*$' \
  | grep -vxF -e '(a+b)/' -e 'a+2b/' -e ' in range(1,10)' -e '5=>a' -e 'map(lambda x:x*x,[1,2,3])' -e 'point(1,2)' -e 'point(1+2i)' \
  | awk '!seen[$0]++' > $B/logs/session_exprs.txt
export ASAN_OPTIONS=detect_leaks=1:malloc_context_size=30
timeout 3600 $B/bin/hostgiac-$V -n $N -m $OPTS -f $B/logs/session_exprs.txt > $B/logs/session_$V.log 2>&1
echo "$V: $(wc -l < $B/logs/session_exprs.txt) expressions x $N rounds, options $OPTS"
for r in $(seq 1 $N); do
  grep "^round $r " $B/logs/session_$V.log | sed -E 's/.*allocator total ([+-][0-9]+).*/\1/' | awk -v r=$r '{s+=$1} END {printf "  round %d: net growth %+d bytes over %d evaluations\n",r,s,NR}'
done
grep -E "^SUMMARY|ERROR: (Leak|Address)Sanitizer" $B/logs/session_$V.log | head -5
