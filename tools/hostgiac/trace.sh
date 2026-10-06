#!/bin/bash
# Allocation stacks of the blocks an evaluation leaves allocated (round K of N, default 3 of 3).
# Usage: trace.sh [orig|fixed] [expression file] [driver options...]; env K, N, DEPTH
HG=$(cd "$(dirname "$0")" && pwd)
B=${HG_BUILD:-$HOME/hostgiac}
V=${1:-fixed}; F=${2:-$HG/exprs_default.txt}; shift $(( $# < 2 ? $# : 2 ))
export ASAN_OPTIONS=detect_leaks=0:malloc_context_size=${DEPTH:-30}
$B/bin/hostgiac-$V -n ${N:-3} -t ${K:-3} -d ${SHOW:-16} -f "$F" "$@" 2>&1
