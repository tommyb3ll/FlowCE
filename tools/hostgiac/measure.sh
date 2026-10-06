#!/bin/bash
# Heap growth of every evaluation (driver -m): blocks allocated during it and still allocated
# after it, and the allocator's net change, round by round.
# Usage: measure.sh [variant] [rounds] [driver options...] (default: head 3, the default list)
B=${HG_BUILD:-$HOME/hostgiac}
V=${1:-head}; N=${2:-3}; shift $(( $# < 2 ? $# : 2 ))
export ASAN_OPTIONS=detect_leaks=0:malloc_context_size=40
timeout 900 $B/bin/hostgiac-$V -n $N -m "$@" 2>&1 | grep -E "^round| -> "
