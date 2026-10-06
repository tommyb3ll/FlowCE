#!/usr/bin/env python3
"""One long console session (no reload between problems): free heap after each evaluation
(heap_free_probe, main.cc), evaluation time, and a screenshot of each result. Finds leaks and
the memory high-water marks that crash a real calculator after a while.

Usage (WSL): session_mem.py <state> <outdir> <casefile>
  casefile lines: name<TAB>input[<TAB>keys after the result, e.g. {trace}{trace}]
"""
import os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cemu import Emu, KB, map_symbol


def main():
    state, out, casefile = sys.argv[1], sys.argv[2], sys.argv[3]
    os.makedirs(out, exist_ok=True)
    e = Emu(image=f'{KB}/emu/states/{state}.ce', shotdir=out)
    probes = [map_symbol(n, mapfile=e.mapfile) for n in ('_heap_free_probe', '_heap_list_probe', '_heap_big_probe')]
    rd = lambda: '/'.join(str(int.from_bytes(e.peek(a, 3), 'little')) if a else '?' for a in probes)
    try:
        e.run(800); e.wait_idle(timeout=5000)
        for line in open(casefile, encoding='utf-8'):
            line = line.rstrip('\n')
            if not line.strip() or line.startswith('#'):
                continue
            parts = line.split('\t')
            name, text = parts[0], parts[1]
            t0 = e.ms
            e.paste(text); e.key('enter')
            t = e.wait_idle(timeout=300000)
            e.run(200)
            free = rd()
            if t >= 299000:  # KhiCAS never polled the keypad again: it hung or aborted (out of memory)
                e.shot(name)
                print(f'{name:14} no reply in 300 s (hung, or aborted out of memory): {free}', flush=True)
                break
            mk = map_symbol('_heap_marks', mapfile=e.mapfile)
            marks = [int.from_bytes(e.peek(mk + 3 * i, 3), 'little') for i in range(6)] if mk else []
            msg = f'{name:14} {t:7} ms  top/freed/largest {free}  marks {marks}'
            if len(parts) > 2:
                e.type(parts[2]); e.wait_idle(timeout=300000); e.run(200)
                msg += f'  after {parts[2]}: {rd()}'
            e.shot(name)
            print(msg, flush=True)
    finally:
        e.close()


if __name__ == '__main__':
    main()
