#!/usr/bin/env python3
"""Type key sequences on the 2D edit line and report the text and caret (and a screenshot).

Each case starts from a fresh copy of the state, types its keys at a human pace, then reads the
edit line from memory (Edit_Line, Line[], Cursor, Start_Line in the state's map) and prints it
with | at the caret. Checks it against the expected text when one is given.

Usage (WSL): edit2d.py <state> <casefile> [outdir]
  casefile lines: name<TAB>keys[<TAB>expected]   (# = comment)
  keys: text typed char by char, {key,key} for named keys (as typecheck: {del} {left} {sq} ...)
  expected: the edit line with | at the caret, e.g. (1)/(x+|)
Env: GAP (ms after each key, default 250).
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cemu import Emu, KB, CHARKEYS, map_symbol


def keyseq(text):
    seq, i = [], 0
    while i < len(text):
        c = text[i]
        if c == '{':
            j = text.index('}', i)
            seq += text[i + 1:j].split(',')
            i = j + 1
            continue
        seq += CHARKEYS[c]
        i += 1
    return seq


def u24(b, o=0):
    return b[o] | b[o + 1] << 8 | b[o + 2] << 16


def s24(b, o=0):
    v = u24(b, o)
    return v - (1 << 24) if v & 0x800000 else v


def edit_line(e, mapfile):
    sym = lambda n: map_symbol(n, mapfile=mapfile)
    el = sym('__ZL9Edit_Line') or sym('_Edit_Line')  # a static: C++-mangled in the map
    cur, sl, ln = sym('_Cursor'), sym('_Start_Line'), sym('_Line')
    if None in (el, cur, sl, ln):
        return None, None
    p = u24(e.peek(el, 3))
    raw = e.peek(p, 200)
    text = raw.split(b'\0')[0].decode('latin-1')
    c = e.peek(cur, 6)
    cx, cy = s24(c, 0), s24(c, 3)
    start = s24(e.peek(sl, 3))
    lines = u24(e.peek(ln, 3))
    rec = e.peek(lines + 13 * (start + cy), 13)  # struct line: str, readonly, type, start_col, disp_len
    if u24(rec, 0) != p:
        return text, None                         # the cursor is not on the edit line
    return text, s24(rec, 7) + cx


def main():
    state, casefile = sys.argv[1], sys.argv[2]
    out = sys.argv[3] if len(sys.argv) > 3 else '/mnt/c/Users/tjb43/Downloads/Projects/khicas/emu-shots/edit2d'
    os.makedirs(out, exist_ok=True)
    mapfile = f'{KB}/emu/states/{state}.map'
    gap = int(os.environ.get('GAP', '250'))
    fails = 0
    for line in open(casefile, encoding='utf-8'):
        line = line.rstrip('\n')
        if not line.strip() or line.startswith('#'):
            continue
        parts = line.split('\t')
        name, keys, want = parts[0], parts[1], parts[2] if len(parts) > 2 else None
        e = Emu(image=f'{KB}/emu/states/{state}.ce', shotdir=out)
        try:
            e.run(800)
            for k in keyseq(keys):
                e.cmd(f'key {k} 80'); e.ms += 80
                e.run(gap)
            e.run(400)
            text, caret = edit_line(e, mapfile)
            shown = text if caret is None else text[:caret] + '|' + text[caret:]
            ok = '' if want is None else ('PASS' if shown == want else 'FAIL')
            fails += ok == 'FAIL'
            e.shot(name)
            print(f'{ok:4} {name:18} {shown}' + (f'   (want {want})' if ok == 'FAIL' else ''), flush=True)
        finally:
            e.close()
    print('failures:', fails)
    sys.exit(1 if fails else 0)


if __name__ == '__main__':
    main()
