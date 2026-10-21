#!/usr/bin/env python3
"""v15_carrycheck.py <lto.s>: refuses a CE toolchain v15 build where a subtraction (sbc hl, bc|de) takes
its borrow from earlier arithmetic instead of a cleared carry (tools/dev/wsl_build_v15.sh runs it on
the LTO step's assembly, after v15_biascheck.py).

clang 19 drops the `or a, a` before a subtraction when it believes the carry is clear. In
ui_rrect_r (2026-10-21) it was not: `-r` had just been computed (or a, a; sbc hl, hl; sbc hl, bc),
its borrow went into the loop's row count (j1 - j - 1), and a 3 px high tile ran 16 M rows into
flash: a reset at startup. Within a basic block, walking back from each `sbc hl, bc|de` over
instructions that keep the carry (loads, push/pop, lea, ex, 16/24-bit inc/dec, sbc hl, hl), the
carry must come from or/and/xor/scf, from `ld r, 0; add r, sp` (a frame's set-up: no carry), or
from flags saved before (push af ... pop af). Exit 1 and the places when another source is found.
"""
import re, sys

lines = open(sys.argv[1], encoding='utf-8', errors='replace').read().split('\n')
KEEP = re.compile(r'^(ld|push|pop|lea|pea|ex|exx|nop|mlt|inc|dec)\b')  # (8-bit inc/dec keep the carry too)
CLEAR = re.compile(r'^(or|and|xor|scf)\b')
ARITH = re.compile(r'^(add|adc|sub|sbc|cp|neg|rla|rra|rlca|rrca|rl|rr|sla|sra|srl|rlc|rrc|daa|ccf|tst)\b')
func, bad = '?', []
for i, l in enumerate(lines):
    m = re.match(r'^([A-Za-z_$][\w.$]*):', l)
    if m:
        func = m.group(1)
        continue
    x = l.split(';')[0].strip()
    if not re.match(r'^sbc\s+hl,\s*(bc|de)$', x):
        continue
    j, src = i - 1, None
    while j >= 0:
        y = lines[j].split(';')[0].strip()
        if not y or y.startswith('.') and not y.startswith('.L'):
            j -= 1
            continue
        if y.endswith(':') or re.match(r'^(call|ret|jp|jr|djnz|rst)\b', y):
            break  # (a block's start, or a call: not known here)
        if y == 'pop\taf' or re.match(r'^pop\s+af$', y):  # the flags as saved: walk on from the push
            while j >= 0 and not re.match(r'^push\s+af$', lines[j].split(';')[0].strip()):
                j -= 1
            j -= 1
            continue
        if re.match(r'^sbc\s+hl,\s*hl$', y) or KEEP.match(y):
            j -= 1
            continue
        if CLEAR.match(y):
            break
        m2 = re.match(r'^add\s+(ix|iy|hl),\s*sp$', y)
        if m2:  # a frame's set-up: ld r, 0 (or a small offset into the frame) then add r, sp
            p = lines[j - 1].split(';')[0].strip() if j else ''
            if re.match(r'^ld\s+%s,\s*0$' % m2.group(1), p):
                break
        if ARITH.match(y):
            src = (j, y)
        break
    if src:
        bad.append('%s: line %d `%s` takes the carry of line %d `%s`' % (func, i + 1, x, src[0] + 1, src[1]))
if bad:
    print('v15_carrycheck: a subtraction uses a stale borrow (clang 19 carry bug):')
    for b in bad:
        print('  ' + b)
    sys.exit(1)
