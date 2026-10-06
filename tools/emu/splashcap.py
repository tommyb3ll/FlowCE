#!/usr/bin/env python3
"""Frames of the start animation: quit the app, relaunch it from APPS, screenshot every <step> ms.

usage: splashcap.py <state> <outdir> [step_ms] [count]
"""
import os
import sys
from cemu import Emu, KB


def main():
    state, out = sys.argv[1], sys.argv[2]
    step = int(sys.argv[3]) if len(sys.argv) > 3 else 100
    count = int(sys.argv[4]) if len(sys.argv) > 4 else 16
    os.makedirs(out, exist_ok=True)
    e = Emu(image=f'{KB}/emu/states/{state}.ce', shotdir=out)
    try:
        e.run(800)
        e.wait_idle(timeout=5000)
        e.type('{2nd,mode}')          # quit to the OS
        e.run(1500)
        e.type('{apps}')
        e.run(800)
        e.cmd('key 2 80'); e.ms += 80  # 2:FlowCE (after Finance)
        for i in range(count):
            e.run(step)
            e.shot(f'splash_{i:02d}')
    finally:
        e.close()


if __name__ == '__main__':
    main()
