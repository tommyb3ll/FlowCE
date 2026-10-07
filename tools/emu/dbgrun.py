#!/usr/bin/env python3
"""Runs expressions on a fresh copy of an emulator state and prints the calculator's debug output
(dbg_printf) lines that contain a filter, after each one: for checking what code does on the
calculator itself (32-bit doubles, the eZ80) when the host build behaves differently.

Usage (WSL): dbgrun.py <state> <filter> expr [expr ...]
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cemu import Emu, KB, map_symbol

state, filt = sys.argv[1], sys.argv[2]
for expr in sys.argv[3:]:
    e = Emu(image=f'{KB}/emu/states/{state}.ce', shotdir='/tmp')
    try:
        e.run(800)
        e.wait_idle(timeout=5000)
        e.paste(expr)
        e.key('enter')
        t = e.wait_idle(timeout=600000)
        e.run(500)
        print(f'== {expr}: {t} ms', flush=True)
        for line in e.debug_output():
            if filt in line:
                print('  ' + line, flush=True)
        a = map_symbol('_flowce_dbg', mapfile=e.mapfile)  # (a probe array of 16 longs, if any)
        if a is not None:
            b = e.peek(a, 64)
            print('  flowce_dbg', [int.from_bytes(b[4 * k:4 * k + 4], 'little', signed=True) for k in range(16)], flush=True)
    finally:
        e.close()
