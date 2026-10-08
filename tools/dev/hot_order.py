#!/usr/bin/env python3
"""Reorder the functions of the LTO assembly (obj/lto.src) so that the hot ones sit together.

The newer CEs (hardware revision M and later: every TI-84 Plus CE Python) read their serial flash
through an 8 KB, 2-way cache of 32-byte lines, and a miss costs about 197 CPU cycles. Misses were
about 30% of FlowCE's calculation time on the emulator (which models that cache), mostly in
small functions that run all the time (malloc, free, gen copies, operator_equal) but lie far
apart in the 2.8 MB app, so they keep evicting each other. Moved together to the start of .text,
in the order file's order, they share the cache instead of fighting over the same lines.

Only the order of whole functions changes: each is a block from one `section .text` directive to
the next, the code inside is untouched, and other sections stay where they are.

usage: hot_order.py <lto.src> <order file> <out.src>
       order file: one assembler label per line (# comments); labels not in lto.src are skipped.
"""
import re, sys

src, order_file, out = sys.argv[1:4]
lines = open(src, 'rb').read().split(b'\n')
starts = [i for i, l in enumerate(lines) if l.startswith(b'\tsection\t')]
head = lines[:starts[0]]
chunks = [lines[s:e] for s, e in zip(starts, starts[1:] + [len(lines)])]
label = re.compile(rb'^([A-Za-z_.$][A-Za-z0-9_.$@?]*):')
where = {}
first_fn = None
for k, c in enumerate(chunks):
    if c[0].split(b'\t')[2].split(b',')[0] != b'.text':
        continue
    for l in c[1:]:
        m = label.match(l)
        if m:
            where.setdefault(m.group(1), k)
            if first_fn is None:
                first_fn = k
            break
want, missing = [], []
for l in open(order_file, encoding='utf-8'):
    l = l.split('#')[0].strip()
    if not l:
        continue
    k = where.get(l.encode())
    if k is None:
        missing.append(l)
    elif k not in want:
        want.append(k)
moved = set(want)
order = [k for k in range(first_fn) if k not in moved] + want + [k for k in range(first_fn, len(chunks)) if k not in moved]
assert sorted(order) == list(range(len(chunks)))
res = head + [l for k in order for l in chunks[k]]
assert len(res) == len(lines)
open(out, 'wb').write(b'\n'.join(res))
print(f'hot_order: {len(want)} functions moved to the start of .text'
      + (f', {len(missing)} not found: {" ".join(missing[:5])}' if missing else ''))
