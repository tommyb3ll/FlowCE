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
    'graph_sin_trace': [('k', 'sin(x{graph}1'), ('w', 6000), ('k', '{yequ}'), ('w', 1000), ('k', '{right}{right}{right}{right}{right}{right}{right}{right}')],
    'forms':          [('p', '(x+1)^3'), ('k', '{trace}')],
    'night':          [('night',), ('p', 'integrate(1/(x^2+4),x,0,2)')],
    'night_graph':    [('night',), ('k', 'sin(x)*e^(-x/5){graph}1'), ('w', 7000)],
    'night_menu':     [('night',), ('p', 'diff(sin(x)^2,x)'), ('k', '{window}')],
    'basel':          [('p', 'sum(1/n^2,n,1,inf)')],
    'sec':            [('p', 'integrate(sec(x)^3,x)')],
    'arclength':      [('p', 'integrate(sqrt(1+(3/2*sqrt(x))^2),x,0,4)')],
    'ivp':            [('p', "desolve([y''+4y=0,y(0)=1,y'(0)=0],y)")],
    'table':          [('k', 'x{sq}-2{graph}1'), ('w', 6000), ('k', '{trace}'), ('w', 3000)],
    'root':           [('k', 'x{sq}-2{graph}1'), ('w', 6000), ('k', '{window}{enter}{down}{down}{down}{enter}'), ('w', 4000)],
    'trigsub':        [('p', 'integrate(1/(x^2*sqrt(4-x^2)),x)')],
    'power_series':   [('p', 'sum(x^n/n,n,1,inf)')],
    'dne':            [('p', 'limit(abs(x)/x,x,0)')],
    'cbrt':           [('p', 'integrate(x^(-1/3),x,-1,8)')],
    'riemann':        [('p', 'limit(sum((1+2i/n)^2*(2/n),i,1,n),n,inf)')],
    'ans':            [('p', '2+3'), ('k', '*2{enter}')],
    'cbrt_graph':     [('k', 'x^(1/3){graph}1'), ('w', 6000)],
    'polar':          [('p', '1/2*integrate((1+cos(t))^2,t,0,2pi)')],
    # the steps build (en-steps): enter on the empty line after a calculus answer opens its steps
    'steps_parts':    [('p', 'integrate(x*sin(x),x)'), ('k', '{enter}'), ('w', 8000)],
    'steps_limit':    [('p', 'limit((1-cos(x))/x^2,x,0)'), ('k', '{enter}'), ('w', 8000)],
    'steps_deriv':    [('p', 'diff(x^2*sin(x),x)'), ('k', '{enter}'), ('w', 6000)],
    'steps_sum':      [('p', 'sum(1/(n*(n+1)),n,1,inf)'), ('k', '{enter}'), ('w', 12000)],
    'steps_taylor':   [('p', 'taylor(e^x*sin(x),x=0,3)'), ('k', '{enter}'), ('w', 12000)],
    'steps_hint':     [('p', 'integrate(x*e^x,x)')],
    # the steps build's applications: the calculus card (item 10: the 0 key), area typed in its labeled boxes
    'menu_calc3':     [('k', '{window}')],
    'menu_apps':      [('k', '{window}0'), ('w', 800)],
    'apps_form':      [('k', '{window}0'), ('w', 1500), ('k', '1'), ('w', 900), ('k', 'x+2{enter}x{sq}{enter}'), ('w', 600)],
    'apps_area_tpl':  [('k', '{window}01x{sq}{right}x+2'), ('w', 800)],
    'apps_area':      [('k', '{window}01x{sq}{right}x+2{enter}'), ('w', 3000)],
    'apps_area_steps': [('k', '{window}01x{sq}{right}x+2{enter}'), ('w', 3000), ('k', '{enter}'), ('w', 8000)],
    'apps_washer_steps': [('p', 'volume(x,x^2,0,1,y=-1)'), ('k', '{enter}'), ('w', 8000)],
    'apps_shell_steps': [('p', 'volume(x-x^2,0,1,x=2)'), ('k', '{enter}'), ('w', 8000)],
    'apps_arc_steps': [('p', 'arclength(x^3/6+1/(2x),1,2)'), ('k', '{enter}'), ('w', 10000)],
    'wsB_steps':      [('p', 'integrate(8x*(2x+1)^2026,x)'), ('k', '{enter}'), ('w', 3000)],
    'steps_parts_end': [('p', 'integrate(x*sin(x),x)'), ('k', '{enter}'), ('w', 8000), ('k', '{down}{down}{down}'), ('w', 1500)],
    'steps_subst':    [('p', 'integrate(2x*cos(x^2),x)'), ('k', '{enter}'), ('w', 8000)],
    # DeltaCE: the action row, algebra steps, the analysis, units, the graph screen's kinds
    'action_row':     [('p', 'expand((x+1)*(x-2)*(x+3))'), ('k', '{right}')],
    'steps_quadratic': [('p', 'solve(2x^2-3x-5=0,x)'), ('k', '{enter}'), ('w', 6000)],
    'steps_factor':   [('p', 'factor(x^3+2x^2-5x-6)'), ('k', '{enter}'), ('w', 6000)],
    'analyze':        [('p', 'analyze((x^2-1)/(x-2))'), ('w', 8000)],
    'units':          [('p', '60_mph=>_km/_h')],
    'graph_points':   [('p', 'plot(x^3-3x)'), ('w', 5000), ('k', '{zoom}'), ('w', 1500), ('k', '4'), ('w', 5000)],
    'graph_conic':    [('p', 'plot([x^2/9+y^2/4=1,y=x-1])'), ('w', 6000)],
    'graph_param':    [('p', 'plotparam([cos(3t),sin(2t)],t)'), ('w', 5000)],
    'graph_polar':    [('p', 'plotpolar(1+2cos(theta),theta)'), ('w', 5000)],
    'graph_field':    [('p', 'plotode(x-y,[0,1])'), ('w', 6000)],
    # DeltaCE 2.0: probability (more menu, 6), practice (7), the new steps
    'prob_normal':    [('k', '{graph}6'), ('w', 2500), ('k', '100{enter}15{enter}{window}{window}130{enter}'), ('w', 3000)],
    'prob_ttest':     [('k', '{graph}6'), ('w', 2500), ('k', '{trace}'), ('w', 1000), ('k', '4'), ('w', 4000)],
    'practice':       [('k', '{graph}7'), ('w', 1500), ('k', '5'), ('w', 3000), ('k', '{enter}'), ('w', 800), ('k', '9x^2+2x-5{enter}'), ('w', 3000)],  # (the same problem each run: the emulator is deterministic)
    'steps_system':   [('p', 'solve([2x+3y=12,x-y=1],[x,y])'), ('k', '{enter}'), ('w', 6000)],
    'steps_log':      [('p', 'solve(log(x)+log(x-3)=1,x)'), ('k', '{enter}'), ('w', 8000)],
    'steps_trig':     [('p', 'solve(2cos(x)+sqrt(3)=0,x)'), ('k', '{enter}'), ('w', 8000), ('k', '{down}{down}{down}'), ('w', 1500)],
    'steps_synth':    [('p', '(x^3-8)/(x-2)'), ('k', '{enter}'), ('w', 5000), ('k', '{trace}'), ('w', 5000), ('k', '{right}{right}{right}{down}'), ('w', 1500)],
    'steps_ineq':     [('p', 'x^2-x-6>0'), ('k', '{enter}'), ('w', 6000), ('k', '{right}{right}{right}'), ('w', 1500)],
    'steps_abs':      [('p', 'abs(3-2x)<=5'), ('k', '{enter}'), ('w', 4000)],
    # Statistics (more menu, 5): hours studied and test scores in L1 and L2
    'stats_data':     [('p', 'L1:=[1,2,3,4,5,6,7,8]'), ('p', 'L2:=[52,55,61,64,70,74,79,85]'), ('k', '{graph}5')],
    'stats_box':      [('p', 'L1:=[1,2,3,4,5,6,7,8]'), ('p', 'L2:=[52,55,61,64,70,74,79,85]'), ('k', '{graph}5{window}')],
    'stats_fit':      [('p', 'L1:=[1,2,3,4,5,6,7,8]'), ('p', 'L2:=[52,55,61,64,70,74,79,85]'), ('k', '{graph}5{zoom}1')],
    'stats_graph':    [('p', 'L1:=[1,2,3,4,5,6,7,8]'), ('p', 'L2:=[52,55,61,64,70,74,79,85]'), ('k', '{graph}5{zoom}1{yequ}'), ('w', 3000)],
}


def run_scene(state, outdir, name, steps):
    e = Emu(image=f'{KB}/emu/states/{state}.ce', shotdir=outdir)
    try:
        e.run(800)
        e.wait_idle(timeout=5000)
        for st in steps:
            if st[0] == 'night':  # more menu, item 3: Themes, then 2: Night
                e.type('{graph}32')
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
