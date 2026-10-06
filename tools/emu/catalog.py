"""The Focus command search (src/focus_catalog.cc) in the emulator: opens it the three ways
(F5 -> All commands, a word then DOWN, 2nd CATALOG), types a query, moves, switches category,
inserts and evaluates; screenshots each step and prints two times per key, from the press:
  ready  KhiCAS polls the keypad again (getkey, or the search's check between rows): a key
         pressed from then on is not lost (keytime.py's busy time, measured to the ms)
  done   the screen is complete (focus_phase back to 0 in getkey)
Then types a word with keys 70 ms apart, faster than the list repaints, and checks no key is lost.

  wsl python3 tools/emu/catalog.py <state> <shotdir>
"""
import sys, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cemu import Emu, KB, map_symbol

state, shots = sys.argv[1], sys.argv[2]
os.makedirs(shots, exist_ok=True)
e = Emu(image=f'{KB}/emu/states/{state}.ce', shotdir=shots)
PH = map_symbol('_focus_phase')   # the last build's map (the build installed in the state)
times = []


def press(k, label=None, hold=80, gap=120):
    """Press k for hold ms, sampling every ms: the work starts when focus_phase leaves 0; ready
    is the first keypad poll after that, done the first poll back in getkey (phase 0). The key
    then stays up gap ms: the OS only sees a second press of the same key after ~60 ms up."""
    e.cmd(f'keydown {k}')
    last, t, t0, ready, done = e.polls(), 0, None, None, None
    while t < 5000:
        if t == hold:
            e.cmd(f'keyup {k}')
        e.run(1); t += 1
        p, ph = e.polls(), e.peek(PH, 1)[0]
        if t0 is None and ph:
            t0 = t
        if p != last:
            last = p
            if t0 is not None and ready is None:
                ready = t
            if t0 is not None and done is None and not ph:
                done = t
        end = done if done is not None else (400 if t0 is None else None)
        if end is not None and t >= max(hold, end) + gap:
            break
    e.ms += t
    times.append((label or k, ready, done))
    print(f'{label or k:26s} ready {ready!s:>4} ms  done {done!s:>4} ms', flush=True)


def shot(name):
    print('shot', e.shot(name), flush=True)


try:
    if e.polls() is None or PH is None:
        sys.exit('no _getkey_polls / _focus_phase in the map: rebuild')
    e.wait_idle(timeout=5000); e.run(100)
    # 1. F5 -> All commands: the search with an empty query
    press('graph', 'F5 (popover)')
    press('enter', 'All commands (open)')
    shot('01_open_empty')
    # 2. the query "int": letters typed directly (alpha is locked)
    press('sq', 'letter i'); press('log', 'letter n'); press('4', 'letter t')
    shot('02_query_int')
    # 3. the selection moves
    for k in range(3):
        press('down', f'down {k + 1}')
    shot('03_down3')
    # 4. the calculus tab (F3)
    press('zoom', 'F3 calculus')
    shot('04_calculus')
    # 5. ENTER inserts the selected command into the edit line
    press('enter', 'enter (insert)')
    shot('05_inserted')
    # 6. its arguments, then evaluate
    e.type('x^2,x')
    press('enter', 'enter (evaluate)')
    e.wait_stable(step=50, stable=4, timeout=5000)
    shot('06_evaluated')
    # 7. a word in the console, then DOWN: the search opens on it
    e.type('lim')
    shot('07a_console_lim')
    press('down', 'down (opens on "lim")')
    shot('07_lim_down')
    # 8. CLEAR: back to the console, "lim" still in the edit line
    press('clear', 'clear (close)')
    shot('08_clear')
    # 9. 2nd CATALOG with an empty query; ALPHA before a letter (habit) still types it; 2nd + a
    #    digit key types the digit; DEL; paging and scrolling; another tab; CLEAR
    press('clear', 'clear (empty the line)')
    press('2nd'); press('0', 'catalog key (open)')
    shot('09_catalog_key')
    press('alpha', 'alpha (no-op)'); press('sin', 'letter e (after alpha)')
    press('sto', 'letter x'); press('8', 'letter p'); press('2nd', '2nd'); press('2', 'digit 2')
    shot('10_query_exp2')
    for k in range(4):
        press('del', f'del {k + 1}')
    shot('11_deleted')
    press('right', 'right (page)'); press('right', 'right (page)')
    for k in range(3):
        press('down', f'down (scrolls) {k + 1}')
    shot('12_scrolled')
    press('left', 'left (page)')
    press('window', 'F2 algebra')
    shot('13_algebra')
    # 10. fast typing: "simp" with keys 70 ms apart (50 ms down, 20 ms up), then wait
    for k in ['ln', 'sq', '/', '8']:
        e.cmd(f'key {k} 50'); e.run(20)
    e.wait_stable(step=20, stable=10, timeout=5000)
    shot('14_fast_simp')
    press('clear', 'clear (close)')
    shot('15_closed')
    r = [x[1] for x in times if x[1]]
    print('ready: max', max(r), 'ms; done:', ', '.join(f'{l}={d}' for l, _, d in times))
finally:
    e.close()
