#!/usr/bin/env python3
"""Run a list of console test cases against a saved KhiCAS state, one fresh state per case.

Each case: type the input, press EXE, wait until the screen settles (= evaluation time), screenshot.
If the modal 2D result viewer opened, screenshot it, dismiss it (EXE) and screenshot the console too.
Prints a summary table (case, eval ms, viewer?) for the notes.

Usage (WSL): scenario.py <state> <outdir> <casefile>     casefile lines: name<TAB>input  (# = comment)
"""
import os, struct, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cemu import Emu, KB


def bottom_bar(e):
    """Bytes of the F-key menu bar (bottom 18 rows): tells console vs 2D viewer apart."""
    bmp = f'/tmp/cemu_bar_{os.getpid()}.bmp'
    e.cmd(f'screenshot {bmp}')
    d = open(bmp, 'rb').read()
    off, = struct.unpack_from('<I', d, 10)
    return d[off: off + 18 * 320 * 3]          # BMP is bottom-up: the first rows are the bottom of the screen


def main(state, outdir, casefile):
    os.makedirs(outdir, exist_ok=True)
    cases = [l.rstrip('\n').split('\t', 1) for l in open(casefile, encoding='utf-8')
             if l.strip() and not l.startswith('#')]
    e0 = Emu(image=f'{KB}/emu/states/{state}.ce', shotdir=outdir)
    console_bar = bottom_bar(e0); e0.close()
    rows = []
    for i, (name, text) in enumerate(cases, 1):
        e = Emu(image=f'{KB}/emu/states/{state}.ce', shotdir=outdir)
        try:
            lat = e.type(text)
            e.cmd('key enter 80'); e.ms += 80
            # While KhiCAS computes, the F-key bar is blank ("cancel: stop calcul." in the status bar).
            # Done = bar visible again (console bar, or the 2D result viewer's bar). Resolution 50 ms.
            t = 0
            while t < 600000:
                bar = bottom_bar(e)
                if bar.count(255) != len(bar):
                    break
                e.run(50); t += 50
            e.wait_stable(step=50, stable=4, timeout=5000)
            viewer = bottom_bar(e) != console_bar
            e.shot(f'{i:02d}_{name}' + ('_viewer' if viewer else ''))
            if viewer:
                e.key('clear', settle=True)        # EXIT: EXE would paste the result into the input line
                e.shot(f'{i:02d}_{name}')
            rows.append((name, text, t, viewer, max(lat) if lat else 0))
        finally:
            e.close()
    print(f'{"case":24s} {"eval ms":>8s} {"viewer":>6s} {"max key ms":>10s}  input')
    for name, text, t, viewer, kl in rows:
        print(f'{name:24s} {t:8d} {str(viewer):>6s} {kl:10d}  {text}')


if __name__ == '__main__':
    main(*sys.argv[1:4])
