#!/bin/bash
# Reachable growth check (what LeakSanitizer cannot see: caches, global lists): each expression is
# evaluated N times in one process (-m); the allocator's total must stop growing after round 2.
# Prints the expressions whose rounds 3..N still grow. Usage: growth.sh [orig|fixed] [file] [N] [opts]
HG=$(cd "$(dirname "$0")" && pwd)
B=${HG_BUILD:-$HOME/hostgiac}
V=${1:-fixed}; F=${2:-$HG/exprs_default.txt}; N=${3:-4}; shift $(( $# < 3 ? $# : 3 ))
export OPTS="$*" EXE=$B/bin/hostgiac-$V N D=$B/growth/$V-$(basename "$F" .txt)
export ASAN_OPTIONS=detect_leaks=0
rm -rf $D; mkdir -p $D
grep -v '^#' "$F" | grep -v '^\s*$' | nl -nrz -w3 -s'	' > $D/list.txt
one(){
  local id=${1%%	*} e=${1#*	}
  printf '%s\n' "$e" > $D/$id.expr
  timeout ${TMO:-120} $EXE -n $N -m $OPTS -f $D/$id.expr > $D/$id.log 2>&1
  local g=$(grep -E "^round ([3-9]|[1-9][0-9])" $D/$id.log | sed -E 's/.*allocator total ([+-][0-9]+).*/\1/' | awk '{s+=$1} END {if (NR) print s}')
  printf '%s\t%s\t%s\n' "$id" "${g:-NA}" "$e" > $D/$id.status
}
export -f one
tr '\n' '\0' < $D/list.txt | xargs -0 -P ${JOBS:-4} -I{} bash -c 'one "$@"' _ {}
cat $D/*.status | sort > $D/summary.txt
echo "== $V $(basename $F): $(wc -l < $D/summary.txt) expressions, rounds 3..$N net growth (bytes):"
awk -F'\t' '$2!="0" {print}' $D/summary.txt | head -40
echo "($(awk -F'\t' '$2=="0"' $D/summary.txt | wc -l) with no growth)"
