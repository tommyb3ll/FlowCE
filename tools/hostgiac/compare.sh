#!/bin/bash
# compares the results printed by corpus.sh runs of two builds (default orig vs fixed)
B=${HG_BUILD:-$HOME/hostgiac}
A=${1:-orig}; C=${2:-fixed}
for d in $B/corpus/$A-*; do
  t=${d#$B/corpus/$A-}
  [ -d $B/corpus/$C-$t ] || continue
  n=$(wc -l < $d/results.txt)
  if cmp -s $d/results.txt $B/corpus/$C-$t/results.txt; then echo "$t: $n results identical ($A vs $C)"
  else echo "$t: DIFFERENT ($A vs $C):"; diff $d/results.txt $B/corpus/$C-$t/results.txt | head -20; fi
done
