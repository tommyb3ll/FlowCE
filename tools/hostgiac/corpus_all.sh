#!/bin/bash
# all expression files through corpus.sh (V=orig|fixed|worktree, N rounds, extra driver options)
HG=$(cd "$(dirname "$0")" && pwd)
V=${1:-fixed}; N=${2:-2}; shift $(( $# < 2 ? $# : 2 ))
for f in exprs_default.txt exprs_more.txt exprs_cases.txt exprs_catalog.txt; do
  bash $HG/corpus.sh $V $HG/$f $N "$@" | grep -v "^details"
done
