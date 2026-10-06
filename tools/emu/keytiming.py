#!/usr/bin/env python3
"""Key timing scan: type a prefix with edit2d.py's timing, then press one key at several delays
after the last key, and print the edit line each time.

usage: keytiming.py <state> [prefix] [key] [delays,comma,separated]
  e.g. keytiming.py fx111 '(x+1)' del 50,100,150,200,250,300,350

Found on 2026-10-06: on one build (fx110) only, DEL pressed exactly 250 ms after ')' cleared
the whole line and LEFT moved the caret to 0; 200 and 300 ms were fine, and the builds before
and after (one unrelated 6-byte change) were fine at every delay; 2nd/alpha were off. Not
explained (the phase of the emulated key against the app's key polling?). Run this if a line
is ever reported cleared by DEL on the calculator.
"""
import sys
from cemu import Emu, KB
import edit2d


def main():
    state = sys.argv[1]
    prefix = sys.argv[2] if len(sys.argv) > 2 else '(x+1)'
    key = sys.argv[3] if len(sys.argv) > 3 else 'del'
    delays = [int(d) for d in (sys.argv[4] if len(sys.argv) > 4 else '50,100,150,200,250,300,350').split(',')]
    mf = f'{KB}/emu/states/{state}.map'
    out = []
    for pre in delays:
        e = Emu(image=f'{KB}/emu/states/{state}.ce', shotdir='/tmp')
        try:
            e.run(800)
            ks = edit2d.keyseq(prefix)
            for i, k in enumerate(ks):
                e.cmd(f'key {k} 80'); e.ms += 80
                e.run(250 if i < len(ks) - 1 else pre)
            e.cmd(f'key {key} 80'); e.ms += 80
            e.run(400)
            text, caret = edit2d.edit_line(e, mf)
            shown = text if caret is None else text[:caret] + '|' + text[caret:]
            out.append(f'{pre}ms:{shown}')
        finally:
            e.close()
    print(state, prefix, key, '  '.join(out))


if __name__ == '__main__':
    main()
