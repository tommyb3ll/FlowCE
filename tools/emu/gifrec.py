#!/usr/bin/env python3
"""README GIFs: a short session replayed on an emulator state, one frame per key, saved as an
animated GIF (2x, lossless: the interface uses fewer than 256 colors).

usage (WSL): gifrec.py <state> <outdir> [movie ...]     (no names: all of them)

A movie is a list of steps:
  ('intro', ms)      quit to the OS, start FlowCE again from APPS: the start animation, a frame
                     every 100 ms for ms
  ('night',)         the Night theme (more menu, item 3: Themes, then 2), not recorded
  ('theme', d)       theme d (1-9) the same way, not recorded (from the console)
  ('k', keys, hold[, ms])  type keys one at a time ({name} groups are one key), a frame after
                     each (ms, default KEY_MS); the last frame stays hold ms
  ('p', text, hold)  paste text and EXE (fast: for long inputs), one frame held hold ms
  ('w', ms)          let the calculator run, no frame
  ('f', hold)        one frame, held hold ms
"""
import os
import re
import sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cemu import Emu, KB
from PIL import Image

KEY_MS = 170  # a typed key on screen

INTEGRAL = [('k', '{window}', 450), ('k', '2', 350), ('k', 'x{sq}+1', 300), ('k', '{down}0{up}{up}3', 600),
            ('k', '{enter}', 1800)]
SERIES = [('p', 'sum(1/n^2,n,1,inf)', 1800)]
FORMS = [('p', '(x+1)^3', 1300), ('k', '{trace}', 1700), ('w', 2500)]
GRAPH = [('k', 'sin(x)*e^(-x/5)', 250), ('k', '{graph}1', 0), ('w', 6000), ('f', 900),
         ('k', '{yequ}', 500), ('k', '{right}' * 20, 1600, 90)]  # (2.0's graph screen: F1 traces)

# the steps build (en-steps): an antiderivative from the calculus menu, enter, then enter again
# for its steps, read down to the answer
STEPS = [('k', '{window}', 450), ('k', '1', 350), ('k', 'x{sin}x', 300), ('k', '{enter}', 1900),
         ('k', '{enter}', 1700), ('k', '{down}' * 3, 2600, 1000)]

# FlowCE 2.0: the themes (more > Themes: the grid, each move showing the theme everywhere, over
# an answer), an application's form (area between curves, its answer, its steps), the graph
# screen (pan, zoom in, key points, trace)
# the themes: the menu once (Night picked), then FlowCE in use, each cut in the next theme (the
# switches not shown), the grid at the end flashing back to Paper
THEMES = [('p', 'integrate(x^2*sin(x),x)', 1000), ('k', '{graph}3', 900), ('k', '{right}', 700),
          ('k', '{enter}', 900), ('k', '{enter}', 1100), ('k', '{down}{down}', 800, 450), ('k', '{clear}', 0),
          ('theme', 3), ('k', '{window}', 650), ('k', '0', 650), ('k', '1', 450),
          ('k', 'x+2{enter}x{sq}', 120), ('k', '{enter}{enter}{enter}', 1000, 150),
          ('theme', 4), ('f', 700), ('k', '{enter}', 1000), ('k', '{clear}', 0),
          ('theme', 5), ('k', 'x^3{right}-3x', 100), ('k', '{graph}1', 0), ('w', 7000), ('f', 900),
          ('k', '{zoom}', 900), ('k', '{clear}', 0), ('k', '{clear}', 0),
          ('theme', 6), ('p', 'solve(x^2-5x+6=0,x)', 800), ('k', '{enter}', 1000), ('k', '{clear}', 0),
          ('theme', 7), ('k', '{yequ}', 800), ('k', '{clear}', 0),
          ('theme', 8), ('p', 'sum(1/n^2,n,1,inf)', 900),
          ('theme', 9), ('k', '{window}', 900), ('k', '{clear}', 0),
          ('k', '{graph}3', 700), ('k', '{up}{up}{left}{left}', 500, 260), ('k', '{enter}', 900)]
