#!/usr/bin/env python3
"""Screens audit: from a fresh copy of a state, press each key sequence and screenshot what
appears, to find the screens still in the classic KhiCAS style.

Usage (WSL): audit.py <state> <casefile> [outdir]
  casefile lines: name<TAB>keys   (keys as edit2d.py: text, {key,key} raw keys; # = comment)
Env: GAP (ms after each key, default 300), SETTLE (ms before the shot, default 1500).
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cemu import Emu, KB
from edit2d import keyseq


def main():
    state, casefile = sys.argv[1], sys.argv[2]
    out = sys.argv[3] if len(sys.argv) > 3 else '/mnt/c/Users/tjb43/Downloads/Projects/khicas/emu-shots/audit'
    os.makedirs(out, exist_ok=True)
    gap, settle = int(os.environ.get('GAP', '300')), int(os.environ.get('SETTLE', '1500'))
    for line in open(casefile, encoding='utf-8'):
        line = line.rstrip('\n')
        if not line.strip() or line.startswith('#'):
            continue
        name, keys = line.split('\t')[:2]
        e = Emu(image=f'{KB}/emu/states/{state}.ce', shotdir=out)
        try:
            e.run(800)
            for k in keyseq(keys):
                e.cmd(f'key {k} 80'); e.ms += 80
                e.run(gap)
            e.run(settle)
            e.shot(name)
            print(name, flush=True)
        finally:
            e.close()


if __name__ == '__main__':
    main()
