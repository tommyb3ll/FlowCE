#!/usr/bin/env python3
"""Showcase screenshots for the README: each shot replays a short scene on a fresh copy of an
emulator state and saves a 2x PNG (and an animated GIF of typing an integral).

Usage (WSL): showcase.py <state> <outdir> [shot ...]     (no shot names: all of them)
Scenes: text is pasted as written (Emu.paste) unless it is a key sequence {..}: then it is
typed (Emu.type keys, through the 2D editing rules), like a user.
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cemu import Emu, KB

# name: list of steps. ('p', text) paste + EXE; ('k', keys) type keys; ('w', ms) run; ('night',)
SCENES = {
    'hero_integral':  [('p', 'integrate(x^2*sin(x),x)')],
    'definite':       [('p', 'integrate(sin(x)^2,x,0,pi)')],
    'solve':          [('p', 'solve(x^2-5x+6=0,x)')],
    'limit':          [('p', 'limit(sin(3x)/x,x,0)')],
    'derivative':     [('p', 'diff(x^2*e^x,x)')],
    'taylor_input':   [('k', '{window}6sin(x')],
    'taylor':         [('k', '{window}6sin(x{enter}'), ('w', 5000)],
    'typing_fraction': [('k', '{math}1x{sq}+1{down}x-1')],
    'typing_integral': [('k', '{window}2x{sq}+1{down}0{up}{up}3')],
    'history':        [('p', 'factor(x^3-x)'), ('p', 'integrate(1/(1+x^2),x)'), ('p', 'limit((1+1/n)^n,n,inf)'), ('k', '{up}{up}')],
    'menu_calculus':  [('k', '{window}')],
    'menu_symbols':   [('k', '{trace}')],
    'menu_2nd':       [('k', '{2nd,yequ}{right}{right}')],
    'search':         [('k', '{graph}2'), ('w', 1500), ('k', 'int')],
    'graph':          [('k', 'x{sq}-2{graph}1'), ('w', 6000)],
    'graph_sin_trace': [('k', 'sin(x{graph}1'), ('w', 6000), ('k', '{right}{right}{right}{right}{right}{right}{right}{right}')],
    'forms':          [('p', '(x+1)^3'), ('k', '{trace}')],
    'night':          [('night',), ('p', 'integrate(1/(x^2+4),x,0,2)')],
    'night_graph':    [('night',), ('k', 'sin(x)*e^(-x/5){graph}1'), ('w', 7000)],
    'night_menu':     [('night',), ('p', 'diff(sin(x)^2,x)'), ('k', '{window}')],
    'basel':          [('p', 'sum(1/n^2,n,1,inf)')],
    'sec':            [('p', 'integrate(sec(x)^3,x)')],
    'arclength':      [('p', 'integrate(sqrt(1+(3/2*sqrt(x))^2),x,0,4)')],
    'ivp':            [('p', "desolve([y''+4y=0,y(0)=1,y'(0)=0],y)")],
    'table':          [('k', 'x{sq}-2{graph}1'), ('w', 6000), ('k', '{2nd,graph}'), ('w', 2000)],
    'root':           [('k', 'x{sq}-2{graph}1'), ('w', 6000), ('k', '{window}{enter}{down}{down}{down}{enter}'), ('w', 4000)],
    'trigsub':        [('p', 'integrate(1/(x^2*sqrt(4-x^2)),x)')],
}


def run_scene(state, outdir, name, steps):
    e = Emu(image=f'{KB}/emu/states/{state}.ce', shotdir=outdir)
    try:
        e.run(800)
        e.wait_idle(timeout=5000)
        for st in steps:
            if st[0] == 'night':  # more menu, item 3: Paper or Night
                e.type('{graph}3')
                e.run(1500)
            elif st[0] == 'p':
                e.paste(st[1])
                e.key('enter')
                e.wait_idle(timeout=120000)
                e.run(400)
            elif st[0] == 'k':
                e.type(st[1])
                e.wait_idle(timeout=120000)
                e.run(300)
            elif st[0] == 'w':
                e.run(st[1])
        e.run(500)
        print(name, e.shot(name), flush=True)
    finally:
        e.close()


def main():
    state, outdir = sys.argv[1], sys.argv[2]
    os.makedirs(outdir, exist_ok=True)
    names = sys.argv[3:] or list(SCENES)
    for n in names:
        run_scene(state, outdir, n, SCENES[n])


if __name__ == '__main__':
    main()
