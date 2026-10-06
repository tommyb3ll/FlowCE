"""Consistency check of the command search's partial repaints (src/focus_catalog.cc): random keys
pressed fast (repaints cut short by the next key, one-row scrolls done by moving pixels), then
the search's state read from memory (cat_state through its static pointer S) is rebuilt slowly
from a fresh open, and both screens must be identical.

  wsl python3 tools/emu/catalog_fuzz.py <state with the search open> <seeds, e.g. 1,2,3> [keys=40] [moves]

moves: arrows and tabs only (long lists: moves, pages, scrolls); else also letters and DEL (DEL on
an empty query closes the search: the following keys go to the console, and a DOWN there reopens
it on the word typed). Note: in the emulator, getkey's 10 ms wait sometimes returns at once ~100
times in a row; an arrow or DEL held meanwhile is then repeated by the OS. The rebuild converges
step by step, so such repeats do not matter.
"""
import sys, os, random, hashlib
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cemu import Emu, KB, map_symbol

state, seeds = sys.argv[1], [int(x) for x in sys.argv[2].split(',')]
nkeys = int(sys.argv[3]) if len(sys.argv) > 3 else 40
moves_only = len(sys.argv) > 4 and sys.argv[4] == 'moves'
SHOTS = '/mnt/c/Users/tjb43/Downloads/Projects/khicas/emu-shots/catalog/fuzz'
os.makedirs(SHOTS, exist_ok=True)
PH, SP = map_symbol('_focus_phase'), map_symbol('__ZL1S')   # the last build's map
LET = {'sin': 'e', 'cos': 'f', 'tan': 'g', '^': 'h', 'sq': 'i', 'log': 'n', 'ln': 's', '4': 't',
       '5': 'u', 'sto': 'x', '8': 'p', '*': 'r', 'apps': 'b', '/': 'm', ')': 'l', '7': 'o'}
KEY_OF = {v: k for k, v in LET.items()}
TABS = ['yequ', 'window', 'zoom', 'trace', 'graph']


def u24(b, o=0):
    return b[o] | b[o + 1] << 8 | b[o + 2] << 16


def idle(e, ms=60):  # the search waits in getkey (focus_phase 0) for ms in a row
    q = 0
    for _ in range(5000):
        e.run(5)
        q = q + 5 if e.peek(PH, 1)[0] == 0 else 0
        if q >= ms:
            return


def st(e):  # cat_state: C idx cls nc ni tab sel top ql exlv q[16] (eZ80: 3-byte ints and pointers)
    b = e.peek(u24(e.peek(SP, 3)), 46)
    return dict(ni=u24(b, 12), tab=u24(b, 15), sel=u24(b, 18), top=u24(b, 21), q=b[30:30 + u24(b, 24)].decode())


def slow(e, k):  # a key, the search idle again, then up long enough for the OS to see a repeat
    e.cmd(f'key {k} 80'); idle(e); e.run(100)


def screen(e, name):
    return hashlib.md5(open(e.shot(name), 'rb').read()).hexdigest()


bad = 0
for seed in seeds:
    rnd = random.Random(seed)
    keys = []
    for _ in range(nkeys):
        r = rnd.random() + (0.24 if moves_only else 0)
        keys.append(rnd.choice(['sin', 'sq', 'log', 'ln', '4', '*']) if r < 0.12 else 'del' if r < 0.24 else
                    rnd.choice(TABS) if r < 0.32 else rnd.choice(['up', 'down', 'down', 'left', 'right']))
    e = Emu(image=f'{KB}/emu/states/{state}.ce', shotdir=SHOTS)
    try:
        idle(e)
        for k in keys:  # 50 ms down, 20-150 ms up: often during a repaint
            e.cmd(f'key {k} 50'); e.run(rnd.choice([20, 40, 80, 150]))
        idle(e, 150)
        closed = not u24(e.peek(SP, 3))
        if not closed:
            s, a = st(e), screen(e, f'{seed}_a')
    finally:
        e.close()
    if closed:
        print(f'seed {seed}: the search ended closed, skipped', flush=True)
        continue
    print(f'seed {seed}: {" ".join(keys)}\n  -> {s}', flush=True)
    e = Emu(image=f'{KB}/emu/states/{state}.ce', shotdir=SHOTS)
    try:
        idle(e)
        if s['tab']:
            slow(e, TABS[s['tab']])
        for c in s['q']:  # letters typed directly; digits and _ after 2nd
            if c.isdigit() or c == '_':
                slow(e, '2nd'); slow(e, '(-)' if c == '_' else c)
            else:
                slow(e, KEY_OF[c])
        for _ in range(500):  # converge on (sel, top) one key at a time
            c = st(e)
            if (c['sel'], c['top']) == (s['sel'], s['top']):
                break
            far = s['top'] - c['top']
            slow(e, 'right' if far > 7 else 'left' if far < -7 else
                 'down' if far > 0 or (far == 0 and c['sel'] < s['sel']) else 'up')
        idle(e, 150)
        s2, b = st(e), screen(e, f'{seed}_b')
    finally:
        e.close()
    bad += a != b or s != s2
    print(f'  rebuilt {s2}  screens {"same" if a == b else "DIFFER"}', flush=True)
print('all same' if not bad else f'{bad} differ')
