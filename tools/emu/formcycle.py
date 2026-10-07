#!/usr/bin/env python3
"""F4's forms of results, as a user cycles them: for each expression, a fresh copy of the state,
the expression pasted and run, then F4 pressed N times. Prints the emulated time of each press
(until KhiCAS polls the keypad again) and saves a screenshot after each one.

Usage (WSL): formcycle.py <state> <shotdir> <presses> expr [expr ...]
Shots: <shotdir>/fc<i>_<press>.png (press 0: the result itself).
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cemu import Emu, KB

state, outdir, n = sys.argv[1], sys.argv[2], int(sys.argv[3])
os.makedirs(outdir, exist_ok=True)
for i, expr in enumerate(sys.argv[4:]):
    e = Emu(image=f'{KB}/emu/states/{state}.ce', shotdir=outdir)
    try:
        e.run(800)
        e.wait_idle(timeout=5000)
        e.paste(expr)
        e.key('enter')
        t = e.wait_idle(timeout=300000)
        e.run(300)
        e.shot(f'fc{i}_0')
        times = [t]
        for k in range(1, n + 1):
            e.key('trace')
            times.append(e.wait_idle(timeout=300000))
            e.run(300)
            e.shot(f'fc{i}_{k}')
        print(f'{expr}: result {times[0]} ms; F4 presses ' + ' '.join(f'{x}' for x in times[1:]) + ' ms', flush=True)
    finally:
        e.close()
