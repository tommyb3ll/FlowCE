#!/bin/bash
# Translates the per-call leaks found on the host into calculator (eZ80) heap bytes: every leaked
# block is a uSTL buffer of gens (host 16 bytes per gen, eZ80 5); on the CE a block of <=16 bytes
# goes to the allocator's small-block pools (tab2/tab3/tab6, 288 slots each) until they are full,
# a larger one costs its size + 6 bytes of header (allocator_custom.c).
# Usage: ce_estimate.sh [orig|fixed] [expression file] [driver options]
HG=$(cd "$(dirname "$0")" && pwd)
B=${HG_BUILD:-$HOME/hostgiac}
V=${1:-orig}; F=${2:-$HG/exprs_default.txt}; shift $(( $# < 2 ? $# : 2 ))
export ASAN_OPTIONS=detect_leaks=1:malloc_context_size=4 LSAN_OPTIONS=report_objects=1
mkdir -p $B/logs
printf "%-34s %12s %10s %18s %18s\n" "expression ($V $*)" "host B/call" "blocks" "CE heap B/call" "CE (pools full)"
grep -v '^#' "$F" | grep -v '^\s*$' | while IFS= read -r e; do
  echo "$e" > $B/logs/ce_expr.txt
  for n in 1 3; do $B/bin/hostgiac-$V -n $n -f $B/logs/ce_expr.txt "$@" 2>&1 | grep -oE "^0x[0-9a-f]+ \([0-9]+ bytes\)" | grep -oE "[0-9]+ bytes" | cut -d' ' -f1 > $B/logs/ce_sizes_$n.txt; done
  python3 - "$e" $B/logs/ce_sizes_1.txt $B/logs/ce_sizes_3.txt <<'PY'
import sys
def load(p): return [int(x) for x in open(p).read().split()]
def ce(sizes, pools_full):
    t = 0
    for s in sizes:
        c = (s // 16) * 5 if s % 16 == 0 else s * 5 // 16
        if c > 16 or pools_full: t += max(c, 6) + 6
    return t
a, b = load(sys.argv[2]), load(sys.argv[3])
host = (sum(b) - sum(a)) / 2; nb = (len(b) - len(a)) / 2
print("%-34s %12d %10d %18d %18d" % (sys.argv[1], host, nb, (ce(b, False) - ce(a, False)) / 2, (ce(b, True) - ce(a, True)) / 2))
PY
done
