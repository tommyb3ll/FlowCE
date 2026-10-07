#!/bin/bash
# hk.sh [-raw] [-F] [-T] [file]  (WSL) FlowCE's answers (host -k path) to the expressions of file,
# or of stdin; see hk. HGBIN: another binary (default ~/hostgiac/bin/hostgiac-worktree).
B=${HGBIN:-$HOME/hostgiac/bin/hostgiac-worktree}
export ASAN_OPTIONS=detect_leaks=0
[ -n "$TRACE" ] && export ANSWER_TRACE=1
K=-k; X=
while [ $# -gt 0 ]; do
  case "$1" in -raw) K=;; -F) X="$X -F";; -T) X="$X -T";; *) break;; esac; shift
done
f=${1:-}
if [ -z "$f" ]; then f=/tmp/hk_in_$$.txt; cat > "$f"; fi
cd /tmp && "$B" -p $K $X -f "$f"