APPS = [('k', '{window}', 700), ('k', '0', 1000), ('k', '1', 900), ('k', 'x+2{enter}x{sq}', 260),
        ('k', '{enter}{enter}', 900, 400), ('k', '{enter}', 1900), ('k', '{enter}', 1600),
        ('k', '{down}{down}', 2200, 900)]
GRAPHNEW = [('k', 'x^3{right}-3x', 200), ('k', '{graph}1', 0), ('w', 7000), ('f', 1000),
            ('k', '{left}{left}{left}{up}{up}', 600, 160), ('k', '+', 900), ('k', '{zoom}', 1300),
            ('k', '{down}{down}{down}{enter}', 1200, 300), ('k', '{yequ}', 400),
            ('k', '{right}' * 12, 1500, 110)]

MOVIES = {
    'flowce_light': [('intro', 1400)] + INTEGRAL + SERIES + FORMS + GRAPH,
    'flowce_dark': [('night',), ('intro', 1400)] + INTEGRAL + SERIES + FORMS + GRAPH,
    'flowce_steps': STEPS,
    'flowce_themes': THEMES,
    'flowce_apps': APPS,
    'flowce_graph': GRAPHNEW,
}


def keys_of(text):
    return re.findall(r'\{[^}]*\}|.', text)


def record(state, outdir, name, steps):
    tmp = os.path.join(outdir, name + '_frames')
    os.makedirs(tmp, exist_ok=True)
    frames = []  # (png path, ms)
    e = Emu(image=f'{KB}/emu/states/{state}.ce', shotdir=tmp)

    def frame(ms):
        frames.append((e.shot(f'{len(frames):03d}'), ms))

    try:
        e.run(800)
        e.wait_idle(timeout=5000)
        for st in steps:
            if st[0] == 'theme':  # more > Themes, theme st[1] (1-9), not recorded
                e.type('{graph}3%d' % st[1])
                e.wait_idle(timeout=20000)
                e.run(800)
            elif st[0] == 'night':  # (more > Themes, 2: Night)
                e.type('{graph}32')
                e.wait_idle(timeout=20000)
                e.run(1500)
            elif st[0] == 'intro':
                e.type('{2nd,mode}')
                e.run(6000)  # (quitting saves the session: the TI home screen after it)
                e.type('{enter}')  # (after an install the OS shows its tips box first: enter, never again)
                e.run(1500)
                e.type('{apps}')
                e.run(1500)
                e.cmd('key 2 80'); e.ms += 80
                for _ in range(st[1] // 100):
                    e.run(100)
                    frame(100)
                e.wait_idle(timeout=20000)
                e.run(600)
                frame(700)
            elif st[0] == 'k':
                ks = keys_of(st[1])
                for i, k in enumerate(ks):
                    e.type(k)
                    e.wait_idle(timeout=120000)
                    e.run(150)
                    if st[2] or i < len(ks) - 1:
                        frame(st[2] if i == len(ks) - 1 else (st[3] if len(st) > 3 else KEY_MS))
            elif st[0] == 'p':
                e.paste(st[1])
                e.key('enter')
                e.wait_idle(timeout=120000)
                e.run(400)
                frame(st[2])
            elif st[0] == 'w':
                e.run(st[1])
            elif st[0] == 'f':
                frame(st[1])
    finally:
        e.close()
    imgs = [Image.open(p).convert('RGB') for p, _ in frames]
    pal = [im.quantize(colors=256, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.NONE) for im in imgs]
    out = os.path.join(outdir, name + '.gif')
    pal[0].save(out, save_all=True, append_images=pal[1:], duration=[ms for _, ms in frames], loop=0,
                optimize=True, disposal=1)
    print(name, out, len(frames), 'frames', os.path.getsize(out), 'B', flush=True)


def main():
    state, outdir = sys.argv[1], sys.argv[2]
    os.makedirs(outdir, exist_ok=True)
    for n in sys.argv[3:] or list(MOVIES):
        record(state, outdir, n, MOVIES[n])


if __name__ == '__main__':
    main()
