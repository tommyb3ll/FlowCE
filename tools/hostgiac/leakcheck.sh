#!/bin/bash
# LeakSanitizer at exit, N=1 vs N=3, one process per expression.
# Usage: leakcheck.sh [orig|fixed] [expression file] [extra driver options, e.g. -a]
HG=$(cd "$(dirname "$0")" && pwd)
B=${HG_BUILD:-$HOME/hostgiac}
V=${1:-orig}; F=${2:-$HG/exprs_default.txt}; shift $(( $# < 2 ? $# : 2 ))
EXE=$B/bin/hostgiac-$V
mkdir -p $B/logs
export ASAN_OPTIONS=detect_leaks=1:malloc_context_size=40:alloc_dealloc_mismatch=1
summary(){ grep -E "^SUMMARY: AddressSanitizer: [0-9]+ byte" "$1" | sed 's/SUMMARY: AddressSanitizer: //' ; }
leaked_bytes(){ grep -oE "^SUMMARY: AddressSanitizer: [0-9]+" "$1" | grep -oE "[0-9]+$" || echo 0; }
printf "%-34s %22s %22s %16s\n" "expression ($V)" "N=1 leaked" "N=3 leaked" "per call"
grep -v '^#' "$F" | grep -v '^\s*$' | while IFS= read -r e; do
  tag=$(echo "$e" | tr -c 'a-zA-Z0-9' '_' | cut -c1-40)
  echo "$e" > $B/logs/expr_$tag.txt
  for n in 1 3; do timeout 600 $EXE -n $n -f $B/logs/expr_$tag.txt "$@" > $B/logs/lsan_${V}_${tag}_n$n.log 2>&1; done
  b1=$(leaked_bytes $B/logs/lsan_${V}_${tag}_n1.log); b3=$(leaked_bytes $B/logs/lsan_${V}_${tag}_n3.log)
  printf "%-34s %22s %22s %16s\n" "$e" "$(summary $B/logs/lsan_${V}_${tag}_n1.log | cut -d' ' -f1,5 | sed 's/ / B in /') " "$(summary $B/logs/lsan_${V}_${tag}_n3.log | cut -d' ' -f1,5 | sed 's/ / B in /')" "$(( (b3-b1)/2 )) B"
done
echo "logs: $B/logs/lsan_${V}_*"
